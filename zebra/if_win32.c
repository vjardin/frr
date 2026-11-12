// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows kernel interface for zebra, on top of the IP Helper API:
 * interfaces and their addresses.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#ifdef _WIN32

#include "lib/if.h"
#include "lib/prefix.h"
#include "lib/vrf.h"
#include "lib/memory.h"
#include "lib/log.h"
#include "lib/lib_errors.h"

#include "zebra/kernel_win32.h"
#include "zebra/interface.h"
#include "zebra/connected.h"
#include "zebra/ioctl.h"
#include "zebra/redistribute.h"
#include "zebra/rt.h"
#include "zebra/zebra_ns.h"
#include "zebra/zebra_dplane.h"
#include "zebra/zebra_errors.h"
#include "zebra/debug.h"

DEFINE_MTYPE_STATIC(ZEBRA, WIN32_ADAPTERS, "Windows adapter list");

/* first buffer size to try, as recommended for GetAdaptersAddresses() */
#define WIN32_ADAPTERS_BUFSZ 15000

/*
 * Snapshot of the adapters that have IPv4 or IPv6 enabled, each with its
 * unicast addresses. Returns false if the system could not tell,
 * *adapters is NULL when there is no adapter at all.
 */
static bool win32_adapters_get(IP_ADAPTER_ADDRESSES **adapters)
{
	ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
	ULONG len = WIN32_ADAPTERS_BUFSZ;
	ULONG rc = ERROR_BUFFER_OVERFLOW;
	int tries;

	*adapters = NULL;

	/* the size needed can grow between two calls */
	for (tries = 0; tries < 5 && rc == ERROR_BUFFER_OVERFLOW; tries++) {
		*adapters = XREALLOC(MTYPE_WIN32_ADAPTERS, *adapters, len);
		rc = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, *adapters, &len);
	}

	if (rc == ERROR_SUCCESS)
		return true;

	XFREE(MTYPE_WIN32_ADAPTERS, *adapters);
	if (rc == ERROR_NO_DATA)
		return true;

	flog_err(EC_ZEBRA_IFLIST_FAILED,
		 "Could not enumerate interfaces: GetAdaptersAddresses() error %lu", rc);
	return false;
}

static ifindex_t win32_adapter_ifindex(const IP_ADAPTER_ADDRESSES *aa)
{
	/* the same index for both families, 0 for a family that is disabled */
	return aa->IfIndex ? aa->IfIndex : aa->Ipv6IfIndex;
}

/*
 * Windows shows its users a "friendly name" such as "Ethernet 2" or
 * "vEthernet (Default Switch)", which the CLI cannot take. Each interface
 * also has a stable name derived from its LUID, such as "ethernet_32769" or
 * "loopback_0": the one if_indextoname() returns. That is the interface
 * name here, the friendly name becomes the OS description.
 *
 * The LUID is what identifies an interface for good. Its index "may change
 * when a network adapter is disabled and then enabled, and should not be
 * considered persistent" (MIB_IF_ROW2 documentation).
 */
static void win32_luid_name(const NET_LUID *luid, ifindex_t ifindex, char *name, size_t len)
{
	char luid_name[NDIS_IF_MAX_STRING_SIZE + 1];

	if (ConvertInterfaceLuidToNameA(luid, luid_name, sizeof(luid_name)) == ERROR_SUCCESS &&
	    strlen(luid_name) < len)
		strlcpy(name, luid_name, len);
	else
		snprintf(name, len, "if%d", ifindex);
}

static void win32_adapter_name(const IP_ADAPTER_ADDRESSES *aa, char *name, size_t len)
{
	win32_luid_name(&aa->Luid, win32_adapter_ifindex(aa), name, len);
}

static bool win32_if_row(const char *name, ifindex_t ifindex, MIB_IF_ROW2 *row)
{
	memset(row, 0, sizeof(*row));

	if (ConvertInterfaceNameToLuidA(name, &row->InterfaceLuid) != ERROR_SUCCESS) {
		row->InterfaceLuid.Value = 0;
		row->InterfaceIndex = ifindex;
	}

	return GetIfEntry2(row) == ERROR_SUCCESS;
}

static bool win32_row_disabled(const MIB_IF_ROW2 *row)
{
	return row->InterfaceAndOperStatusFlags.HardwareInterface &&
	       row->AdminStatus == NET_IF_ADMIN_STATUS_DOWN;
}

static uint64_t win32_if_flags(const MIB_IF_ROW2 *row)
{
	uint64_t flags = 0;

	if (row->AdminStatus != NET_IF_ADMIN_STATUS_DOWN && row->OperStatus != IfOperStatusNotPresent)
		SET_FLAG(flags, IFF_UP);

	/* Link state, as Linux reports it: the interface flags have the
	 * Linux values on Windows (see lib/win32/include/net/if.h), and with
	 * IFF_LOWER_UP defined an operative interface must have it.
	 */
	if (row->OperStatus == IfOperStatusUp)
		SET_FLAG(flags, IFF_RUNNING | IFF_LOWER_UP);

	switch (row->AccessType) {
	case NET_IF_ACCESS_LOOPBACK:
		SET_FLAG(flags, IFF_LOOPBACK | IFF_MULTICAST);
		break;
	case NET_IF_ACCESS_POINT_TO_POINT:
		SET_FLAG(flags, IFF_POINTOPOINT | IFF_MULTICAST);
		break;
	case NET_IF_ACCESS_BROADCAST:
	case NET_IF_ACCESS_POINT_TO_MULTI_POINT:
	case NET_IF_ACCESS_MAXIMUM:
		SET_FLAG(flags, IFF_BROADCAST | IFF_MULTICAST);
		break;
	}

	return flags;
}

static enum zebra_link_type win32_if_link_type(const MIB_IF_ROW2 *row)
{
	switch (row->Type) {
	case IF_TYPE_ETHERNET_CSMACD:
	case IF_TYPE_IEEE80211:
		return ZEBRA_LLT_ETHER;
	case IF_TYPE_SOFTWARE_LOOPBACK:
		return ZEBRA_LLT_LOOPBACK;
	case IF_TYPE_PPP:
		return ZEBRA_LLT_PPP;
	case IF_TYPE_IEEE1394:
		return ZEBRA_LLT_IEEE1394;
	default:
		return ZEBRA_LLT_UNKNOWN;
	}
}

/*
 * Windows gives its loopback an MTU of 4294967295, for "no limit". Cap it
 * at the largest IP packet, so that nothing sizes a buffer from it.
 */
static uint32_t win32_if_mtu(ULONG mtu)
{
	return mtu > 65535 ? 65535 : mtu;
}

/* bits per second to Mbit/s, all ones when Windows does not know */
static uint32_t win32_if_speed(ULONG64 bps)
{
	if (bps == ~(ULONG64)0)
		return 0;

	bps /= 1000000;
	return bps > UINT32_MAX ? UINT32_MAX : (uint32_t)bps;
}

/* Tell zebra about an interface of the system, new or already known. */
static struct interface *win32_interface_update(const MIB_IF_ROW2 *row, ifindex_t ifindex,
						uint32_t mtu)
{
	char name[IFNAMSIZ];
	char alias[256];
	struct interface *ifp;
	struct zebra_if *zif;
	uint64_t flags;
	bool created = false;

	if (!ifindex)
		return NULL;

	win32_luid_name(&row->InterfaceLuid, ifindex, name, sizeof(name));
	flags = win32_if_flags(row);

	ifp = if_get_by_name(name, VRF_DEFAULT, VRF_DEFAULT_NAME);

	/* the name is back with another index: not the interface we knew */
	if (ifp->ifindex != IFINDEX_INTERNAL && ifp->ifindex != ifindex) {
		if_flags_update(ifp, ifp->flags & ~(uint64_t)(IFF_UP | IFF_RUNNING | IFF_LOWER_UP));
		if_delete_update(&ifp);
		ifp = if_get_by_name(name, VRF_DEFAULT, VRF_DEFAULT_NAME);
	}

	/* Unknown so far, or only known from the configuration. */
	if (ifp->ifindex == IFINDEX_INTERNAL) {
		if (IS_ZEBRA_DEBUG_KERNEL)
			zlog_debug("%s: new interface %s, ifindex %d", __func__, name, ifindex);

		if_set_index(ifp, ifindex);
		created = true;
	}

	zif = ifp->info;

	if (WideCharToMultiByte(CP_UTF8, 0, row->Alias, -1, alias, sizeof(alias), NULL, NULL) > 0 &&
	    (!zif->desc || strcmp(zif->desc, alias))) {
		XFREE(MTYPE_ZIF_DESC, zif->desc);
		zif->desc = XSTRDUP(MTYPE_ZIF_DESC, alias);
	}

	ifp->ll_type = win32_if_link_type(row);
	if (row->PhysicalAddressLength <= sizeof(ifp->hw_addr) &&
	    (ifp->hw_addr_len != (int)row->PhysicalAddressLength ||
	     memcmp(ifp->hw_addr, row->PhysicalAddress, row->PhysicalAddressLength)))
		if_update_state_hw_addr(ifp, row->PhysicalAddress, row->PhysicalAddressLength);

	if (ifp->speed != win32_if_speed(row->TransmitLinkSpeed))
		if_update_state_speed(ifp, win32_if_speed(row->TransmitLinkSpeed));

	if (created) {
		if_update_state_mtu(ifp, mtu);
		if_update_state_mtu6(ifp, mtu);
		if_flags_update(ifp, flags);
		if_add_update(ifp);
		return ifp;
	}

	/* up or down as needed, with everything that depends on it */
	if_flags_update(ifp, flags);

	if (ifp->mtu != mtu || ifp->mtu6 != mtu) {
		if_update_state_mtu(ifp, mtu);
		if_update_state_mtu6(ifp, mtu);

		/* propagate */
		if (if_is_operative(ifp))
			zebra_interface_up_update(ifp);
	}

	return ifp;
}

/*
 * An address of the system as a prefix.
 */
static bool win32_address_prefix(const IP_ADAPTER_UNICAST_ADDRESS *unicast, struct prefix *p)
{
	const struct sockaddr *sa = unicast->Address.lpSockaddr;

	if (unicast->DadState != IpDadStatePreferred && unicast->DadState != IpDadStateDeprecated)
		return false;

	memset(p, 0, sizeof(*p));

	switch (sa->sa_family) {
	case AF_INET:
		p->family = AF_INET;
		p->u.prefix4 = ((const struct sockaddr_in *)sa)->sin_addr;
		break;
	case AF_INET6:
		p->family = AF_INET6;
		p->u.prefix6 = ((const struct sockaddr_in6 *)sa)->sin6_addr;
		break;
	default:
		return false;
	}
	p->prefixlen = unicast->OnLinkPrefixLength;

	return true;
}

static bool win32_adapter_has_address(const IP_ADAPTER_ADDRESSES *aa, const struct prefix *p)
{
	const IP_ADAPTER_UNICAST_ADDRESS *unicast;
	struct prefix addr;

	for (unicast = aa ? aa->FirstUnicastAddress : NULL; unicast; unicast = unicast->Next) {
		if (win32_address_prefix(unicast, &addr) && prefix_same(&addr, p))
			return true;
	}

	return false;
}

/*
 * Bring the addresses zebra has for an interface in line with the system's.
 */
static void win32_addresses_update(struct interface *ifp, const IP_ADAPTER_ADDRESSES *aa)
{
	const IP_ADAPTER_UNICAST_ADDRESS *unicast;
	struct connected *ifc;
	struct prefix p;

	/* the ones that are gone */
	frr_each_safe (if_connected, ifp->connected, ifc) {
		if (!CHECK_FLAG(ifc->conf, ZEBRA_IFC_REAL))
			continue;
		if (win32_adapter_has_address(aa, ifc->address))
			continue;

		/* a copy: ifc can be freed below */
		p = *ifc->address;

		if (IS_ZEBRA_DEBUG_KERNEL)
			zlog_debug("%s: %s lost address %pFX", __func__, ifp->name, &p);

		if (p.family == AF_INET)
			connected_delete_ipv4(ifp, 0, &p.u.prefix4, p.prefixlen, NULL);
		else if (p.family == AF_INET6)
			connected_delete_ipv6(ifp, &p.u.prefix6, NULL, p.prefixlen);
	}

	/* the new ones */
	for (unicast = aa ? aa->FirstUnicastAddress : NULL; unicast; unicast = unicast->Next) {
		if (!win32_address_prefix(unicast, &p))
			continue;

		/* zebra does not keep these, 127.0.0.1 and ::1 for instance */
		if (p.family == AF_INET ? ipv4_martian(&p.u.prefix4) : ipv6_martian(&p.u.prefix6))
			continue;

		ifc = connected_lookup_prefix_exact(ifp, &p);
		if (ifc && CHECK_FLAG(ifc->conf, ZEBRA_IFC_REAL))
			continue;

		if (IS_ZEBRA_DEBUG_KERNEL)
			zlog_debug("%s: %s got address %pFX", __func__, ifp->name, &p);

		if (p.family == AF_INET)
			connected_add_ipv4(ifp, 0, &p.u.prefix4, p.prefixlen, NULL, NULL,
					   aa->Ipv4Metric);
		else
			connected_add_ipv6(ifp, 0, &p.u.prefix6, NULL, p.prefixlen, NULL,
					   aa->Ipv6Metric);
	}
}

static const MIB_IF_ROW2 *win32_table_disabled(const MIB_IF_TABLE2 *table, const char *name)
{
	char row_name[IFNAMSIZ];
	ULONG i;

	for (i = 0; table && i < table->NumEntries; i++) {
		const MIB_IF_ROW2 *row = &table->Table[i];

		if (!win32_row_disabled(row))
			continue;

		win32_luid_name(&row->InterfaceLuid, row->InterfaceIndex, row_name,
				sizeof(row_name));
		if (!strcmp(row_name, name))
			return row;
	}

	return NULL;
}

/* A pass of win32_interface_sync() is in progress. */
static bool win32_syncing;

void win32_interface_sync(void)
{
	struct vrf *vrf = vrf_lookup_by_id(VRF_DEFAULT);
	IP_ADAPTER_ADDRESSES *adapters, *aa;
	MIB_IF_TABLE2 *table = NULL;
	struct interface *ifp, *next;
	char name[IFNAMSIZ];
	MIB_IF_ROW2 row;
	ULONG i;

	if (!vrf || !win32_adapters_get(&adapters))
		return;

	win32_syncing = true;

	if (GetIfTable2(&table) != ERROR_SUCCESS)
		table = NULL;

	/* interfaces the system no longer has */
	RB_FOREACH_SAFE (ifp, if_name_head, &vrf->ifaces_by_name, next) {
		if (ifp->ifindex == IFINDEX_INTERNAL)
			continue;

		for (aa = adapters; aa; aa = aa->Next) {
			win32_adapter_name(aa, name, sizeof(name));
			if (win32_adapter_ifindex(aa) && !strcmp(name, ifp->name))
				break;
		}
		if (aa || win32_table_disabled(table, ifp->name))
			continue;

		if (IS_ZEBRA_DEBUG_KERNEL)
			zlog_debug("%s: interface %s, ifindex %d is gone", __func__, ifp->name,
				   ifp->ifindex);

		/* down first, so that what depends on it is withdrawn */
		if_flags_update(ifp, ifp->flags & ~(uint64_t)(IFF_UP | IFF_RUNNING | IFF_LOWER_UP));
		if_delete_update(&ifp);
	}

	for (aa = adapters; aa; aa = aa->Next) {
		memset(&row, 0, sizeof(row));
		row.InterfaceLuid = aa->Luid;
		if (GetIfEntry2(&row) != ERROR_SUCCESS)
			continue;

		ifp = win32_interface_update(&row, win32_adapter_ifindex(aa),
					     win32_if_mtu(aa->Mtu));
		if (ifp)
			win32_addresses_update(ifp, aa);
	}

	for (i = 0; table && i < table->NumEntries; i++) {
		if (!win32_row_disabled(&table->Table[i]))
			continue;

		ifp = win32_interface_update(&table->Table[i], table->Table[i].InterfaceIndex,
					     win32_if_mtu(table->Table[i].Mtu));
		if (ifp)
			win32_addresses_update(ifp, NULL);
	}

	if (table)
		FreeMibTable(table);
	XFREE(MTYPE_WIN32_ADAPTERS, adapters);

	win32_syncing = false;
}

/* Interface listing up function, at startup. */
void interface_list(struct zebra_ns *zns)
{
	if (zns->ns_id != NS_DEFAULT) {
		zlog_debug("%s: ignore NS %u", __func__, zns->ns_id);
		return;
	}

	win32_interface_sync();

	zebra_dplane_startup_stage(zns->ns_id, ZEBRA_DPLANE_INTERFACES_READ);
}

void interface_list_tunneldump(struct zebra_ns *zns)
{
	zebra_dplane_startup_stage(zns->ns_id, ZEBRA_DPLANE_TUNNELS_READ);
}

void interface_list_second(struct zebra_ns *zns)
{
	zebra_dplane_startup_stage(zns->ns_id, ZEBRA_DPLANE_ADDRESSES_READ);
}

/* get interface flags */
void if_get_flags(struct interface *ifp)
{
	MIB_IF_ROW2 row;

	if (!win32_if_row(ifp->name, ifp->ifindex, &row))
		return;

	if_flags_update(ifp, win32_if_flags(&row));
}

/* the device setup class of the network adapters, GUID_DEVCLASS_NET */
#define WIN32_NET_CLASS "{4d36e972-e325-11ce-bfc1-08002be10318}"

/* longest wait for the IP interfaces of an adapter that was just enabled */
#define WIN32_ENABLE_WAIT_MSEC 3000

static bool win32_if_devnode(const NET_LUID *luid, DEVINST *devnode)
{
	ULONG filter = CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT;
	CONFIGRET cr = CR_BUFFER_SMALL;
	char *ids = NULL, *id;
	bool found = false;
	char value[64];
	char want[64];
	ULONG len = 0;
	DEVINST dn;
	DWORD size;
	HKEY key;
	int tries;
	GUID g;

	if (ConvertInterfaceLuidToGuid(luid, &g) != ERROR_SUCCESS)
		return false;

	snprintf(want, sizeof(want), "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		 g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
		 g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);

	/* the list can grow between the two calls */
	for (tries = 0; tries < 5 && cr == CR_BUFFER_SMALL; tries++) {
		if (CM_Get_Device_ID_List_SizeA(&len, WIN32_NET_CLASS, filter) != CR_SUCCESS || !len)
			break;

		ids = XREALLOC(MTYPE_WIN32_ADAPTERS, ids, len);
		cr = CM_Get_Device_ID_ListA(WIN32_NET_CLASS, ids, len, filter);
	}

	/* a list of strings that ends with an empty one */
	for (id = ids; cr == CR_SUCCESS && *id && !found; id += strlen(id) + 1) {
		if (CM_Locate_DevNodeA(&dn, id, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
			continue;
		if (CM_Open_DevNode_Key(dn, KEY_QUERY_VALUE, 0, RegDisposition_OpenExisting, &key,
					CM_REGISTRY_SOFTWARE) != CR_SUCCESS)
			continue;

		size = sizeof(value);
		if (RegGetValueA(key, NULL, "NetCfgInstanceId", RRF_RT_REG_SZ, NULL, value,
				 &size) == ERROR_SUCCESS &&
		    !strcasecmp(value, want)) {
			*devnode = dn;
			found = true;
		}
		RegCloseKey(key);
	}

	XFREE(MTYPE_WIN32_ADAPTERS, ids);
	return found;
}

/* Is the adapter in the list of those that have an IP interface? */
static bool win32_adapter_present(const NET_LUID *luid)
{
	IP_ADAPTER_ADDRESSES *adapters, *aa;
	bool present = false;

	if (!win32_adapters_get(&adapters))
		return false;

	for (aa = adapters; aa && !present; aa = aa->Next)
		present = aa->Luid.Value == luid->Value && win32_adapter_ifindex(aa);

	XFREE(MTYPE_WIN32_ADAPTERS, adapters);
	return present;
}

/*
 * Administrative state of an interface: "shutdown" and "no shutdown".
 *
 * Only for adapters. The loopback interface cannot be shut down.
 */
static int win32_if_admin_set(struct interface *ifp, bool up)
{
	MIB_IF_ROW2 row;
	DEVINST devnode;
	CONFIGRET cr;
	int waited;

	if (!win32_if_row(ifp->name, ifp->ifindex, &row)) {
		errno = ENXIO;
		return -1;
	}

	/* nothing to change */
	if ((row.AdminStatus != NET_IF_ADMIN_STATUS_DOWN) == up)
		return 0;

	if (!win32_if_devnode(&row.InterfaceLuid, &devnode)) {
		errno = EOPNOTSUPP;
		return -1;
	}

	cr = up ? CM_Enable_DevNode(devnode, 0) : CM_Disable_DevNode(devnode, 0);
	if (cr != CR_SUCCESS) {
		flog_err(EC_LIB_SYSTEM_CALL,
			 "Could not %s the network adapter of %s: configuration manager error %lu%s",
			 up ? "enable" : "disable", ifp->name, cr,
			 cr == CR_ACCESS_DENIED ? " (zebra must run as Administrator)" : "");
		errno = cr == CR_ACCESS_DENIED ? EPERM : EIO;
		return -1;
	}

	/* The IP interfaces of the adapter come back a moment after it is
	 * enabled, and addresses can only be set once they
	 * are there: the caller does that next.
	 */
	for (waited = 0; up && waited < WIN32_ENABLE_WAIT_MSEC; waited += 50) {
		if (win32_adapter_present(&row.InterfaceLuid))
			break;
		Sleep(50);
	}

	if (!win32_syncing)
		win32_interface_sync();

	return 0;
}

int if_set_flags(struct interface *ifp, uint64_t flags)
{
	if (!CHECK_FLAG(flags, IFF_UP) || (flags & ~(uint64_t)(IFF_UP | IFF_RUNNING))) {
		errno = ENOTSUP;
		return -1;
	}

	return win32_if_admin_set(ifp, true);
}

int if_unset_flags(struct interface *ifp, uint64_t flags)
{
	if (flags != IFF_UP) {
		errno = ENOTSUP;
		return -1;
	}

	return win32_if_admin_set(ifp, false);
}

/* Called from the dplane pthread */
enum zebra_dplane_result kernel_address_update_ctx(struct zebra_dplane_ctx *ctx)
{
	bool install = dplane_ctx_get_op(ctx) == DPLANE_OP_ADDR_INSTALL;
	const struct prefix *p = dplane_ctx_get_intf_addr(ctx);
	const char *ifname = dplane_ctx_get_ifname(ctx);
	MIB_UNICASTIPADDRESS_ROW row;
	DWORD rc;

	InitializeUnicastIpAddressEntry(&row);

	/* by LUID when the name gives it: the index can be out of date */
	if (ConvertInterfaceNameToLuidA(ifname, &row.InterfaceLuid) != ERROR_SUCCESS) {
		row.InterfaceLuid.Value = 0;
		row.InterfaceIndex = dplane_ctx_get_ifindex(ctx);
	}

	switch (p->family) {
	case AF_INET:
		row.Address.Ipv4.sin_family = AF_INET;
		row.Address.Ipv4.sin_addr = p->u.prefix4;
		break;
	case AF_INET6:
		row.Address.Ipv6.sin6_family = AF_INET6;
		row.Address.Ipv6.sin6_addr = p->u.prefix6;
		break;
	default:
		dplane_ctx_set_errno(ctx, EAFNOSUPPORT);
		return ZEBRA_DPLANE_REQUEST_FAILURE;
	}
	row.OnLinkPrefixLength = p->prefixlen;

	if (install) {
		row.DadState = IpDadStatePreferred;
		rc = CreateUnicastIpAddressEntry(&row);

		/* already there, set by the system's tools for instance */
		if (rc == ERROR_OBJECT_ALREADY_EXISTS)
			rc = ERROR_SUCCESS;
	} else {
		rc = DeleteUnicastIpAddressEntry(&row);

		if (rc == ERROR_NOT_FOUND || rc == ERROR_INVALID_PARAMETER)
			rc = ERROR_SUCCESS;
	}

	if (rc == ERROR_SUCCESS)
		return ZEBRA_DPLANE_REQUEST_SUCCESS;

	flog_err(EC_LIB_SYSTEM_CALL, "Could not %s address %pFX %s %s: %s() error %lu%s",
		 install ? "add" : "remove", p, install ? "to" : "from", ifname,
		 install ? "CreateUnicastIpAddressEntry" : "DeleteUnicastIpAddressEntry", rc,
		 rc == ERROR_ACCESS_DENIED ? " (zebra must run as Administrator)" : "");
	dplane_ctx_set_errno(ctx, rc == ERROR_ACCESS_DENIED ? EPERM : EIO);
	return ZEBRA_DPLANE_REQUEST_FAILURE;
}

/* Called from the dplane pthread. */
void kernel_read_intf_speed(struct zebra_dplane_ctx *ctx)
{
	MIB_IF_ROW2 row;

	if (!win32_if_row(dplane_ctx_get_ifname(ctx), dplane_ctx_get_ifindex(ctx), &row)) {
		dplane_ctx_set_status(ctx, ZEBRA_DPLANE_REQUEST_FAILURE);
		dplane_ctx_set_ifp_speed_set(ctx, false);
		return;
	}

	dplane_ctx_set_status(ctx, ZEBRA_DPLANE_REQUEST_SUCCESS);
	dplane_ctx_set_ifp_speed(ctx, win32_if_speed(row.TransmitLinkSpeed));
	dplane_ctx_set_ifp_speed_set(ctx, true);
}

#endif /* _WIN32 */
