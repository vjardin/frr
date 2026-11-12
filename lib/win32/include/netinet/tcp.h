// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <netinet/tcp.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_NETINET_TCP_H
#define _FRR_WIN_NETINET_TCP_H

#ifndef TCP_MAXSEG
#define TCP_MAXSEG 4
#endif
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#endif
