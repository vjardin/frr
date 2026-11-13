// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows kernel interface for zebra, on top of the IP Helper API:
 * IP forwarding state.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#ifdef _WIN32

#include "lib/log.h"
#include "lib/lib_errors.h"

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

static int win32_forwarding_set(ULONG family, bool on)
{
	MIB_IPSTATS stats = {};
	DWORD rc;

	stats.dwForwarding = on ? MIB_IP_FORWARDING : MIB_IP_NOT_FORWARDING;
	stats.dwDefaultTTL = MIB_USE_CURRENT_TTL;

	rc = SetIpStatisticsEx(&stats, family);
	if (rc != ERROR_SUCCESS)
		flog_err(EC_LIB_SYSTEM_CALL,
			 "Could not turn %s forwarding %s: SetIpStatisticsEx() error %lu%s",
			 family == AF_INET ? "IPv4" : "IPv6", on ? "on" : "off", rc,
			 rc == ERROR_ACCESS_DENIED ? " (zebra must run as Administrator)" : "");

	return win32_forwarding(family);
}

int ipforward(void)
{
	return win32_forwarding(AF_INET);
}

int ipforward_on(void)
{
	return win32_forwarding_set(AF_INET, true);
}

int ipforward_off(void)
{
	return win32_forwarding_set(AF_INET, false);
}

int ipforward_ipv6(void)
{
	return win32_forwarding(AF_INET6);
}

int ipforward_ipv6_on(void)
{
	return win32_forwarding_set(AF_INET6, true);
}

int ipforward_ipv6_off(void)
{
	return win32_forwarding_set(AF_INET6, false);
}

#endif /* _WIN32 */
