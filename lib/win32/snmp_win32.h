// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Forced ahead of the SNMP sources on Windows (/FI, after frr_win32.h)
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SNMP_H
#define _FRR_WIN_SNMP_H

#include <zebra.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-dllimport"
#pragma clang diagnostic ignored "-Wundef"
#define mode_t frr_win_netsnmp_mode_t
#define HAVE_SSIZE_T 1
#define HAVE_NFDS_T 1
#define HAVE_GETTIMEOFDAY 1
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/large_fd_set.h>
#undef mode_t
#undef HAVE_SSIZE_T
#undef HAVE_NFDS_T
#undef HAVE_GETTIMEOFDAY
#pragma clang diagnostic pop

extern int frr_win_snmp_select_info2(int *numfds, netsnmp_large_fd_set *fdset,
				     struct timeval *timeout, int *block);
extern void frr_win_snmp_read2(netsnmp_large_fd_set *fdset);

#define snmp_select_info2 frr_win_snmp_select_info2
#define snmp_read2 frr_win_snmp_read2

#endif /* _FRR_WIN_SNMP_H */
