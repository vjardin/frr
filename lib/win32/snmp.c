// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The sockets of net-snmp as FRR descriptors on Windows
 *
 * net-snmp makes and uses its sockets itself, with no hook to make them
 * otherwise. The descriptors here stand for them without owning them
 * (frr_win_fd_adopt()). A large fd set of net-snmp is an fd_set on
 * Windows, a list of SOCKET values: the set agentx.c gets holds the
 * descriptors instead, as numbers, which is what it gives the event loop
 * and what it looks up in the set.
 *
 * net-snmp closes its sockets itself: a descriptor whose socket is no
 * longer in its list is freed, before agentx.c stops watching it in the
 * same pass.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#include "snmp_win32.h"

#undef snmp_select_info2
#undef snmp_read2

#define SNMP_FDS_MAX 64

static int snmp_fds[SNMP_FDS_MAX];
static int snmp_fd_count;

static void snmp_fd_track(int fd)
{
	int i;

	for (i = 0; i < snmp_fd_count; i++)
		if (snmp_fds[i] == fd)
			return;
	if (snmp_fd_count < SNMP_FDS_MAX)
		snmp_fds[snmp_fd_count++] = fd;
}

static bool snmp_set_has(const netsnmp_large_fd_set *set, SOCKET s)
{
	u_int i;

	for (i = 0; i < set->lfs_setptr->fd_count; i++)
		if (set->lfs_setptr->fd_array[i] == s)
			return true;
	return false;
}

int frr_win_snmp_select_info2(int *numfds, netsnmp_large_fd_set *fdset,
			      struct timeval *timeout, int *block)
{
	netsnmp_large_fd_set native;
	int native_numfds = 0, ret, fd, i;
	u_int n;

	netsnmp_large_fd_set_init(&native, FD_SETSIZE);
	ret = snmp_select_info2(&native_numfds, &native, timeout, block);

	for (n = 0; n < native.lfs_setptr->fd_count; n++) {
		fd = frr_win_fd_adopt(native.lfs_setptr->fd_array[n]);
		if (fd < 0)
			continue;
		snmp_fd_track(fd);
		netsnmp_large_fd_setfd((SOCKET)fd, fdset);
		if (fd + 1 > *numfds)
			*numfds = fd + 1;
	}

	/* the sockets net-snmp closed */
	for (i = 0; i < snmp_fd_count;) {
		if (snmp_set_has(&native, frr_win_fd2sock(snmp_fds[i]))) {
			i++;
			continue;
		}
		frr_win_fd_unadopt(snmp_fds[i]);
		snmp_fds[i] = snmp_fds[--snmp_fd_count];
	}

	netsnmp_large_fd_set_cleanup(&native);
	return ret;
}

void frr_win_snmp_read2(netsnmp_large_fd_set *fdset)
{
	netsnmp_large_fd_set native;
	SOCKET s;
	u_int n;

	netsnmp_large_fd_set_init(&native, FD_SETSIZE);
	for (n = 0; n < fdset->lfs_setptr->fd_count; n++) {
		s = frr_win_fd2sock((int)fdset->lfs_setptr->fd_array[n]);
		if (s != INVALID_SOCKET)
			netsnmp_large_fd_setfd(s, &native);
	}
	snmp_read2(&native);
	netsnmp_large_fd_set_cleanup(&native);
}
