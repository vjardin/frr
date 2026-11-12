// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * vtysh on Windows
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#ifdef _WIN32

#define VOID void
#define CONST const
#define IN
#define OUT
#define OPTIONAL
#include <iphlpapi.h>
#undef VOID
#undef CONST
#undef IN
#undef OUT
#undef OPTIONAL
#include <process.h>
#include <readline/readline.h>

#include "lib/log.h"
#include "vtysh/vtysh.h"

/* ^Z leaves the configuration: on POSIX systems the terminal turns it into
 * SIGTSTP, which vtysh handles. The Windows console has it as a key, so it
 * is bound to raising the signal for that same handler.
 */
static int vtysh_win32_ctrl_z(int count, int key)
{
	kill(getpid(), SIGTSTP);
	return 0;
}

FRR_CONSTRUCTOR_DEFAULT(vtysh_win32_init);
static void vtysh_win32_init(void)
{
	rl_bind_key('Z' - '@', vtysh_win32_ctrl_z);

	/* readline would put its own handlers in front of the C runtime's
	 * default ones, which end the process: ^C is for vtysh's SIGINT
	 * handler alone, which starts a new line
	 */
	rl_catch_signals = 0;
}

static int addr_family(const char *s)
{
	struct in_addr a4;

	if (strchr(s, ':'))
		return AF_INET6;
	if (inet_pton(AF_INET, s, &a4) == 1)
		return AF_INET;
	return AF_UNSPEC;
}

static bool if_source_addr(const char *ifname, int family, char *buf, size_t len)
{
	IP_ADAPTER_ADDRESSES *list = NULL, *a;
	IP_ADAPTER_UNICAST_ADDRESS *u, *found = NULL;
	ULONG size = 16384, ret, ifindex = if_nametoindex(ifname);
	const void *addr;

	if (!ifindex)
		return false;
	for (int tries = 0; tries < 3; tries++) {
		list = malloc(size);
		if (!list)
			return false;
		ret = GetAdaptersAddresses(family,
					   GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
						   GAA_FLAG_SKIP_DNS_SERVER,
					   NULL, list, &size);
		if (ret != ERROR_BUFFER_OVERFLOW)
			break;
		free(list);
		list = NULL;
	}
	if (!list || ret != ERROR_SUCCESS) {
		free(list);
		return false;
	}

	for (a = list; a && !found; a = a->Next) {
		if ((family == AF_INET ? a->IfIndex : a->Ipv6IfIndex) != ifindex)
			continue;
		for (u = a->FirstUnicastAddress; u; u = u->Next) {
			const struct sockaddr *sa = u->Address.lpSockaddr;

			if (sa->sa_family != family)
				continue;
			if (!found)
				found = u;
			if (family == AF_INET ||
			    !IN6_IS_ADDR_LINKLOCAL(&((const struct sockaddr_in6 *)sa)->sin6_addr)) {
				found = u;
				break;
			}
		}
	}

	if (found) {
		const struct sockaddr *sa = found->Address.lpSockaddr;

		addr = family == AF_INET
			       ? (const void *)&((const struct sockaddr_in *)sa)->sin_addr
			       : (const void *)&((const struct sockaddr_in6 *)sa)->sin6_addr;
		found = inet_ntop(family, addr, buf, len) ? found : NULL;
	}
	free(list);
	return found != NULL;
}

bool vtysh_win32_execute(const char *command, char *argv[])
{
	bool ping = !strcmp(command, "ping") || !strcmp(command, "ping6");
	bool v6 = !strcmp(command, "ping6") || !strcmp(command, "traceroute6");
	const char *dst, *source = NULL, *count = NULL;
	const char *args[12];
	char srcbuf[INET6_ADDRSTRLEN], path[MAX_PATH];
	bool dontfragment = false;
	int family, n = 0;
	UINT len;

	if (!ping && strcmp(command, "traceroute") && strcmp(command, "traceroute6"))
		return false;
	if (!argv[0] || !argv[1])
		return false;

	dst = argv[1];
	for (int i = 2; argv[i]; i++) {
		if (!strcmp(argv[i], "-I") && argv[i + 1])
			source = argv[++i];
		else if (!strcmp(argv[i], "-c") && argv[i + 1])
			count = argv[++i];
		else if (!strcmp(argv[i], "-M") && argv[i + 1])
			dontfragment = !strcmp(argv[++i], "do");
		else if (!strcmp(argv[i], "-D"))
			dontfragment = true;
	}

	family = v6 ? AF_INET6 : AF_UNSPEC;
	if (family == AF_UNSPEC && source)
		family = addr_family(source);
	if (family == AF_UNSPEC)
		family = addr_family(dst);

	if (source && addr_family(source) == AF_UNSPEC) {
		if (family == AF_UNSPEC)
			family = AF_INET;
		if (!if_source_addr(source, family, srcbuf, sizeof(srcbuf))) {
			fprintf(stderr, "%% No %s address on interface %s\n",
				family == AF_INET ? "IPv4" : "IPv6", source);
			return true;
		}
		source = srcbuf;
	}

	args[n++] = ping ? "ping" : "tracert";
	if (count) {
		args[n++] = "/n";
		args[n++] = count;
	}
	if (dontfragment && family != AF_INET6)
		args[n++] = "/f";
	if (source) {
		args[n++] = "/S";
		args[n++] = source;
	}
	if (family == AF_INET)
		args[n++] = "/4";
	else if (family == AF_INET6)
		args[n++] = "/6";
	args[n++] = dst;
	args[n] = NULL;

	len = GetSystemDirectoryA(path, sizeof(path));
	if (!len || len >= sizeof(path) ||
	    strlcat(path, ping ? "\\PING.EXE" : "\\TRACERT.EXE", sizeof(path)) >= sizeof(path)) {
		fprintf(stderr, "Can't execute %s: no system directory\n", args[0]);
		return true;
	}
	if (_spawnv(_P_WAIT, path, args) == -1)
		fprintf(stderr, "Can't execute %s: %s\n", path, safe_strerror(errno));
	return true;
}

#endif /* _WIN32 */
