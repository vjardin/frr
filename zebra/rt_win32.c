// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows kernel interface for zebra, on top of the IP Helper API: routes.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#ifdef _WIN32

#include "lib/prefix.h"
#include "lib/nexthop.h"
#include "lib/nexthop_group.h"
#include "lib/vrf.h"
#include "lib/memory.h"
#include "lib/log.h"
#include "lib/lib_errors.h"

#include "zebra/kernel_win32.h"
#include "zebra/rib.h"
#include "zebra/rt.h"
#include "zebra/zebra_ns.h"
#include "zebra/zebra_dplane.h"
#include "zebra/zebra_pbr.h"
#include "zebra/zebra_tc.h"
#include "zebra/zebra_errors.h"
#include "zebra/debug.h"

DEFINE_MTYPE_STATIC(ZEBRA, WIN32_ROUTES, "Windows routes");

/* One next hop of a route of the system. */
struct win32_route {
	struct prefix p;
	uint32_t metric;
	union g_addr gate; /* unspecified when the prefix is on-link */
	ifindex_t ifindex;
};

/*
 * The routes of the system zebra has been told about, sorted: next hops of
 * the same prefix and metric are next to each other and make one (multipath)
 * route. Windows only tells that its table changed, so the table is read
 * again and compared with this.
 */
static struct win32_route *win32_routes;
static size_t win32_route_count;

static int win32_route_group_cmp(const struct win32_route *a, const struct win32_route *b)
{
	int ret;

	ret = prefix_cmp(&a->p, &b->p);
	if (ret)
		return ret;

	return numcmp(a->metric, b->metric);
}

static int win32_route_cmp(const void *va, const void *vb)
{
	const struct win32_route *a = va, *b = vb;
	int ret;

	ret = win32_route_group_cmp(a, b);
	if (ret)
		return ret;

	ret = memcmp(&a->gate, &b->gate, sizeof(a->gate));
	if (ret)
		return ret;

	return numcmp(a->ifindex, b->ifindex);
}

/* Number of next hops of the route starting at routes[at]. */
static size_t win32_route_group_len(const struct win32_route *routes, size_t count, size_t at)
{
	size_t len = 1;

	while (at + len < count && !win32_route_group_cmp(&routes[at], &routes[at + len]))
		len++;

	return len;
}

/*
 * Routes zebra wants from the system, as kernel routes. Not the ones
 * Windows derives from the interface addresses: the on-link prefix, the
 * host routes of the address and of the broadcast address, and the
 * multicast and link-local prefixes of every interface. zebra makes its
 * own connected and local routes out of the addresses.
 */
static bool win32_route_wanted(const MIB_IPFORWARD_ROW2 *row)
{
	return row->Protocol != MIB_IPPROTO_LOCAL && !row->Loopback;
}

/*
 * Windows adds the metric of the interface to the one of the route to rank
 * routes, that sum is the metric here, as "route print" shows it.
 */
static uint32_t win32_interface_metric(const MIB_IPINTERFACE_TABLE *interfaces,
				       const MIB_IPFORWARD_ROW2 *row)
{
	ULONG i;

	if (!interfaces)
		return 0;

	for (i = 0; i < interfaces->NumEntries; i++) {
		const MIB_IPINTERFACE_ROW *iface = &interfaces->Table[i];

		if (iface->InterfaceIndex == row->InterfaceIndex &&
		    iface->Family == row->DestinationPrefix.Prefix.si_family)
			return iface->Metric;
	}

	return 0;
}

static bool win32_route_convert(const MIB_IPFORWARD_ROW2 *row, uint32_t interface_metric,
				struct win32_route *route)
{
	const SOCKADDR_INET *dest = &row->DestinationPrefix.Prefix;

	/* padding included: these get compared */
	memset(route, 0, sizeof(*route));

	switch (dest->si_family) {
	case AF_INET:
		route->p.family = AF_INET;
		route->p.u.prefix4 = dest->Ipv4.sin_addr;
		route->gate.ipv4 = row->NextHop.Ipv4.sin_addr;
		break;
	case AF_INET6:
		route->p.family = AF_INET6;
		route->p.u.prefix6 = dest->Ipv6.sin6_addr;
		route->gate.ipv6 = row->NextHop.Ipv6.sin6_addr;
		break;
	default:
		return false;
	}
	route->p.prefixlen = row->DestinationPrefix.PrefixLength;
	route->metric = row->Metric + interface_metric;
	route->ifindex = row->InterfaceIndex;

	return true;
}

static void win32_route_nexthop(const struct win32_route *route, struct nexthop *nexthop)
{
	nexthop->vrf_id = VRF_DEFAULT;
	nexthop->ifindex = route->ifindex;
	nexthop->type = NEXTHOP_TYPE_IFINDEX;

	if (route->p.family == AF_INET && route->gate.ipv4.s_addr != INADDR_ANY) {
		nexthop->type = NEXTHOP_TYPE_IPV4_IFINDEX;
		nexthop->gate.ipv4 = route->gate.ipv4;
	} else if (route->p.family == AF_INET6 && !IN6_IS_ADDR_UNSPECIFIED(&route->gate.ipv6)) {
		nexthop->type = NEXTHOP_TYPE_IPV6_IFINDEX;
		nexthop->gate.ipv6 = route->gate.ipv6;
	}
}

/* Give zebra a route of the system, with its "count" next hops. */
static void win32_route_add(const struct win32_route *route, size_t count, bool startup)
{
	afi_t afi = family2afi(route->p.family);
	struct nexthop_group ng = {};
	struct prefix p = route->p;
	struct route_entry *re;
	struct nexthop *nexthop;
	size_t i;

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("%s: %pFX metric %u, %zu nexthop(s)", __func__, &p, route->metric,
			   count);

	for (i = 0; i < count; i++) {
		nexthop = nexthop_new();
		win32_route_nexthop(&route[i], nexthop);
		nexthop_group_add_sorted(&ng, nexthop);
	}

	re = zebra_rib_route_entry_new(VRF_DEFAULT, ZEBRA_ROUTE_KERNEL, 0, 0, 0, rt_table_main_id,
				       route->metric, 0, 0, 0);

	/* takes a copy of the next hops; replaces the route if it is known */
	rib_add_multipath(afi, SAFI_UNICAST, &p, NULL, re, &ng, startup, false);

	nexthops_free(ng.nexthop);
}

static void win32_route_delete(const struct win32_route *route)
{
	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("%s: %pFX metric %u", __func__, &route->p, route->metric);

	rib_delete(family2afi(route->p.family), SAFI_UNICAST, VRF_DEFAULT, ZEBRA_ROUTE_KERNEL, 0,
		   0, &route->p, NULL, NULL, 0, rt_table_main_id, route->metric, 0, true);
}

static bool win32_route_group_same(const struct win32_route *a, size_t alen,
				   const struct win32_route *b, size_t blen)
{
	size_t i;

	if (alen != blen)
		return false;

	for (i = 0; i < alen; i++) {
		if (win32_route_cmp(&a[i], &b[i]))
			return false;
	}

	return true;
}

void win32_route_sync(bool startup)
{
	MIB_IPINTERFACE_TABLE *interfaces = NULL;
	MIB_IPFORWARD_TABLE2 *table = NULL;
	struct win32_route *routes = NULL;
	size_t count = 0;
	size_t o = 0, n = 0;
	DWORD rc;
	ULONG i;

	rc = GetIpForwardTable2(AF_UNSPEC, &table);
	if (rc != ERROR_SUCCESS) {
		flog_err(EC_LIB_SYSTEM_CALL, "Could not read routes: GetIpForwardTable2() error %lu",
			 rc);
		return;
	}

	/* for the interface metrics; routes keep their own metric without */
	if (GetIpInterfaceTable(AF_UNSPEC, &interfaces) != ERROR_SUCCESS)
		interfaces = NULL;

	if (table->NumEntries)
		routes = XCALLOC(MTYPE_WIN32_ROUTES, table->NumEntries * sizeof(*routes));

	for (i = 0; i < table->NumEntries; i++) {
		const MIB_IPFORWARD_ROW2 *row = &table->Table[i];

		if (!win32_route_wanted(row))
			continue;

		if (win32_route_convert(row, win32_interface_metric(interfaces, row),
					&routes[count]))
			count++;
	}

	FreeMibTable(table);
	if (interfaces)
		FreeMibTable(interfaces);

	if (count)
		qsort(routes, count, sizeof(*routes), win32_route_cmp);

	/* Both sorted the same way: walk them together, a route at a time. */
	while (o < win32_route_count || n < count) {
		size_t olen = 0, nlen = 0;
		int cmp;

		if (o < win32_route_count)
			olen = win32_route_group_len(win32_routes, win32_route_count, o);
		if (n < count)
			nlen = win32_route_group_len(routes, count, n);

		if (!nlen)
			cmp = -1;
		else if (!olen)
			cmp = 1;
		else
			cmp = win32_route_group_cmp(&win32_routes[o], &routes[n]);

		if (cmp < 0) {
			/* gone */
			win32_route_delete(&win32_routes[o]);
			o += olen;
		} else if (cmp > 0) {
			/* new */
			win32_route_add(&routes[n], nlen, startup);
			n += nlen;
		} else {
			/* known: did its next hops change? */
			if (!win32_route_group_same(&win32_routes[o], olen, &routes[n], nlen))
				win32_route_add(&routes[n], nlen, startup);
			o += olen;
			n += nlen;
		}
	}

	XFREE(MTYPE_WIN32_ROUTES, win32_routes);
	win32_routes = routes;
	win32_route_count = count;
}

void win32_route_fini(void)
{
	XFREE(MTYPE_WIN32_ROUTES, win32_routes);
	win32_route_count = 0;
}

void nexthop_read(struct zebra_ns *zns)
{
	zebra_dplane_startup_stage(zns->ns_id, ZEBRA_DPLANE_NEXTHOPS_READ);
}

void route_read(struct zebra_ns *zns)
{
	if (zns->ns_id != NS_DEFAULT)
		return;

	win32_route_sync(true);
}

void kernel_read_macfdb(struct zebra_dplane_ctx *ctx)
{
}

void kernel_read_neigh(struct zebra_dplane_ctx *ctx)
{
}

void kernel_read_pbr_rules(struct zebra_ns *zns)
{
}

void kernel_read_tc_qdisc(struct zebra_dplane_ctx *ctx)
{
	zebra_dplane_startup_stage(dplane_ctx_get_ns_id(ctx), ZEBRA_DPLANE_FINISHED_READING);
}

void vlan_read(struct zebra_ns *zns)
{
}

enum zebra_dplane_result kernel_route_update(struct zebra_dplane_ctx *ctx)
{
	static bool warned;

	/* nothing was installed, so there is nothing to remove either */
	if (dplane_ctx_get_op(ctx) == DPLANE_OP_ROUTE_DELETE)
		return ZEBRA_DPLANE_REQUEST_SUCCESS;

	if (RSYSTEM_ROUTE(dplane_ctx_get_type(ctx)))
		return ZEBRA_DPLANE_REQUEST_SUCCESS;

	if (!warned) {
		warned = true;
		flog_err(EC_LIB_UNAVAILABLE,
			 "Installing routes into the Windows routing table is not implemented: %pFX and the following ones stay out of it",
			 dplane_ctx_get_dest(ctx));
	}

	dplane_ctx_set_errno(ctx, ENOTSUP);
	return ZEBRA_DPLANE_REQUEST_FAILURE;
}

#endif /* _WIN32 */
