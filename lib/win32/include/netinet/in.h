// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <netinet/in.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_NETINET_IN_H
#define _FRR_WIN_NETINET_IN_H

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET 127
#endif
#ifndef IN_MULTICAST
#define IN_MULTICAST(a) ((((uint32_t)(a)) & 0xf0000000) == 0xe0000000)
#endif
#ifndef IN_EXPERIMENTAL
#define IN_EXPERIMENTAL(a) ((((uint32_t)(a)) & 0xf0000000) == 0xf0000000)
#endif
#ifndef IN_BADCLASS
#define IN_BADCLASS(a) IN_EXPERIMENTAL(a)
#endif
#ifndef IN_CLASSD
#define IN_CLASSD(a) IN_MULTICAST(a)
#endif
#ifndef INADDR_ALLHOSTS_GROUP
#define INADDR_ALLHOSTS_GROUP 0xe0000001U
#endif
#ifndef INADDR_ALLRTRS_GROUP
#define INADDR_ALLRTRS_GROUP 0xe0000002U
#endif
#ifndef INADDR_MAX_LOCAL_GROUP
#define INADDR_MAX_LOCAL_GROUP 0xe00000ffU
#endif
#ifndef IPPROTO_GRE
#define IPPROTO_GRE 47
#endif
#ifndef IPPROTO_OSPFIGP
#define IPPROTO_OSPFIGP 89
#endif
#ifndef IPPROTO_PIM
#define IPPROTO_PIM 103
#endif
#ifndef IPPROTO_VRRP
#define IPPROTO_VRRP 112
#endif
#ifndef IPPROTO_MH
#define IPPROTO_MH 135
#endif
#ifndef IPPROTO_EIGRPIGP
#define IPPROTO_EIGRPIGP 88
#endif

#endif
