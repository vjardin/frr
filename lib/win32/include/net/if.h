// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <net/if.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_NET_IF_H
#define _FRR_WIN_NET_IF_H
#define IFNAMSIZ 16
#undef IF_NAMESIZE
#define IF_NAMESIZE IFNAMSIZ
/* interface flag values (zapi carries them as-is) */
#undef IFF_UP
#undef IFF_BROADCAST
#undef IFF_LOOPBACK
#undef IFF_POINTTOPOINT
#undef IFF_MULTICAST
#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_DEBUG 0x4
#define IFF_LOOPBACK 0x8
#define IFF_POINTOPOINT 0x10
#define IFF_NOTRAILERS 0x20
#define IFF_RUNNING 0x40
#define IFF_NOARP 0x80
#define IFF_PROMISC 0x100
#define IFF_ALLMULTI 0x200
#define IFF_MASTER 0x400
#define IFF_SLAVE 0x800
#define IFF_MULTICAST 0x1000
#define IFF_PORTSEL 0x2000
#define IFF_AUTOMEDIA 0x4000
#define IFF_DYNAMIC 0x8000
#define IFF_LOWER_UP 0x10000
#define IFF_DORMANT 0x20000
#define IFF_ECHO 0x40000
__declspec(dllimport) unsigned long __stdcall if_nametoindex(const char *ifname);
__declspec(dllimport) char *__stdcall if_indextoname(unsigned long ifindex, char *ifname);
struct ifreq {
	char ifr_name[IFNAMSIZ];
	union {
		struct sockaddr ifru_addr;
		short ifru_flags;
		int ifru_ivalue;
		int ifru_mtu;
		char ifru_data[24];
	} ifr_ifru;
};
#define ifr_addr ifr_ifru.ifru_addr
#define ifr_flags ifr_ifru.ifru_flags
#define ifr_mtu ifr_ifru.ifru_mtu
#define ifr_ifindex ifr_ifru.ifru_ivalue
#endif
