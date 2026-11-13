// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows kernel interface for zebra, on top of the IP Helper API:
 * change notifications and the dataplane entry points.
 *
 * Windows has neither netlink nor a routing socket. The IP Helper API
 * gives snapshots of the interface, address and route tables, and calls
 * back, on a thread of its own, when one of them changes. Those callbacks
 * only record which table changed and wake the main pthread up through a
 * pipe, the main pthread then reads the tables again and applies the
 * differences.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#ifdef _WIN32

#include "lib/frrevent.h"
#include "lib/frratomic.h"
#include "lib/network.h"
#include "lib/lib_errors.h"

#include "zebra/kernel_win32.h"
#include "zebra/zebra_router.h"
#include "zebra/zebra_ns.h"
#include "zebra/zebra_dplane.h"
#include "zebra/zebra_errors.h"
#include "zebra/rt.h"
#include "zebra/debug.h"

/* tables with a change pending, set by the notification callbacks */
#define WIN32_CHANGE_INTERFACES (1 << 0)
#define WIN32_CHANGE_ROUTES	(1 << 1)

/* changes come in bursts: give them time to settle before reading */
#define WIN32_SYNC_DELAY_MSEC 100

static _Atomic uint32_t win32_changes;
static int win32_wake[2] = { -1, -1 };
static struct event *t_win32_wake;
static struct event *t_win32_sync;
static HANDLE win32_notify_if;
static HANDLE win32_notify_addr;
static HANDLE win32_notify_route;

/*
 * Notification callbacks. They run on a Windows thread pool thread, which
 * is not an FRR pthread: nothing of FRR (logging, memory, events) may be
 * used from here.
 */
static void win32_changed(uint32_t what)
{
	uint32_t pending;

	pending = atomic_fetch_or_explicit(&win32_changes, what, memory_order_relaxed);

	/* the first one to flag a change wakes the main pthread up */
	if (!pending)
		(void)write(win32_wake[1], "", 1);
}

static void WINAPI win32_interface_changed(void *arg, MIB_IPINTERFACE_ROW *row,
					   MIB_NOTIFICATION_TYPE type)
{
	win32_changed(WIN32_CHANGE_INTERFACES);
}

static void WINAPI win32_address_changed(void *arg, MIB_UNICASTIPADDRESS_ROW *row,
					 MIB_NOTIFICATION_TYPE type)
{
	win32_changed(WIN32_CHANGE_INTERFACES);
}

static void WINAPI win32_route_changed(void *arg, MIB_IPFORWARD_ROW2 *row,
				       MIB_NOTIFICATION_TYPE type)
{
	win32_changed(WIN32_CHANGE_ROUTES);
}

static void win32_sync(struct event *event)
{
	uint32_t what;

	what = atomic_exchange_explicit(&win32_changes, 0, memory_order_relaxed);

	if (IS_ZEBRA_DEBUG_KERNEL)
		zlog_debug("%s: system change:%s%s", __func__,
			   CHECK_FLAG(what, WIN32_CHANGE_INTERFACES) ? " interfaces" : "",
			   CHECK_FLAG(what, WIN32_CHANGE_ROUTES) ? " routes" : "");

	if (CHECK_FLAG(what, WIN32_CHANGE_INTERFACES))
		win32_interface_sync();

	/* the routes too, whatever changed: Windows adds and removes routes
	 * along with interfaces and addresses
	 */
	if (what)
		win32_route_sync(false);
}

static void win32_wake_read(struct event *event)
{
	char buf[64];

	while (read(win32_wake[0], buf, sizeof(buf)) > 0)
		;

	event_add_read(zrouter.master, win32_wake_read, NULL, win32_wake[0], &t_win32_wake);
	event_add_timer_msec(zrouter.master, win32_sync, NULL, WIN32_SYNC_DELAY_MSEC,
			     &t_win32_sync);
}

void kernel_init(struct zebra_ns *zns)
{
	DWORD rc;

	if (zns->ns_id != NS_DEFAULT)
		return;

	if (pipe(win32_wake) < 0) {
		flog_err_sys(EC_LIB_SYSTEM_CALL,
			     "%s: pipe() failed, system changes will go unnoticed: %s", __func__,
			     safe_strerror(errno));
		return;
	}
	set_nonblocking(win32_wake[0]);
	set_nonblocking(win32_wake[1]);
	event_add_read(zrouter.master, win32_wake_read, NULL, win32_wake[0], &t_win32_wake);

	/* Registered before the tables are first read, so nothing that
	 * changes in between is missed.
	 */
	rc = NotifyIpInterfaceChange(AF_UNSPEC, win32_interface_changed, NULL, FALSE,
				     &win32_notify_if);
	if (rc == ERROR_SUCCESS)
		rc = NotifyUnicastIpAddressChange(AF_UNSPEC, win32_address_changed, NULL, FALSE,
						  &win32_notify_addr);
	if (rc == ERROR_SUCCESS)
		rc = NotifyRouteChange2(AF_UNSPEC, win32_route_changed, NULL, FALSE,
					&win32_notify_route);
	if (rc != ERROR_SUCCESS)
		flog_err(EC_LIB_SYSTEM_CALL,
			 "%s: cannot register for IP Helper change notifications: error %lu",
			 __func__, rc);
}

void kernel_terminate(struct zebra_ns *zns, bool complete)
{
	if (zns->ns_id != NS_DEFAULT)
		return;

	/* these wait for a callback in progress to return */
	if (win32_notify_route)
		CancelMibChangeNotify2(win32_notify_route);
	if (win32_notify_addr)
		CancelMibChangeNotify2(win32_notify_addr);
	if (win32_notify_if)
		CancelMibChangeNotify2(win32_notify_if);
	win32_notify_route = win32_notify_addr = win32_notify_if = NULL;

	event_cancel(&t_win32_wake);
	event_cancel(&t_win32_sync);

	if (win32_wake[0] >= 0) {
		close(win32_wake[0]);
		close(win32_wake[1]);
		win32_wake[0] = win32_wake[1] = -1;
	}

	win32_route_fini();
}

/*
 * Global init for platform-/OS-specific things
 */
void kernel_router_init(void)
{
}

/*
 * Global deinit for platform-/OS-specific things
 */
void kernel_router_terminate(void)
{
}

/*
 * Called by the dplane pthread to read incoming OS messages and dispatch them.
 */
int kernel_dplane_read(struct zebra_dplane_info *info)
{
	return 0;
}

enum zebra_dplane_result kernel_nexthop_update(struct zebra_dplane_ctx *ctx)
{
	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

int kernel_neigh_register(vrf_id_t vrf_id, struct zserv *client, bool reg)
{
	return 0;
}

int kernel_neigh_update(int add, int ifindex, void *addr, char *lla, int llalen, ns_id_t ns_id,
			uint8_t family, bool permanent)
{
	return 0;
}

/* Not implemented, reports 'success' like the routing-socket platforms. */
enum zebra_dplane_result kernel_neigh_update_ctx(struct zebra_dplane_ctx *ctx)
{
	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

int kernel_get_ipmr_sg_stats(struct zebra_vrf *zvrf, void *mroute)
{
	return 0;
}

/* Not implemented, reports 'success' like the routing-socket platforms. */
enum zebra_dplane_result kernel_mac_update_ctx(struct zebra_dplane_ctx *ctx)
{
	return ZEBRA_DPLANE_REQUEST_SUCCESS;
}

int kernel_interface_set_master(struct interface *master, struct interface *slave)
{
	return 0;
}

void kernel_update_multi(struct dplane_ctx_list_head *ctx_list)
{
	struct zebra_dplane_ctx *ctx;
	struct dplane_ctx_list_head handled_list;
	enum zebra_dplane_result res = ZEBRA_DPLANE_REQUEST_SUCCESS;

	dplane_ctx_q_init(&handled_list);

	while (true) {
		ctx = dplane_ctx_dequeue(ctx_list);
		if (ctx == NULL)
			break;

		/*
		 * Skip-kernel ctxs are routed through work_list to preserve
		 * FIFO ordering for downstream providers. Pass them directly
		 * to handled_list without kernel programming.
		 */
		if (dplane_ctx_is_skip_kernel(ctx)) {
			dplane_ctx_enqueue_tail(&handled_list, ctx);
			continue;
		}

		switch (dplane_ctx_get_op(ctx)) {
		case DPLANE_OP_ROUTE_INSTALL:
		case DPLANE_OP_ROUTE_UPDATE:
		case DPLANE_OP_ROUTE_DELETE:
			res = kernel_route_update(ctx);
			break;

		case DPLANE_OP_NH_INSTALL:
		case DPLANE_OP_NH_UPDATE:
		case DPLANE_OP_NH_DELETE:
			res = kernel_nexthop_update(ctx);
			break;

		case DPLANE_OP_LSP_INSTALL:
		case DPLANE_OP_LSP_UPDATE:
		case DPLANE_OP_LSP_DELETE:
			res = kernel_lsp_update(ctx);
			break;

		case DPLANE_OP_PW_INSTALL:
		case DPLANE_OP_PW_UNINSTALL:
			res = kernel_pw_update(ctx);
			break;

		case DPLANE_OP_ADDR_INSTALL:
		case DPLANE_OP_ADDR_UNINSTALL:
			res = kernel_address_update_ctx(ctx);
			break;

		case DPLANE_OP_MAC_INSTALL:
		case DPLANE_OP_MAC_DELETE:
			res = kernel_mac_update_ctx(ctx);
			break;

		case DPLANE_OP_NEIGH_INSTALL:
		case DPLANE_OP_NEIGH_UPDATE:
		case DPLANE_OP_NEIGH_DELETE:
		case DPLANE_OP_VTEP_ADD:
		case DPLANE_OP_VTEP_DELETE:
		case DPLANE_OP_NEIGH_DISCOVER:
			res = kernel_neigh_update_ctx(ctx);
			break;

		case DPLANE_OP_RULE_ADD:
		case DPLANE_OP_RULE_DELETE:
		case DPLANE_OP_RULE_UPDATE:
			res = kernel_pbr_rule_update(ctx);
			break;

		case DPLANE_OP_INTF_INSTALL:
		case DPLANE_OP_INTF_UPDATE:
		case DPLANE_OP_INTF_DELETE:
			res = kernel_intf_update(ctx);
			break;

		case DPLANE_OP_TC_QDISC_INSTALL:
		case DPLANE_OP_TC_QDISC_UNINSTALL:
		case DPLANE_OP_TC_CLASS_ADD:
		case DPLANE_OP_TC_CLASS_DELETE:
		case DPLANE_OP_TC_CLASS_UPDATE:
		case DPLANE_OP_TC_FILTER_ADD:
		case DPLANE_OP_TC_FILTER_DELETE:
		case DPLANE_OP_TC_FILTER_UPDATE:
			res = kernel_tc_update(ctx);
			break;

		/* Ignore 'notifications' - no-op */
		case DPLANE_OP_SYS_ROUTE_ADD:
		case DPLANE_OP_SYS_ROUTE_DELETE:
		case DPLANE_OP_ROUTE_NOTIFY:
		case DPLANE_OP_LSP_NOTIFY:
			res = ZEBRA_DPLANE_REQUEST_SUCCESS;
			break;

		/* EVPN-MH FDB nexthops are a netlink-only feature - no-op here */
		case DPLANE_OP_NH_FDB_INSTALL:
		case DPLANE_OP_NH_FDB_DELETE:
			res = ZEBRA_DPLANE_REQUEST_SUCCESS;
			break;

		case DPLANE_OP_INTF_NETCONFIG:
			res = kernel_intf_netconf_update(ctx);
			break;

		case DPLANE_OP_NONE:
		case DPLANE_OP_BR_PORT_UPDATE:
		case DPLANE_OP_IPTABLE_ADD:
		case DPLANE_OP_IPTABLE_DELETE:
		case DPLANE_OP_IPSET_ADD:
		case DPLANE_OP_IPSET_DELETE:
		case DPLANE_OP_IPSET_ENTRY_ADD:
		case DPLANE_OP_IPSET_ENTRY_DELETE:
		case DPLANE_OP_NEIGH_IP_INSTALL:
		case DPLANE_OP_NEIGH_IP_DELETE:
		case DPLANE_OP_NEIGH_TABLE_UPDATE:
		case DPLANE_OP_GRE_SET:
		case DPLANE_OP_INTF_ADDR_ADD:
		case DPLANE_OP_INTF_ADDR_DEL:
		case DPLANE_OP_INTF_SPEED_GET:
		case DPLANE_OP_STARTUP_STAGE:
		case DPLANE_OP_SRV6_ENCAP_SRCADDR_SET:
		case DPLANE_OP_VLAN_INSTALL:
		case DPLANE_OP_FDB_READ:
		case DPLANE_OP_NEIGH_READ:
		case DPLANE_OP_TC_QDISC_READ:
		case DPLANE_OP_TC_QDISC_NOTIFY:
		case DPLANE_OP_TUNNEL_NOTIFY:
			flog_err(EC_ZEBRA_DPLANE_OP_UNHANDLED, "Unhandled dplane data for %s",
				 dplane_op2str(dplane_ctx_get_op(ctx)));
			res = ZEBRA_DPLANE_REQUEST_FAILURE;
		}

		dplane_ctx_set_status(ctx, res);

		dplane_ctx_enqueue_tail(&handled_list, ctx);
	}

	dplane_ctx_q_init(ctx_list);
	dplane_ctx_list_append(ctx_list, &handled_list);
}

#endif /* _WIN32 */
