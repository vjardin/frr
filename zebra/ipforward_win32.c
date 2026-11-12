// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows kernel interface for zebra, on top of the IP Helper API:
 * IP forwarding state.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#ifdef _WIN32

#include "zebra/kernel_win32.h"
#include "zebra/ipforward.h"

/* 1 if the system forwards packets of that family, 0 if it does not. */
static int win32_forwarding(ULONG family)
{
	MIB_IPSTATS stats = {};

	if (GetIpStatisticsEx(&stats, family) != ERROR_SUCCESS)
		return 0;

	return stats.dwForwarding == MIB_IP_FORWARDING;
}

int ipforward(void)
{
	return win32_forwarding(AF_INET);
}

/*
 * Only reading the system's state for now: these report it unchanged, so
 * that asking for a change that did not happen is reported as a failure.
 */
int ipforward_on(void)
{
	return ipforward();
}

int ipforward_off(void)
{
	return ipforward();
}

int ipforward_ipv6(void)
{
	return win32_forwarding(AF_INET6);
}

int ipforward_ipv6_on(void)
{
	return ipforward_ipv6();
}

int ipforward_ipv6_off(void)
{
	return ipforward_ipv6();
}

#endif /* _WIN32 */
