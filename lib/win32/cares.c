// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * c-ares sockets as FRR descriptors on Windows (see cares_win32.h).
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#include <ares.h>

#include "lib/log.h"
#include "lib/lib_errors.h"

static ares_socket_t frr_ares_socket(int domain, int type, int protocol, void *user_data)
{
	int fd = socket(domain, type | SOCK_NONBLOCK, protocol);

	(void)user_data;
	return fd < 0 ? ARES_SOCKET_BAD : (ares_socket_t)fd;
}

static int frr_ares_close(ares_socket_t sock, void *user_data)
{
	(void)user_data;
	return close((int)sock);
}

static int frr_ares_setsockopt(ares_socket_t sock, ares_socket_opt_t opt, const void *val,
			       ares_socklen_t val_size, void *user_data)
{
	(void)user_data;
	switch (opt) {
	case ARES_SOCKET_OPT_SENDBUF_SIZE:
		return setsockopt((int)sock, SOL_SOCKET, SO_SNDBUF, val, val_size);
	case ARES_SOCKET_OPT_RECVBUF_SIZE:
		return setsockopt((int)sock, SOL_SOCKET, SO_RCVBUF, val, val_size);
	default:
		errno = ENOSYS;
		WSASetLastError(ENOSYS);
		return -1;
	}
}

static int frr_ares_connect(ares_socket_t sock, const struct sockaddr *address,
			    ares_socklen_t address_len, unsigned int flags, void *user_data)
{
	(void)flags;
	(void)user_data;
	return connect((int)sock, address, address_len);
}

static ares_ssize_t frr_ares_recvfrom(ares_socket_t sock, void *buffer, size_t length, int flags,
				      struct sockaddr *address, ares_socklen_t *address_len,
				      void *user_data)
{
	(void)user_data;
	return recvfrom((int)sock, buffer, length, flags, address, address_len);
}

static ares_ssize_t frr_ares_sendto(ares_socket_t sock, const void *buffer, size_t length,
				    int flags, const struct sockaddr *address,
				    ares_socklen_t address_len, void *user_data)
{
	(void)user_data;
	if (!address)
		return send((int)sock, buffer, length, flags);
	return sendto((int)sock, buffer, length, flags, address, address_len);
}

static int frr_ares_getsockname(ares_socket_t sock, struct sockaddr *address,
				ares_socklen_t *address_len, void *user_data)
{
	(void)user_data;
	return getsockname((int)sock, address, address_len);
}

static int frr_ares_bind(ares_socket_t sock, unsigned int flags, const struct sockaddr *address,
			 socklen_t address_len, void *user_data)
{
	(void)flags;
	(void)user_data;
	return bind((int)sock, address, address_len);
}

static unsigned int frr_ares_if_nametoindex(const char *ifname, void *user_data)
{
	(void)user_data;
	return if_nametoindex(ifname);
}

static const char *frr_ares_if_indextoname(unsigned int ifindex, char *ifname_buf,
					   size_t ifname_buf_len, void *user_data)
{
	(void)user_data;
	if (ifname_buf_len < IF_NAMESIZE)
		return NULL;
	return if_indextoname(ifindex, ifname_buf);
}

static const struct ares_socket_functions_ex frr_ares_functions = {
	.version = 1,
	.flags = ARES_SOCKFUNC_FLAG_NONBLOCKING,
	.asocket = frr_ares_socket,
	.aclose = frr_ares_close,
	.asetsockopt = frr_ares_setsockopt,
	.aconnect = frr_ares_connect,
	.arecvfrom = frr_ares_recvfrom,
	.asendto = frr_ares_sendto,
	.agetsockname = frr_ares_getsockname,
	.abind = frr_ares_bind,
	.aif_nametoindex = frr_ares_if_nametoindex,
	.aif_indextoname = frr_ares_if_indextoname,
};

int frr_win_ares_init_options(ares_channel_t **channelptr, const struct ares_options *options,
			      int optmask)
{
	static bool library_ready;
	int ret;

	/* required on Windows, which FRR does not do elsewhere */
	if (!library_ready) {
		ret = ares_library_init(ARES_LIB_INIT_ALL);
		if (ret != ARES_SUCCESS) {
			flog_err(EC_LIB_RESOLVER, "c-ares: ares_library_init() failed: %s",
				 ares_strerror(ret));
			return ret;
		}
		library_ready = true;
	}

	ret = ares_init_options(channelptr, options, optmask);
	if (ret != ARES_SUCCESS) {
		flog_err(EC_LIB_RESOLVER, "c-ares: ares_init_options() failed: %s",
			 ares_strerror(ret));
		return ret;
	}

	ret = ares_set_socket_functions_ex(*channelptr, &frr_ares_functions, NULL);
	if (ret != ARES_SUCCESS) {
		flog_err(EC_LIB_RESOLVER, "c-ares: ares_set_socket_functions_ex() failed: %s",
			 ares_strerror(ret));
		ares_destroy(*channelptr);
		*channelptr = NULL;
	}
	return ret;
}
