// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows (MSVC) implementation of the POSIX subset used by FRR.
 *
 * Built with FRR_WIN32_COMPAT_IMPL defined, so the POSIX name redirections
 * from frr_win32.h are not active here and the real CRT / Winsock functions
 * can be called.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <net/if.h>
#include <netinet/icmp6.h>
#include <syslog.h>
#include <dlfcn.h>
#include <getopt.h>
#include <libgen.h>
#include <pthread.h>
#include <pwd.h>
#include <crtdbg.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")

static inline int clamp_int(size_t len)
{
	return len > INT_MAX ? INT_MAX : (int)len;
}

static int wsa_errno(int w)
{
	switch (w) {
	case WSAEWOULDBLOCK:
		return EWOULDBLOCK;
	case WSAEINPROGRESS:
		return EINPROGRESS;
	case WSAEALREADY:
		return EALREADY;
	case WSAENOTSOCK:
		return ENOTSOCK;
	case WSAEDESTADDRREQ:
		return EDESTADDRREQ;
	case WSAEMSGSIZE:
		return EMSGSIZE;
	case WSAEPROTOTYPE:
		return EPROTOTYPE;
	case WSAENOPROTOOPT:
		return ENOPROTOOPT;
	case WSAEPROTONOSUPPORT:
		return EPROTONOSUPPORT;
	case WSAESOCKTNOSUPPORT:
		return ESOCKTNOSUPPORT;
	case WSAEOPNOTSUPP:
		return EOPNOTSUPP;
	case WSAEPFNOSUPPORT:
		return EPFNOSUPPORT;
	case WSAEAFNOSUPPORT:
		return EAFNOSUPPORT;
	case WSAEADDRINUSE:
		return EADDRINUSE;
	case WSAEADDRNOTAVAIL:
		return EADDRNOTAVAIL;
	case WSAENETDOWN:
		return ENETDOWN;
	case WSAENETUNREACH:
		return ENETUNREACH;
	case WSAENETRESET:
		return ENETRESET;
	case WSAECONNABORTED:
		return ECONNABORTED;
	case WSAECONNRESET:
		return ECONNRESET;
	case WSAENOBUFS:
		return ENOBUFS;
	case WSAEISCONN:
		return EISCONN;
	case WSAENOTCONN:
		return ENOTCONN;
	case WSAESHUTDOWN:
		return EPIPE;
	case WSAETIMEDOUT:
		return ETIMEDOUT;
	case WSAECONNREFUSED:
		return ECONNREFUSED;
	case WSAELOOP:
		return ELOOP;
	case WSAENAMETOOLONG:
		return ENAMETOOLONG;
	case WSAEHOSTDOWN:
		return EHOSTDOWN;
	case WSAEHOSTUNREACH:
		return EHOSTUNREACH;
	case WSAEINTR:
		return EINTR;
	case WSAEBADF:
		return EBADF;
	case WSAEACCES:
		return EACCES;
	case WSAEFAULT:
		return EFAULT;
	case WSAEINVAL:
		return EINVAL;
	case WSAEMFILE:
		return EMFILE;
	default:
		return EIO;
	}
}

static int sock_fail(void)
{
	errno = wsa_errno(WSAGetLastError());
	return -1;
}

static int win_errno(DWORD e)
{
	switch (e) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_INVALID_DRIVE:
		return ENOENT;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
		return EACCES;
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		return EEXIST;
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
		return ENOMEM;
	case ERROR_INVALID_HANDLE:
		return EBADF;
	case ERROR_DIR_NOT_EMPTY:
		return ENOTEMPTY;
	case ERROR_NOT_SAME_DEVICE:
		return EXDEV;
	default:
		return EINVAL;
	}
}

static const char *const posix_errstr[] = {
	[EADDRINUSE - 100] = "Address already in use",
	[EADDRNOTAVAIL - 100] = "Cannot assign requested address",
	[EAFNOSUPPORT - 100] = "Address family not supported by protocol",
	[EALREADY - 100] = "Operation already in progress",
	[EBADMSG - 100] = "Bad message",
	[ECANCELED - 100] = "Operation canceled",
	[ECONNABORTED - 100] = "Software caused connection abort",
	[ECONNREFUSED - 100] = "Connection refused",
	[ECONNRESET - 100] = "Connection reset by peer",
	[EDESTADDRREQ - 100] = "Destination address required",
	[EHOSTUNREACH - 100] = "No route to host",
	[EIDRM - 100] = "Identifier removed",
	[EINPROGRESS - 100] = "Operation now in progress",
	[EISCONN - 100] = "Transport endpoint is already connected",
	[ELOOP - 100] = "Too many levels of symbolic links",
	[EMSGSIZE - 100] = "Message too long",
	[ENETDOWN - 100] = "Network is down",
	[ENETRESET - 100] = "Network dropped connection on reset",
	[ENETUNREACH - 100] = "Network is unreachable",
	[ENOBUFS - 100] = "No buffer space available",
	[ENODATA - 100] = "No data available",
	[ENOLINK - 100] = "Link has been severed",
	[ENOMSG - 100] = "No message of desired type",
	[ENOPROTOOPT - 100] = "Protocol not available",
	[ENOSR - 100] = "Out of streams resources",
	[ENOSTR - 100] = "Device not a stream",
	[ENOTCONN - 100] = "Transport endpoint is not connected",
	[ENOTRECOVERABLE - 100] = "State not recoverable",
	[ENOTSOCK - 100] = "Socket operation on non-socket",
	[EOPNOTSUPP - 100] = "Operation not supported",
	[EOTHER - 100] = "Other error",
	[EOVERFLOW - 100] = "Value too large for defined data type",
	[EOWNERDEAD - 100] = "Owner died",
	[EPROTO - 100] = "Protocol error",
	[EPROTONOSUPPORT - 100] = "Protocol not supported",
	[EPROTOTYPE - 100] = "Protocol wrong type for socket",
	[ETIME - 100] = "Timer expired",
	[ETIMEDOUT - 100] = "Connection timed out",
	[ETXTBSY - 100] = "Text file busy",
	[EWOULDBLOCK - 100] = "Resource temporarily unavailable",
	[ESHUTDOWN - 100] = "Cannot send after transport endpoint shutdown",
	[EHOSTDOWN - 100] = "Host is down",
	[ESOCKTNOSUPPORT - 100] = "Socket type not supported",
	[EPFNOSUPPORT - 100] = "Protocol family not supported",
};

char *frr_win_strerror(int errnum)
{
	if (errnum >= 100 && errnum - 100 < (int)(sizeof(posix_errstr) /
						  sizeof(posix_errstr[0])) &&
	    posix_errstr[errnum - 100])
		return (char *)posix_errstr[errnum - 100];
	return (strerror)(errnum);
}

int frr_win_strerror_r(int errnum, char *buf, size_t buflen)
{
	if (!buflen)
		return ERANGE;
	strncpy_s(buf, buflen, frr_win_strerror(errnum), _TRUNCATE);
	return 0;
}

#define FD_TABLE_SIZE 8192

static SOCKET fd_sock[FD_TABLE_SIZE];
static bool fd_nonblock[FD_TABLE_SIZE];

static bool fd_icmp6[FD_TABLE_SIZE];
static struct icmp6_filter *fd_icmp6_filter[FD_TABLE_SIZE];

static void rs_helper_del(int fd, unsigned int ifindex);

SOCKET frr_win_fd2sock(int fd)
{
	if (fd < 0 || fd >= FD_TABLE_SIZE || !fd_sock[fd])
		return INVALID_SOCKET;
	return fd_sock[fd];
}

int frr_win_is_socket(int fd)
{
	return frr_win_fd2sock(fd) != INVALID_SOCKET;
}

int frr_win_sock2fd(SOCKET s)
{
	for (int fd = 0; fd < FD_TABLE_SIZE; fd++)
		if (fd_sock[fd] == s)
			return fd;
	return -1;
}

static int fd_alloc(SOCKET s)
{
	int fd;

	if (s == INVALID_SOCKET)
		return sock_fail();

	fd = _open("NUL", _O_RDWR | _O_BINARY | _O_NOINHERIT);
	if (fd < 0 || fd >= FD_TABLE_SIZE) {
		if (fd >= 0)
			_close(fd);
		closesocket(s);
		errno = EMFILE;
		return -1;
	}
	SetHandleInformation((HANDLE)s, HANDLE_FLAG_INHERIT, 0);
	fd_nonblock[fd] = false;
	fd_sock[fd] = s;
	return fd;
}

static void fd_release(int fd)
{
	if (fd_icmp6[fd])
		rs_helper_del(fd, 0);
	fd_sock[fd] = 0;
	fd_nonblock[fd] = false;
	fd_icmp6[fd] = false;
	free(fd_icmp6_filter[fd]);
	fd_icmp6_filter[fd] = NULL;
}

#define SOCK_OR_FAIL(var, fd)                                                  \
	SOCKET var = frr_win_fd2sock(fd);                                      \
	do {                                                                   \
		if (var == INVALID_SOCKET) {                                   \
			errno = ENOTSOCK;                                      \
			return -1;                                             \
		}                                                              \
	} while (0)

static int set_nonblock(int fd, SOCKET s, bool nb)
{
	u_long arg = nb;

	if (ioctlsocket(s, FIONBIO, &arg))
		return sock_fail();
	fd_nonblock[fd] = nb;
	return 0;
}

int frr_win_socket(int domain, int type, int protocol)
{
	int flags = type & (SOCK_NONBLOCK | SOCK_CLOEXEC);
	int fd;

	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
	fd = fd_alloc(WSASocketW(domain, type, protocol, NULL, 0, 0));
	if (fd >= 0 && (flags & SOCK_NONBLOCK))
		set_nonblock(fd, fd_sock[fd], true);
	if (fd >= 0)
		fd_icmp6[fd] = domain == AF_INET6 && type == SOCK_RAW && protocol == IPPROTO_ICMPV6;
	if (fd >= 0 && type == SOCK_RAW &&
	    (domain == AF_INET || domain == AF_INET6)) {
		struct sockaddr_storage any = { .ss_family = (ADDRESS_FAMILY)domain };
		DWORD off = 0, on = 1;

		if (domain == AF_INET6)
			setsockopt(fd_sock[fd], IPPROTO_IPV6, IPV6_V6ONLY,
				   (char *)&on, sizeof(on));
		bind(fd_sock[fd], (struct sockaddr *)&any,
		     domain == AF_INET ? sizeof(struct sockaddr_in)
				       : sizeof(struct sockaddr_in6));
		if (domain == AF_INET)
			setsockopt(fd_sock[fd], IPPROTO_IP, IP_MULTICAST_LOOP,
				   (char *)&off, sizeof(off));
		else
			setsockopt(fd_sock[fd], IPPROTO_IPV6,
				   IPV6_MULTICAST_LOOP, (char *)&off,
				   sizeof(off));
	}
	return fd;
}

int frr_win_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
	SOCK_OR_FAIL(s, fd);
	SOCKET n = accept(s, addr, len);
	int nfd;

	if (n == INVALID_SOCKET)
		return sock_fail();
	nfd = fd_alloc(n);
	if (nfd >= 0)
		fd_nonblock[nfd] = fd_nonblock[fd];
	return nfd;
}

int frr_win_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
	SOCK_OR_FAIL(s, fd);
	return bind(s, addr, len) ? sock_fail() : 0;
}

static void connect_bind_local(SOCKET s, const struct sockaddr *addr,
			       socklen_t len)
{
	SOCKADDR_STORAGE local;
	int llen = sizeof(local);
	DWORD got = 0;

	if (addr->sa_family != AF_INET && addr->sa_family != AF_INET6)
		return;
	/* bound already (update-source & co.) */
	if (getsockname(s, (struct sockaddr *)&local, &llen) == 0)
		return;
	if (WSAIoctl(s, SIO_ROUTING_INTERFACE_QUERY, (void *)addr, len, &local,
		     sizeof(local), &got, NULL, NULL))
		return;
	if (local.ss_family == AF_INET)
		((struct sockaddr_in *)&local)->sin_port = 0;
	else
		((struct sockaddr_in6 *)&local)->sin6_port = 0;
	bind(s, (struct sockaddr *)&local, (int)got);
}

int frr_win_connect(int fd, const struct sockaddr *addr, socklen_t len)
{
	SOCK_OR_FAIL(s, fd);

	connect_bind_local(s, addr, len);
	if (connect(s, addr, len) == 0)
		return 0;
	if (WSAGetLastError() == WSAEWOULDBLOCK) {
		errno = EINPROGRESS;
		return -1;
	}
	return sock_fail();
}

int frr_win_listen(int fd, int backlog)
{
	SOCK_OR_FAIL(s, fd);
	return listen(s, backlog) ? sock_fail() : 0;
}

int frr_win_getsockopt(int fd, int level, int opt, void *val, socklen_t *len)
{
	SOCK_OR_FAIL(s, fd);
	return getsockopt(s, level, opt, val, len) ? sock_fail() : 0;
}

/*
 * Router solicitations for raw ICMPv6 sockets.
 *
 * Windows keeps away from raw sockets the ICMPv6 messages that its stack
 * handles itself: echo requests, and the router solicitations that a host
 * must discard (RFC 4861, 6.1.1). So zebra, which sends router
 * advertisements, never hears a solicitation, whether forwarding is on or
 * not. It does as long as a socket on the interface has SIO_RCVALL set,
 * even in RCVALL_IPLEVEL mode, which leaves the adapter out of promiscuous
 * mode: something that Microsoft does not document!
 */
#define RS_HELPER_MAX 64

#define RS_HELPER_TRIES 60
#define RS_HELPER_RETRY_MSEC 1000

/* from <mstcpip.h>, which collides with the FRR shims */
#ifndef SIO_RCVALL
#define SIO_RCVALL _WSAIOW(IOC_VENDOR, 1)
#endif
#define RS_RCVALL_IPLEVEL 3

static struct {
	bool used;
	int fd;
	unsigned int ifindex;
	/* 0 while the interface has no usable link-local address */
	SOCKET s;
	int tries;
} rs_helper[RS_HELPER_MAX];
static SRWLOCK rs_helper_lock = SRWLOCK_INIT;
static PTP_TIMER rs_helper_timer;

static bool rs_helper_linklocal(unsigned int ifindex, struct sockaddr_in6 *ll)
{
	SOCKET_ADDRESS_LIST *list = NULL;
	DWORD size = 4096, ret;
	bool found = false;
	SOCKET q;
	int i;

	q = WSASocketW(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
	if (q == INVALID_SOCKET)
		return false;

	for (i = 0; i < 4; i++, size *= 4) {
		free(list);
		list = malloc(size);
		if (!list)
			break;
		if (!WSAIoctl(q, SIO_ADDRESS_LIST_QUERY, NULL, 0, list, size, &ret, NULL, NULL))
			break;
		if (WSAGetLastError() != WSAEFAULT) {
			free(list);
			list = NULL;
			break;
		}
	}

	for (i = 0; list && i < list->iAddressCount && !found; i++) {
		const struct sockaddr_in6 *a = (struct sockaddr_in6 *)list->Address[i].lpSockaddr;

		if (a->sin6_family == AF_INET6 && IN6_IS_ADDR_LINKLOCAL(&a->sin6_addr) &&
		    a->sin6_scope_id == ifindex) {
			*ll = *a;
			found = true;
		}
	}

	free(list);
	closesocket(q);
	return found;
}

static SOCKET rs_helper_open(unsigned int ifindex)
{
	DWORD mode = RS_RCVALL_IPLEVEL, zero = 0, ret;
	struct sockaddr_in6 ll;
	SOCKET s;

	if (!rs_helper_linklocal(ifindex, &ll))
		return INVALID_SOCKET;

	s = WSASocketW(AF_INET6, SOCK_RAW, IPPROTO_IPV6, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
	if (s == INVALID_SOCKET)
		return s;

	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&zero, sizeof(zero)) ||
	    bind(s, (struct sockaddr *)&ll, sizeof(ll)) ||
	    WSAIoctl(s, SIO_RCVALL, &mode, sizeof(mode), NULL, 0, &ret, NULL, NULL)) {
		closesocket(s);
		return INVALID_SOCKET;
	}

	return s;
}

static void rs_helper_arm(void)
{
	ULARGE_INTEGER t;
	FILETIME due;

	t.QuadPart = (ULONGLONG)(-(LONGLONG)RS_HELPER_RETRY_MSEC * 10000);
	due.dwLowDateTime = t.LowPart;
	due.dwHighDateTime = t.HighPart;
	SetThreadpoolTimer(rs_helper_timer, &due, 0, 0);
}

static void CALLBACK rs_helper_retry(PTP_CALLBACK_INSTANCE instance, PVOID ctx, PTP_TIMER timer)
{
	bool pending = false;
	int i;

	AcquireSRWLockExclusive(&rs_helper_lock);
	for (i = 0; i < RS_HELPER_MAX; i++) {
		SOCKET s;

		if (!rs_helper[i].used || rs_helper[i].s)
			continue;

		s = rs_helper_open(rs_helper[i].ifindex);
		if (s != INVALID_SOCKET)
			rs_helper[i].s = s;
		else if (--rs_helper[i].tries > 0)
			pending = true;
		else
			rs_helper[i].used = false;
	}
	if (pending)
		rs_helper_arm();
	ReleaseSRWLockExclusive(&rs_helper_lock);
}

static void rs_helper_del(int fd, unsigned int ifindex)
{
	int i;

	AcquireSRWLockExclusive(&rs_helper_lock);
	for (i = 0; i < RS_HELPER_MAX; i++) {
		if (!rs_helper[i].used || rs_helper[i].fd != fd ||
		    (ifindex && rs_helper[i].ifindex != ifindex))
			continue;

		if (rs_helper[i].s)
			closesocket(rs_helper[i].s);
		rs_helper[i].used = false;
		rs_helper[i].s = 0;
	}
	ReleaseSRWLockExclusive(&rs_helper_lock);
}

static void rs_helper_add(int fd, unsigned int ifindex)
{
	SOCKET s;
	int i;

	rs_helper_del(fd, ifindex);

	s = rs_helper_open(ifindex);

	AcquireSRWLockExclusive(&rs_helper_lock);
	for (i = 0; i < RS_HELPER_MAX && rs_helper[i].used; i++)
		;
	if (i < RS_HELPER_MAX) {
		rs_helper[i].used = true;
		rs_helper[i].fd = fd;
		rs_helper[i].ifindex = ifindex;
		rs_helper[i].s = s != INVALID_SOCKET ? s : 0;
		rs_helper[i].tries = RS_HELPER_TRIES;
		s = INVALID_SOCKET;

		if (!rs_helper[i].s) {
			if (!rs_helper_timer)
				rs_helper_timer = CreateThreadpoolTimer(rs_helper_retry, NULL, NULL);
			if (rs_helper_timer)
				rs_helper_arm();
		}
	}
	ReleaseSRWLockExclusive(&rs_helper_lock);

	if (s != INVALID_SOCKET)
		closesocket(s);
}

static void rs_helper_update(int fd, int opt, const void *val, socklen_t len)
{
	static const struct in6_addr allrouters = { { { 0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
							 0, 0, 0, 0x02 } } };
	const struct ipv6_mreq *mreq = val;

	if (!fd_icmp6[fd] || len < (socklen_t)sizeof(*mreq) ||
	    memcmp(&mreq->ipv6mr_multiaddr, &allrouters, sizeof(allrouters)) ||
	    !mreq->ipv6mr_interface)
		return;

	if (opt == IPV6_ADD_MEMBERSHIP)
		rs_helper_add(fd, mreq->ipv6mr_interface);
	else
		rs_helper_del(fd, mreq->ipv6mr_interface);
}

int frr_win_setsockopt(int fd, int level, int opt, const void *val,
		       socklen_t len)
{
	SOCK_OR_FAIL(s, fd);

	if (level == SOL_SOCKET && (opt == SO_REUSEADDR || opt == SO_REUSEPORT))
		return 0;
	/* XXX IP_TOS and IPV6_TCLASS are refused (EACCES) unless a QoS policy is
	 * set up: nothing useful to do or to report per socket
	 */
	if (level == IPPROTO_IP && opt == IP_TOS)
		return 0;
#ifdef IPV6_TCLASS
	if (level == IPPROTO_IPV6 && opt == IPV6_TCLASS)
		return 0;
#endif
	if (level == IPPROTO_ICMPV6 && opt == ICMP6_FILTER) {
		if (len != sizeof(struct icmp6_filter)) {
			errno = EINVAL;
			return -1;
		}
		if (!fd_icmp6_filter[fd])
			fd_icmp6_filter[fd] = malloc(sizeof(struct icmp6_filter));
		if (!fd_icmp6_filter[fd]) {
			errno = ENOMEM;
			return -1;
		}
		memcpy(fd_icmp6_filter[fd], val, sizeof(struct icmp6_filter));
		return 0;
	}
	if (!setsockopt(s, level, opt, val, len)) {
		if (level == IPPROTO_IPV6 &&
		    (opt == IPV6_ADD_MEMBERSHIP || opt == IPV6_DROP_MEMBERSHIP))
			rs_helper_update(fd, opt, val, len);
		return 0;
	}
	if (WSAGetLastError() == WSAEINVAL &&
	    ((level == IPPROTO_IP && (opt == IP_ADD_MEMBERSHIP || opt == MCAST_JOIN_GROUP)) ||
	     (level == IPPROTO_IPV6 &&
	      (opt == IPV6_ADD_MEMBERSHIP || opt == MCAST_JOIN_GROUP)))) {
		if (level == IPPROTO_IPV6 && opt == IPV6_ADD_MEMBERSHIP)
			rs_helper_update(fd, opt, val, len);
		errno = EADDRINUSE;
		return -1;
	}
	return sock_fail();
}

int frr_win_getsockname(int fd, struct sockaddr *addr, socklen_t *len)
{
	SOCK_OR_FAIL(s, fd);
	return getsockname(s, addr, len) ? sock_fail() : 0;
}

int frr_win_getpeername(int fd, struct sockaddr *addr, socklen_t *len)
{
	SOCK_OR_FAIL(s, fd);
	return getpeername(s, addr, len) ? sock_fail() : 0;
}

int frr_win_shutdown(int fd, int how)
{
	SOCK_OR_FAIL(s, fd);
	return shutdown(s, how) ? sock_fail() : 0;
}

/* MSG_DONTWAIT does not exist on Windows: flip the socket */
static bool dontwait_begin(int fd, SOCKET s, int *flags)
{
	if (!(*flags & MSG_DONTWAIT))
		return false;
	*flags &= ~MSG_DONTWAIT;
	if (fd_nonblock[fd])
		return false;
	set_nonblock(fd, s, true);
	return true;
}

static void dontwait_end(int fd, SOCKET s, bool restore)
{
	int saved = errno;

	if (restore)
		set_nonblock(fd, s, false);
	errno = saved;
}

ssize_t frr_win_send(int fd, const void *buf, size_t len, int flags)
{
	SOCK_OR_FAIL(s, fd);
	bool r = dontwait_begin(fd, s, &flags);
	int n = send(s, buf, clamp_int(len), flags);
	ssize_t ret = n == SOCKET_ERROR ? sock_fail() : n;

	dontwait_end(fd, s, r);
	return ret;
}

static int sock_read_fail(SOCKET s)
{
	WSAPROTOCOL_INFOW info;
	int err = WSAGetLastError();
	int len = sizeof(info);

	if (err == WSAECONNRESET &&
	    !getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFOW, (char *)&info, &len) &&
	    info.iAddressFamily == AF_UNIX)
		return 0;
	errno = wsa_errno(err);
	return -1;
}

ssize_t frr_win_recv(int fd, void *buf, size_t len, int flags)
{
	SOCK_OR_FAIL(s, fd);
	bool r = dontwait_begin(fd, s, &flags);
	int n = recv(s, buf, clamp_int(len), flags);
	ssize_t ret = n == SOCKET_ERROR ? sock_read_fail(s) : n;

	dontwait_end(fd, s, r);
	return ret;
}

ssize_t frr_win_sendto(int fd, const void *buf, size_t len, int flags,
		       const struct sockaddr *to, socklen_t tolen)
{
	SOCK_OR_FAIL(s, fd);
	bool r = dontwait_begin(fd, s, &flags);
	int n = sendto(s, buf, clamp_int(len), flags, to, tolen);
	ssize_t ret = n == SOCKET_ERROR ? sock_fail() : n;

	dontwait_end(fd, s, r);
	return ret;
}

ssize_t frr_win_recvfrom(int fd, void *buf, size_t len, int flags,
			 struct sockaddr *from, socklen_t *fromlen)
{
	SOCK_OR_FAIL(s, fd);
	bool r = dontwait_begin(fd, s, &flags);
	int n = recvfrom(s, buf, clamp_int(len), flags, from, fromlen);
	ssize_t ret = n == SOCKET_ERROR ? sock_fail() : n;

	dontwait_end(fd, s, r);
	return ret;
}

#define IOV_STACK 16

static WSABUF *iov_to_wsabuf(const struct iovec *iov, size_t cnt,
			     WSABUF *stack)
{
	WSABUF *b = cnt <= IOV_STACK ? stack : malloc(cnt * sizeof(*b));

	if (!b)
		return NULL;
	for (size_t i = 0; i < cnt; i++) {
		b[i].buf = iov[i].iov_base;
		b[i].len = (ULONG)iov[i].iov_len;
	}
	return b;
}

static int sock_type(SOCKET s)
{
	int type = 0, len = sizeof(type);

	getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &len);
	return type;
}

static LPFN_WSARECVMSG wsarecvmsg_fn;

static bool sendmsg_pktinfo_ifindex(SOCKET s, const struct msghdr *msg,
				    struct sockaddr_in6 *dst)
{
	const struct cmsghdr *cmsg = msg->msg_control;
	const struct in6_pktinfo *pi;
	DWORD ifindex;

	if (!cmsg || msg->msg_controllen < CMSG_LEN(sizeof(*pi)) ||
	    msg->msg_controllen > CMSG_SPACE(sizeof(*pi)) ||
	    cmsg->cmsg_level != IPPROTO_IPV6 || cmsg->cmsg_type != IPV6_PKTINFO)
		return false;
	pi = (const struct in6_pktinfo *)CMSG_DATA(cmsg);
	if (!IN6_IS_ADDR_UNSPECIFIED(&pi->ipi6_addr) || !pi->ipi6_ifindex)
		return false;
	if (!msg->msg_name || msg->msg_namelen < sizeof(*dst))
		return false;

	memcpy(dst, msg->msg_name, sizeof(*dst));
	if (dst->sin6_family != AF_INET6)
		return false;

	ifindex = pi->ipi6_ifindex;
	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr)) {
		setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char *)&ifindex,
			   sizeof(ifindex));
	} else {
		setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_IF, (char *)&ifindex,
			   sizeof(ifindex));
		if (IN6_IS_ADDR_LINKLOCAL(&dst->sin6_addr) &&
		    !dst->sin6_scope_id)
			dst->sin6_scope_id = ifindex;
	}
	return true;
}

static bool sendmsg_hoplimit(SOCKET s, const struct msghdr *msg)
{
	const struct cmsghdr *cmsg = msg->msg_control;
	int hops;

	if (!cmsg || msg->msg_controllen < CMSG_LEN(sizeof(hops)) ||
	    msg->msg_controllen > CMSG_SPACE(sizeof(hops)) ||
	    cmsg->cmsg_level != IPPROTO_IPV6 || cmsg->cmsg_type != IPV6_HOPLIMIT)
		return false;
	memcpy(&hops, CMSG_DATA(cmsg), sizeof(hops));

	setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (char *)&hops, sizeof(hops));
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char *)&hops, sizeof(hops));
	return true;
}

/*
 * The sender duplicates the socket into the process at the other end,
 * which SIO_AF_UNIX_GETPEERPID gives, with WSADuplicateSocketW(), and sends
 * the WSAPROTOCOL_INFOW that it gets ahead of the data, behind a marker:
 * the record that stands for the control message. The receiver, when it
 * reads with room for a control message, takes the record out of the
 * stream and imports the socket with WSASocketW(), to give it as the
 * descriptor of an SCM_RIGHTS message, together with the data that
 * follows. The marker starts with 0xff 0xfe, which UTF-8 text, what goes
 * over these sockets, never has. Only sockets can be passed so.
 */
#define FD_PASS_MAGIC "\xff\xfe" "FD"
#define FD_PASS_MAGIC_LEN 4
#define FD_PASS_SEND_MSEC 10000

struct fd_pass_rec {
	char magic[FD_PASS_MAGIC_LEN];
	uint32_t len;
	WSAPROTOCOL_INFOW info;
};

/* the descriptor of an SCM_RIGHTS control message, or -1 */
static int fd_pass_cmsg(const struct msghdr *msg)
{
	struct cmsghdr *cm = CMSG_FIRSTHDR(msg);
	int pass_fd = -1;

	if (cm && cm->cmsg_level == SOL_SOCKET && cm->cmsg_type == SCM_RIGHTS &&
	    cm->cmsg_len >= CMSG_LEN(sizeof(int)))
		memcpy(&pass_fd, CMSG_DATA(cm), sizeof(int));
	return pass_fd;
}

static ssize_t fd_pass_send(SOCKET s, const struct msghdr *msg, int pass_fd)
{
	SOCKET ps = frr_win_fd2sock(pass_fd);
	struct fd_pass_rec rec = { .len = sizeof(WSAPROTOCOL_INFOW) };
	size_t datalen = 0, total, off = 0, i;
	ULONG peer = 0;
	DWORD n, waited = 0;
	char *buf;

	if (ps == INVALID_SOCKET) {
		errno = EBADF;
		return -1;
	}
	if (WSAIoctl(s, SIO_AF_UNIX_GETPEERPID, NULL, 0, &peer, sizeof(peer), &n, NULL, NULL) ||
	    WSADuplicateSocketW(ps, peer, &rec.info))
		return sock_fail();
	memcpy(rec.magic, FD_PASS_MAGIC, FD_PASS_MAGIC_LEN);

	for (i = 0; i < (size_t)msg->msg_iovlen; i++)
		datalen += msg->msg_iov[i].iov_len;
	total = sizeof(rec) + datalen;
	buf = malloc(total);
	if (!buf) {
		errno = ENOMEM;
		return -1;
	}
	memcpy(buf, &rec, sizeof(rec));
	for (i = 0, off = sizeof(rec); i < (size_t)msg->msg_iovlen; i++) {
		memcpy(buf + off, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		off += msg->msg_iov[i].iov_len;
	}

	/* all of it, a record cut short would be read as data */
	for (off = 0; off < total;) {
		int rc = send(s, buf + off, (int)(total - off), 0);
		WSAPOLLFD p = { .fd = s, .events = POLLWRNORM };

		if (rc != SOCKET_ERROR) {
			off += (size_t)rc;
			continue;
		}
		if (WSAGetLastError() != WSAEWOULDBLOCK || waited >= FD_PASS_SEND_MSEC) {
			free(buf);
			return sock_fail();
		}
		WSAPoll(&p, 1, 100);
		waited += 100;
	}
	free(buf);
	return (ssize_t)datalen;
}

static size_t fd_pass_find(const char *data, size_t len)
{
	size_t i, n;

	for (i = 0; i < len; i++) {
		n = len - i < FD_PASS_MAGIC_LEN ? len - i : FD_PASS_MAGIC_LEN;
		if (!memcmp(data + i, FD_PASS_MAGIC, n))
			return i;
	}
	return len;
}

static ssize_t fd_pass_scatter(SOCKET s, const struct msghdr *msg, char *tmp, size_t len)
{
	size_t off = 0, i, n;
	int rc;

	rc = recv(s, tmp, (int)len, 0);
	if (rc == SOCKET_ERROR)
		return sock_fail();
	for (i = 0; i < (size_t)msg->msg_iovlen && off < (size_t)rc; i++) {
		n = msg->msg_iov[i].iov_len;
		if (n > (size_t)rc - off)
			n = (size_t)rc - off;
		memcpy(msg->msg_iov[i].iov_base, tmp + off, n);
		off += n;
	}
	return (ssize_t)off;
}

static ssize_t fd_pass_recv(SOCKET s, struct msghdr *msg)
{
	struct cmsghdr *cm = msg->msg_control;
	struct fd_pass_rec rec;
	size_t total = 0, i, at;
	ssize_t got;
	SOCKET ns;
	char *tmp;
	int rc, nfd;

	for (i = 0; i < (size_t)msg->msg_iovlen; i++)
		total += msg->msg_iov[i].iov_len;
	if (!total)
		return -2;
	tmp = malloc(total + sizeof(rec));
	if (!tmp)
		return -2;

	rc = recv(s, tmp, (int)(total + sizeof(rec)), MSG_PEEK);
	if (rc == SOCKET_ERROR || rc == 0) {
		free(tmp);
		return -2;
	}
	at = fd_pass_find(tmp, (size_t)rc);

	/* data first, up to a record if one follows */
	if (at > 0) {
		got = fd_pass_scatter(s, msg, tmp, at < total ? at : total);
		msg->msg_controllen = 0;
		free(tmp);
		return got;
	}

	/* a record: all of it, sent in one go */
	rc = recv(s, (char *)&rec, sizeof(rec), MSG_WAITALL);
	if (rc != (int)sizeof(rec) || memcmp(rec.magic, FD_PASS_MAGIC, FD_PASS_MAGIC_LEN) ||
	    rec.len != sizeof(WSAPROTOCOL_INFOW)) {
		free(tmp);
		errno = EPROTO;
		return -1;
	}
	ns = WSASocketW(FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, FROM_PROTOCOL_INFO, &rec.info, 0,
			WSA_FLAG_OVERLAPPED);
	nfd = ns == INVALID_SOCKET ? -1 : fd_alloc(ns);

	memset(cm, 0, CMSG_SPACE(sizeof(int)));
	cm->cmsg_len = CMSG_LEN(sizeof(int));
	cm->cmsg_level = SOL_SOCKET;
	cm->cmsg_type = SCM_RIGHTS;
	memcpy(CMSG_DATA(cm), &nfd, sizeof(int));
	msg->msg_controllen = CMSG_SPACE(sizeof(int));

	/* and the data that came with it, up to another record */
	rc = recv(s, tmp, (int)total, MSG_PEEK);
	if (rc == SOCKET_ERROR || rc == 0) {
		free(tmp);
		return 0;
	}
	at = fd_pass_find(tmp, (size_t)rc);
	got = at ? fd_pass_scatter(s, msg, tmp, at) : 0;
	free(tmp);
	return got;
}

static int sock_family(SOCKET s)
{
	WSAPROTOCOL_INFOW pi;
	int len = sizeof(pi);

	if (getsockopt(s, SOL_SOCKET, SO_PROTOCOL_INFOW, (char *)&pi, &len))
		return AF_UNSPEC;
	return pi.iAddressFamily;
}

ssize_t frr_win_sendmsg(int fd, const struct msghdr *msg, int flags)
{
	SOCK_OR_FAIL(s, fd);
	WSABUF stack[IOV_STACK], *bufs;
	struct sockaddr_in6 dst6;
	DWORD sent = 0;
	bool r;
	int rc;

	if (msg->msg_control && msg->msg_controllen && sock_type(s) == SOCK_STREAM &&
	    sock_family(s) == AF_UNIX) {
		int pass_fd = fd_pass_cmsg(msg);

		if (pass_fd >= 0)
			return fd_pass_send(s, msg, pass_fd);
	}

	bufs = iov_to_wsabuf(msg->msg_iov, msg->msg_iovlen, stack);
	if (!bufs) {
		errno = ENOMEM;
		return -1;
	}
	r = dontwait_begin(fd, s, &flags);

	if (sock_type(s) == SOCK_STREAM) {
		rc = WSASend(s, bufs, (DWORD)msg->msg_iovlen, &sent, flags,
			     NULL, NULL);
	} else {
		WSAMSG wm = {
			.name = msg->msg_name,
			.namelen = msg->msg_namelen,
			.lpBuffers = bufs,
			.dwBufferCount = (ULONG)msg->msg_iovlen,
			.Control.buf = msg->msg_control,
			.Control.len = (ULONG)msg->msg_controllen,
			.dwFlags = 0,
		};

		if (sendmsg_pktinfo_ifindex(s, msg, &dst6)) {
			wm.name = (struct sockaddr *)&dst6;
			wm.namelen = sizeof(dst6);
			wm.Control.buf = NULL;
			wm.Control.len = 0;
		} else if (sendmsg_hoplimit(s, msg)) {
			wm.Control.buf = NULL;
			wm.Control.len = 0;
		}
		rc = WSASendMsg(s, &wm, flags, &sent, NULL, NULL);
	}
	if (rc == SOCKET_ERROR)
		sock_fail();
	dontwait_end(fd, s, r);
	if (bufs != stack)
		free(bufs);
	return rc == SOCKET_ERROR ? -1 : (ssize_t)sent;
}

ssize_t frr_win_recvmsg(int fd, struct msghdr *msg, int flags)
{
	SOCK_OR_FAIL(s, fd);
	WSABUF stack[IOV_STACK], *bufs;
	DWORD got = 0, wflags = flags & ~MSG_DONTWAIT;
	bool r;
	int rc;

	if (msg->msg_control && msg->msg_controllen >= CMSG_SPACE(sizeof(int)) &&
	    !(flags & (MSG_PEEK | MSG_DONTWAIT)) && sock_type(s) == SOCK_STREAM &&
	    sock_family(s) == AF_UNIX) {
		ssize_t passed = fd_pass_recv(s, msg);

		if (passed != -2)
			return passed;
	}

	bufs = iov_to_wsabuf(msg->msg_iov, msg->msg_iovlen, stack);
	if (!bufs) {
		errno = ENOMEM;
		return -1;
	}
	r = dontwait_begin(fd, s, &flags);

	if (sock_type(s) == SOCK_STREAM || !wsarecvmsg_fn) {
		rc = WSARecvFrom(s, bufs, (DWORD)msg->msg_iovlen, &got, &wflags,
				 msg->msg_name, (int *)&msg->msg_namelen, NULL,
				 NULL);
		msg->msg_controllen = 0;
		msg->msg_flags = (int)wflags;
	} else {
		WSAMSG wm = {
			.name = msg->msg_name,
			.namelen = msg->msg_namelen,
			.lpBuffers = bufs,
			.dwBufferCount = (ULONG)msg->msg_iovlen,
			.Control.buf = msg->msg_control,
			.Control.len = (ULONG)msg->msg_controllen,
			.dwFlags = wflags,
		};

		rc = wsarecvmsg_fn(s, &wm, &got, NULL, NULL);
		msg->msg_namelen = wm.namelen;
		msg->msg_controllen = wm.Control.len;
		msg->msg_flags = (int)wm.dwFlags;

		if (rc != SOCKET_ERROR)
			for (WSACMSGHDR *cm = WSA_CMSG_FIRSTHDR(&wm); cm;
			     cm = WSA_CMSG_NXTHDR(&wm, cm))
				if (cm->cmsg_level == IPPROTO_IP && cm->cmsg_type == IP_TTL)
					cm->cmsg_type = IP_RECVTTL;
	}
	if (rc == SOCKET_ERROR && !sock_read_fail(s)) {
		/* end of file */
		rc = 0;
		got = 0;
	}
	dontwait_end(fd, s, r);
	if (bufs != stack)
		free(bufs);
	return rc == SOCKET_ERROR ? -1 : (ssize_t)got;
}

struct cmsghdr *frr_win_cmsg_nxthdr(struct msghdr *msg, struct cmsghdr *cmsg)
{
	unsigned char *next, *end;

	if (!cmsg)
		return CMSG_FIRSTHDR(msg);
	next = (unsigned char *)cmsg + WSA_CMSGHDR_ALIGN(cmsg->cmsg_len);
	end = (unsigned char *)msg->msg_control + msg->msg_controllen;
	if (next + sizeof(struct cmsghdr) > end)
		return NULL;
	return (struct cmsghdr *)next;
}

/* loopback TCP socket pair (also used to emulate pipe()) */
static int sockpair(SOCKET sv[2])
{
	struct sockaddr_in sin = { .sin_family = AF_INET };
	struct sockaddr_in local, peer;
	int len = sizeof(sin), one = 1;
	SOCKET l, a = INVALID_SOCKET, c = INVALID_SOCKET;

	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	l = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
	if (l == INVALID_SOCKET)
		return sock_fail();
	setsockopt(l, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&one,
		   sizeof(one));
	if (bind(l, (struct sockaddr *)&sin, sizeof(sin)) ||
	    getsockname(l, (struct sockaddr *)&sin, &len) || listen(l, 1))
		goto fail;
	c = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
	if (c == INVALID_SOCKET || connect(c, (struct sockaddr *)&sin, sizeof(sin)))
		goto fail;
	a = accept(l, (struct sockaddr *)&peer, &len);
	if (a == INVALID_SOCKET)
		goto fail;
	/* make sure nobody else raced into our listener */
	len = sizeof(local);
	if (getsockname(c, (struct sockaddr *)&local, &len) ||
	    local.sin_port != peer.sin_port) {
		WSASetLastError(WSAECONNREFUSED);
		goto fail;
	}
	setsockopt(a, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one));
	setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one));
	closesocket(l);
	sv[0] = a;
	sv[1] = c;
	return 0;
fail:
	sock_fail();
	closesocket(l);
	if (a != INVALID_SOCKET)
		closesocket(a);
	if (c != INVALID_SOCKET)
		closesocket(c);
	return -1;
}

static int dgrampair(SOCKET sv[2])
{
	struct sockaddr_in sin[2];
	int i, len;

	sv[0] = sv[1] = INVALID_SOCKET;
	for (i = 0; i < 2; i++) {
		memset(&sin[i], 0, sizeof(sin[i]));
		sin[i].sin_family = AF_INET;
		sin[i].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		len = sizeof(sin[i]);
		sv[i] = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0);
		if (sv[i] == INVALID_SOCKET ||
		    bind(sv[i], (struct sockaddr *)&sin[i], sizeof(sin[i])) ||
		    getsockname(sv[i], (struct sockaddr *)&sin[i], &len))
			goto fail;
	}
	if (connect(sv[0], (struct sockaddr *)&sin[1], sizeof(sin[1])) ||
	    connect(sv[1], (struct sockaddr *)&sin[0], sizeof(sin[0])))
		goto fail;
	return 0;
fail:
	sock_fail();
	for (i = 0; i < 2; i++)
		if (sv[i] != INVALID_SOCKET)
			closesocket(sv[i]);
	return -1;
}

int frr_win_socketpair(int domain, int type, int protocol, int fds[2])
{
	SOCKET sv[2];
	int nb = type & SOCK_NONBLOCK;

	(void)domain;
	(void)protocol;
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (type == SOCK_SEQPACKET) {
		errno = EPROTONOSUPPORT;
		return -1;
	}
	if (type == SOCK_DGRAM ? dgrampair(sv) : sockpair(sv))
		return -1;
	fds[0] = fd_alloc(sv[0]);
	if (fds[0] < 0) {
		closesocket(sv[1]);
		return -1;
	}
	fds[1] = fd_alloc(sv[1]);
	if (fds[1] < 0) {
		frr_win_close(fds[0]);
		return -1;
	}
	if (nb) {
		set_nonblock(fds[0], sv[0], true);
		set_nonblock(fds[1], sv[1], true);
	}
	return 0;
}

int frr_win_pipe(int fds[2])
{
	return frr_win_socketpair(AF_INET, SOCK_STREAM, 0, fds);
}

ssize_t frr_win_read(int fd, void *buf, size_t len)
{
	SOCKET s = frr_win_fd2sock(fd);

	if (s != INVALID_SOCKET) {
		int n = recv(s, buf, clamp_int(len), 0);

		return n == SOCKET_ERROR ? sock_read_fail(s) : n;
	}
	return _read(fd, buf, (unsigned int)clamp_int(len));
}

ssize_t frr_win_write(int fd, const void *buf, size_t len)
{
	SOCKET s = frr_win_fd2sock(fd);

	if (s != INVALID_SOCKET) {
		int n = send(s, buf, clamp_int(len), 0);

		return n == SOCKET_ERROR ? sock_fail() : n;
	}
	return _write(fd, buf, (unsigned int)clamp_int(len));
}

ssize_t frr_win_readv(int fd, const struct iovec *iov, int iovcnt)
{
	SOCKET s = frr_win_fd2sock(fd);
	ssize_t total = 0;

	if (s != INVALID_SOCKET) {
		WSABUF stack[IOV_STACK], *bufs;
		DWORD got = 0, flags = 0;
		int rc;

		bufs = iov_to_wsabuf(iov, iovcnt, stack);
		if (!bufs) {
			errno = ENOMEM;
			return -1;
		}
		rc = WSARecv(s, bufs, iovcnt, &got, &flags, NULL, NULL);
		if (bufs != stack)
			free(bufs);
		return rc == SOCKET_ERROR ? sock_read_fail(s) : (ssize_t)got;
	}
	for (int i = 0; i < iovcnt; i++) {
		int n = _read(fd, iov[i].iov_base, (unsigned int)iov[i].iov_len);

		if (n < 0)
			return total ? total : -1;
		total += n;
		if ((size_t)n < iov[i].iov_len)
			break;
	}
	return total;
}

ssize_t frr_win_writev(int fd, const struct iovec *iov, int iovcnt)
{
	SOCKET s = frr_win_fd2sock(fd);
	ssize_t total = 0;

	if (s != INVALID_SOCKET) {
		WSABUF stack[IOV_STACK], *bufs;
		DWORD sent = 0;
		int rc;

		bufs = iov_to_wsabuf(iov, iovcnt, stack);
		if (!bufs) {
			errno = ENOMEM;
			return -1;
		}
		rc = WSASend(s, bufs, iovcnt, &sent, 0, NULL, NULL);
		if (bufs != stack)
			free(bufs);
		return rc == SOCKET_ERROR ? sock_fail() : (ssize_t)sent;
	}
	for (int i = 0; i < iovcnt; i++) {
		int n = _write(fd, iov[i].iov_base,
			       (unsigned int)iov[i].iov_len);

		if (n < 0)
			return total ? total : -1;
		total += n;
		if ((size_t)n < iov[i].iov_len)
			break;
	}
	return total;
}

int frr_win_close(int fd)
{
	SOCKET s = frr_win_fd2sock(fd);

	if (s != INVALID_SOCKET) {
		fd_release(fd);
		closesocket(s);
	}
	return _close(fd);
}

SOCKET frr_win_fd_detach(int fd)
{
	SOCKET s = frr_win_fd2sock(fd);

	if (s != INVALID_SOCKET) {
		fd_release(fd);
		_close(fd);
	}
	return s;
}

#define WHOLE_FILE_LOCK_OFFSET (1ULL << 62)

static int file_lock(int fd, int cmd, struct flock *fl)
{
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	OVERLAPPED ov = { 0 };
	ULARGE_INTEGER off, len;
	DWORD flags = 0;

	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	if (fl->l_len == 0) {
		off.QuadPart = WHOLE_FILE_LOCK_OFFSET;
		len.QuadPart = 1;
	} else {
		off.QuadPart = (ULONGLONG)fl->l_start;
		len.QuadPart = (ULONGLONG)fl->l_len;
	}
	ov.Offset = off.LowPart;
	ov.OffsetHigh = off.HighPart;

	if (cmd == F_GETLK) {
		if (LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK |
					  LOCKFILE_FAIL_IMMEDIATELY,
			       0, len.LowPart, len.HighPart, &ov)) {
			UnlockFileEx(h, 0, len.LowPart, len.HighPart, &ov);
			fl->l_type = F_UNLCK;
		} else
			fl->l_pid = 0;
		return 0;
	}
	if (fl->l_type == F_UNLCK) {
		if (UnlockFileEx(h, 0, len.LowPart, len.HighPart, &ov))
			return 0;
		errno = EINVAL;
		return -1;
	}
	if (fl->l_type == F_WRLCK)
		flags |= LOCKFILE_EXCLUSIVE_LOCK;
	if (cmd == F_SETLK)
		flags |= LOCKFILE_FAIL_IMMEDIATELY;
	if (LockFileEx(h, flags, 0, len.LowPart, len.HighPart, &ov))
		return 0;
	errno = GetLastError() == ERROR_LOCK_VIOLATION ? EAGAIN : EACCES;
	return -1;
}

int frr_win_fcntl(int fd, int cmd, ...)
{
	SOCKET s = frr_win_fd2sock(fd);
	va_list ap;
	int arg = 0;
	struct flock *fl = NULL;

	va_start(ap, cmd);
	if (cmd == F_SETLK || cmd == F_SETLKW || cmd == F_GETLK)
		fl = va_arg(ap, struct flock *);
	else if (cmd == F_SETFL || cmd == F_SETFD)
		arg = va_arg(ap, int);
	va_end(ap);

	switch (cmd) {
	case F_GETFD:
		return FD_CLOEXEC;
	case F_SETFD:
		return 0;
	case F_GETFL:
		if (s == INVALID_SOCKET)
			return _O_RDWR;
		return _O_RDWR | (fd_nonblock[fd] ? O_NONBLOCK : 0);
	case F_SETFL:
		if (s == INVALID_SOCKET)
			return 0;
		return set_nonblock(fd, s, !!(arg & O_NONBLOCK));
	case F_SETLK:
	case F_SETLKW:
	case F_GETLK:
		if (s != INVALID_SOCKET) {
			errno = EINVAL;
			return -1;
		}
		return file_lock(fd, cmd, fl);
	default:
		errno = EINVAL;
		return -1;
	}
}

int frr_win_ioctl(int fd, unsigned long req, ...)
{
	SOCK_OR_FAIL(s, fd);
	va_list ap;
	void *argp;
	u_long v;

	va_start(ap, req);
	argp = va_arg(ap, void *);
	va_end(ap);

	switch (req) {
	case FIONREAD:
		if (ioctlsocket(s, FIONREAD, &v))
			return sock_fail();
		*(int *)argp = (int)v;
		return 0;
	case FIONBIO:
		return set_nonblock(fd, s, *(int *)argp != 0);
	default:
		errno = ENOTTY;
		return -1;
	}
}

int frr_win_dup(int fd)
{
	if (frr_win_is_socket(fd)) {
		errno = ENOTSUP;
		return -1;
	}
	return _dup(fd);
}

int frr_win_dup2(int fd, int fd2)
{
	if (frr_win_is_socket(fd) || frr_win_is_socket(fd2)) {
		errno = ENOTSUP;
		return -1;
	}
	return _dup2(fd, fd2);
}

static struct sigaction sigtab[FRR_WIN_NSIG];
static volatile DWORD sig_thread;
static SOCKET sigwake[2] = { INVALID_SOCKET, INVALID_SOCKET };
static thread_local sigset_t thread_sigmask;

static void sig_default(int sig)
{
	switch (sig) {
	case SIGCHLD:
	case SIGWINCH:
	case SIGPIPE:
	case SIGCONT:
	case SIGALRM:
		return;
	default:
		_exit(128 + sig);
	}
}

static void sig_deliver(int sig)
{
	struct sigaction *sa;

	if (sig <= 0 || sig >= FRR_WIN_NSIG)
		return;
	sa = &sigtab[sig];
	if (sa->sa_flags & SA_SIGINFO) {
		siginfo_t si = { .si_signo = sig };

		if (sa->sa_sigaction)
			sa->sa_sigaction(sig, &si, NULL);
		else
			sig_default(sig);
	} else if (sa->sa_handler == SIG_IGN) {
		return;
	} else if (sa->sa_handler == SIG_DFL || !sa->sa_handler) {
		sig_default(sig);
	} else {
		sa->sa_handler(sig);
	}

	/* kick the signal-handling thread out of poll() with EINTR */
	if (sigwake[1] != INVALID_SOCKET)
		send(sigwake[1], "s", 1, 0);
}

void frr_win_raise_signal(int sig)
{
	sig_deliver(sig);
}

static HANDLE sigterm_event_open(DWORD pid, bool create)
{
	static const char *const spaces[] = { "Global", "Local" };
	char name[64];
	HANDLE ev;

	for (size_t i = 0; i < sizeof(spaces) / sizeof(spaces[0]); i++) {
		snprintf(name, sizeof(name), "%s\\FRR-SIGTERM-%lu", spaces[i], pid);
		ev = create ? CreateEventA(NULL, FALSE, FALSE, name)
			    : OpenEventA(EVENT_MODIFY_STATE, FALSE, name);
		if (ev)
			return ev;
	}
	return NULL;
}

static void CALLBACK sigterm_event_set(PVOID arg, BOOLEAN timedout)
{
	(void)arg;
	(void)timedout;
	sig_deliver(SIGTERM);
}

static void sigterm_event_init(void)
{
	static HANDLE ev, wait;

	if (ev)
		return;
	ev = sigterm_event_open(GetCurrentProcessId(), true);
	if (ev)
		RegisterWaitForSingleObject(&wait, ev, sigterm_event_set, NULL, INFINITE,
					    WT_EXECUTEDEFAULT);
}

static BOOL WINAPI console_ctrl(DWORD type)
{
	switch (type) {
	case CTRL_C_EVENT:
		sig_deliver(SIGINT);
		return TRUE;
	case CTRL_BREAK_EVENT:
		sig_deliver(SIGTERM);
		return TRUE;
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		sig_deliver(SIGTERM);
		/* XXX: give the daemon time to exit */
		Sleep(5000);
		return TRUE;
	default:
		return FALSE;
	}
}

void frr_win_crash_dump(void *exception_pointers);
void frr_win_crash_context(const void *context);

static int crash_signal(DWORD code)
{
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_IN_PAGE_ERROR:
	case EXCEPTION_STACK_OVERFLOW:
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		return SIGSEGV;
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	case EXCEPTION_PRIV_INSTRUCTION:
		return SIGILL;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_INT_OVERFLOW:
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_STACK_CHECK:
	case EXCEPTION_FLT_UNDERFLOW:
		return SIGFPE;
	default:
		return 0;
	}
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
	const EXCEPTION_RECORD *er = ep->ExceptionRecord;
	int sig = crash_signal(er->ExceptionCode);
	struct sigaction *sa;
	siginfo_t si = { 0 };

	if (!sig)
		return EXCEPTION_CONTINUE_SEARCH;

	frr_win_crash_dump(ep);

	sa = &sigtab[sig];
	si.si_signo = sig;
	/* an access violation says what it tried to access */
	if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
		si.si_addr = (void *)er->ExceptionInformation[1];
	else
		si.si_addr = er->ExceptionAddress;

	frr_win_crash_context(ep->ContextRecord);
	if ((sa->sa_flags & SA_SIGINFO) && sa->sa_sigaction)
		sa->sa_sigaction(sig, &si, NULL);
	else if (!(sa->sa_flags & SA_SIGINFO) && sa->sa_handler != SIG_DFL &&
		 sa->sa_handler != SIG_IGN && sa->sa_handler)
		sa->sa_handler(sig);
	frr_win_crash_context(NULL);

	/* the handler did not end the process: the system does */
	return EXCEPTION_CONTINUE_SEARCH;
}

static void crt_signal_trampoline(int sig)
{
	/* the CRT resets the handler; re-arm then deliver */
	(signal)(sig, crt_signal_trampoline);

	/* An exception the CRT turned into this signal: the CRT has it in
	 * _pxcptinfoptrs, which raise() leaves NULL.
	 */
	if (sig == SIGSEGV || sig == SIGILL || sig == SIGFPE) {
		EXCEPTION_POINTERS *ep = _pxcptinfoptrs;

		frr_win_crash_dump(ep);
		frr_win_crash_context(ep ? ep->ContextRecord : NULL);
	} else if (sig == SIGABRT)
		frr_win_crash_dump(NULL);
	sig_deliver(sig);
	frr_win_crash_context(NULL);
}

int sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
	if (sig <= 0 || sig >= FRR_WIN_NSIG) {
		errno = EINVAL;
		return -1;
	}
	if (oact)
		*oact = sigtab[sig];
	if (!act)
		return 0;
	sigtab[sig] = *act;

	switch (sig) {
	case SIGINT:
	case SIGTERM:
	case SIGHUP:
		if (!sig_thread)
			sig_thread = GetCurrentThreadId();
		if (sig == SIGTERM && ((act->sa_flags & SA_SIGINFO) ||
				       (act->sa_handler != SIG_DFL && act->sa_handler != SIG_IGN)))
			sigterm_event_init();
		break;
	case SIGSEGV:
	case SIGILL:
	case SIGFPE:
	case SIGABRT:
		if (act->sa_handler == SIG_DFL && !(act->sa_flags & SA_SIGINFO))
			(signal)(sig, SIG_DFL);
		else
			(signal)(sig, crt_signal_trampoline);
		break;
	}
	return 0;
}

frr_win_sighandler_t frr_win_signal(int sig, frr_win_sighandler_t h)
{
	struct sigaction sa = { .sa_handler = h }, osa;

	if (sigaction(sig, &sa, &osa))
		return SIG_ERR;
	return (osa.sa_flags & SA_SIGINFO) ? SIG_DFL : osa.sa_handler;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	if (oset)
		*oset = thread_sigmask;
	if (!set)
		return 0;
	switch (how) {
	case SIG_BLOCK:
		thread_sigmask |= *set;
		break;
	case SIG_UNBLOCK:
		thread_sigmask &= ~*set;
		break;
	case SIG_SETMASK:
		thread_sigmask = *set;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	int rv = pthread_sigmask(how, set, oset);

	if (rv) {
		errno = rv;
		return -1;
	}
	return 0;
}

int sigpending(sigset_t *set)
{
	*set = 0;
	return 0;
}

char *strsignal(int sig)
{
	static char buf[32];

	switch (sig) {
	case SIGINT:
		return "Interrupt";
	case SIGTERM:
		return "Terminated";
	case SIGHUP:
		return "Hangup";
	case SIGSEGV:
		return "Segmentation fault";
	case SIGABRT:
		return "Aborted";
	case SIGILL:
		return "Illegal instruction";
	case SIGFPE:
		return "Floating point exception";
	case SIGUSR1:
		return "User defined signal 1";
	case SIGUSR2:
		return "User defined signal 2";
	default:
		snprintf(buf, sizeof(buf), "Signal %d", sig);
		return buf;
	}
}

unsigned int alarm(unsigned int seconds)
{
	(void)seconds;
	return 0;
}

#define SVC_STOP_WAIT_MSEC 30000
#define SVC_STOP_COMMAND_MSEC 120000
#define SVC_CHECKPOINT_MSEC 2000

static SERVICE_STATUS_HANDLE svc_handle;
static volatile LONG svc_stop_requested;
static wchar_t svc_name[257];
static SRWLOCK svc_lock = SRWLOCK_INIT;

static void svc_report(DWORD state, DWORD exitcode, DWORD wait_hint)
{
	static DWORD checkpoint;
	SERVICE_STATUS st = {
		.dwServiceType = SERVICE_WIN32_OWN_PROCESS,
		.dwCurrentState = state,
		.dwWin32ExitCode = exitcode,
		.dwWaitHint = wait_hint,
	};

	if (state == SERVICE_RUNNING)
		st.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	AcquireSRWLockExclusive(&svc_lock);
	if (state != SERVICE_RUNNING && state != SERVICE_STOPPED)
		st.dwCheckPoint = ++checkpoint;
	SetServiceStatus(svc_handle, &st);
	ReleaseSRWLockExclusive(&svc_lock);
}

static DWORD WINAPI svc_control(DWORD control, DWORD type, LPVOID data, LPVOID ctx)
{
	(void)type;
	(void)data;
	(void)ctx;

	switch (control) {
	case SERVICE_CONTROL_INTERROGATE:
		return ERROR_SUCCESS;
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		if (!InterlockedExchange(&svc_stop_requested, 1)) {
			svc_report(SERVICE_STOP_PENDING, ERROR_SUCCESS, SVC_STOP_WAIT_MSEC);
			sig_deliver(SIGTERM);
		}
		return ERROR_SUCCESS;
	default:
		return ERROR_CALL_NOT_IMPLEMENTED;
	}
}

static void WINAPI svc_main(DWORD argc, LPWSTR *argv)
{
	if (argc > 0 && argv[0])
		wcsncpy_s(svc_name, _countof(svc_name), argv[0], _TRUNCATE);

	svc_handle = RegisterServiceCtrlHandlerExW(svc_name, svc_control, NULL);
	if (svc_handle)
		svc_report(SERVICE_RUNNING, ERROR_SUCCESS, 0);
}

static DWORD WINAPI svc_dispatcher(LPVOID arg)
{
	SERVICE_TABLE_ENTRYW table[] = {
		{ L"", svc_main },
		{ NULL, NULL },
	};

	(void)arg;
	/* returns once the service is stopped, or at once if not a service */
	StartServiceCtrlDispatcherW(table);
	return 0;
}

static void svc_stop_command(void)
{
	wchar_t key[300], command[2048], line[2200], comspec[MAX_PATH];
	PROCESS_INFORMATION pi;
	STARTUPINFOW si = { .cb = sizeof(si) };
	DWORD size = sizeof(command), waited;

	if (swprintf(key, _countof(key), L"SYSTEM\\CurrentControlSet\\Services\\%ls\\Parameters",
		     svc_name) < 0 ||
	    RegGetValueW(HKEY_LOCAL_MACHINE, key, L"StopCommand", RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
			 NULL, command, &size) != ERROR_SUCCESS ||
	    !command[0])
		return;

	if (!GetEnvironmentVariableW(L"ComSpec", comspec, _countof(comspec)) ||
	    swprintf(line, _countof(line), L"\"%ls\" /d /s /c \"%ls\"", comspec, command) < 0)
		return;

	if (!CreateProcessW(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
		return;
	CloseHandle(pi.hThread);

	for (waited = 0; waited < SVC_STOP_COMMAND_MSEC; waited += SVC_CHECKPOINT_MSEC) {
		if (WaitForSingleObject(pi.hProcess, SVC_CHECKPOINT_MSEC) != WAIT_TIMEOUT)
			break;
		svc_report(SERVICE_STOP_PENDING, ERROR_SUCCESS, SVC_CHECKPOINT_MSEC * 2);
	}
	CloseHandle(pi.hProcess);
}

static void svc_atexit(void)
{
	if (!svc_handle)
		return;

	if (svc_stop_requested) {
		svc_report(SERVICE_STOP_PENDING, ERROR_SUCCESS, SVC_CHECKPOINT_MSEC * 2);
		svc_stop_command();
	}
	svc_report(SERVICE_STOPPED, svc_stop_requested ? ERROR_SUCCESS : ERROR_PROCESS_ABORTED, 0);
}

static void svc_init(void)
{
	HANDLE t = CreateThread(NULL, 0, svc_dispatcher, NULL, 0, NULL);

	if (t) {
		CloseHandle(t);
		atexit(svc_atexit);
	}
}

/* There is no fork() */
#define CHILD_MAX 64

static struct child {
	DWORD pid; /* 0: free */
	HANDLE process;
	HANDLE job;
	HANDLE wait;
	bool exited;
	int status; /* what waitpid() gives */
	int sig;    /* kill() ended it with this signal */
} children[CHILD_MAX];
static SRWLOCK children_lock = SRWLOCK_INIT;
static HANDLE children_event; /* a child exited */

/* on a thread of the system, like console_ctrl() */
static void CALLBACK child_exited(PVOID arg, BOOLEAN timedout)
{
	struct child *c = arg;
	DWORD code = 0;

	(void)timedout;
	AcquireSRWLockExclusive(&children_lock);
	GetExitCodeProcess(c->process, &code);
	c->status = c->sig ? c->sig : (int)(code & 0xff) << 8;
	c->exited = true;
	SetEvent(children_event);
	ReleaseSRWLockExclusive(&children_lock);

	sig_deliver(SIGCHLD);
}

pid_t frr_win_spawn_shell(const char *cmd)
{
	static const char fmt[] = "\"%s\" /d /s /c \"%s\"";
	STARTUPINFOA si = { .cb = sizeof(si) };
	PROCESS_INFORMATION pi;
	const char *comspec = getenv("COMSPEC");
	struct child *c = NULL;
	size_t len;
	char *line;

	if (!comspec)
		comspec = "cmd.exe";
	len = sizeof(fmt) + strlen(comspec) + strlen(cmd);
	line = malloc(len);
	if (!line) {
		errno = ENOMEM;
		return -1;
	}
	snprintf(line, len, fmt, comspec, cmd);

	AcquireSRWLockExclusive(&children_lock);
	if (!children_event)
		children_event = CreateEventA(NULL, TRUE, FALSE, NULL);
	for (int i = 0; i < CHILD_MAX && !c; i++)
		if (!children[i].pid)
			c = &children[i];
	if (!c || !children_event) {
		ReleaseSRWLockExclusive(&children_lock);
		free(line);
		errno = EAGAIN;
		return -1;
	}
	memset(c, 0, sizeof(*c));

	if (!CreateProcessA(NULL, line, NULL, NULL, FALSE,
			    CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, NULL,
			    NULL, &si, &pi)) {
		errno = win_errno(GetLastError());
		ReleaseSRWLockExclusive(&children_lock);
		free(line);
		return -1;
	}
	free(line);

	c->job = CreateJobObjectA(NULL, NULL);
	if (c->job && !AssignProcessToJobObject(c->job, pi.hProcess)) {
		CloseHandle(c->job);
		c->job = NULL;
	}
	c->process = pi.hProcess;
	if (!RegisterWaitForSingleObject(&c->wait, pi.hProcess, child_exited, c, INFINITE,
					 WT_EXECUTEONLYONCE)) {
		TerminateProcess(pi.hProcess, 1);
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		if (c->job)
			CloseHandle(c->job);
		ReleaseSRWLockExclusive(&children_lock);
		errno = EAGAIN;
		return -1;
	}
	c->pid = pi.dwProcessId;
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	ReleaseSRWLockExclusive(&children_lock);

	return (pid_t)pi.dwProcessId;
}

int kill(pid_t pid, int sig)
{
	HANDLE h;

	if (pid == _getpid()) {
		if (sig)
			sig_deliver(sig);
		return 0;
	}
	if (pid < -1) {
		/* a process group: the job of a child */
		bool found = false;

		AcquireSRWLockExclusive(&children_lock);
		for (int i = 0; i < CHILD_MAX && !found; i++) {
			struct child *c = &children[i];

			if (c->pid != (DWORD)-pid)
				continue;
			found = true;
			if (sig && !c->exited) {
				c->sig = sig;
				if (c->job)
					TerminateJobObject(c->job, 128 + sig);
				else
					TerminateProcess(c->process, 128 + sig);
			}
		}
		ReleaseSRWLockExclusive(&children_lock);
		if (!found) {
			errno = ESRCH;
			return -1;
		}
		return 0;
	}
	if (sig == SIGTERM && pid > 0) {
		HANDLE ev = sigterm_event_open((DWORD)pid, false);

		if (ev) {
			BOOL set = SetEvent(ev);

			CloseHandle(ev);
			if (set)
				return 0;
		}
	}
	h = OpenProcess(sig ? PROCESS_TERMINATE
			    : PROCESS_QUERY_LIMITED_INFORMATION,
			FALSE, (DWORD)pid);
	if (!h) {
		errno = GetLastError() == ERROR_ACCESS_DENIED ? EPERM : ESRCH;
		return -1;
	}
	if (sig && !TerminateProcess(h, 128 + sig)) {
		CloseHandle(h);
		errno = EPERM;
		return -1;
	}
	CloseHandle(h);
	return 0;
}

/* the children of frr_win_spawn_shell() */
pid_t waitpid(pid_t pid, int *status, int options)
{
	for (;;) {
		bool any = false;

		AcquireSRWLockExclusive(&children_lock);
		for (int i = 0; i < CHILD_MAX; i++) {
			struct child *c = &children[i];
			pid_t ret;

			if (!c->pid || (pid > 0 && c->pid != (DWORD)pid))
				continue;
			any = true;
			if (!c->exited)
				continue;

			ret = (pid_t)c->pid;
			if (status)
				*status = c->status;
			UnregisterWaitEx(c->wait, NULL);
			CloseHandle(c->process);
			if (c->job)
				CloseHandle(c->job);
			c->pid = 0;
			ReleaseSRWLockExclusive(&children_lock);
			return ret;
		}
		if (any && !(options & WNOHANG))
			ResetEvent(children_event);
		ReleaseSRWLockExclusive(&children_lock);

		if (!any) {
			errno = ECHILD;
			return -1;
		}
		if (options & WNOHANG)
			return 0;
		WaitForSingleObject(children_event, INFINITE);
	}
}

pid_t getppid(void)
{
	return 0;
}

int frr_win_select(int nfds, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tv)
{
	fd_set *fds[3] = { rd, wr, ex };
	fd_set socks[3];
	int rc;

	(void)nfds;
	for (int i = 0; i < 3; i++) {
		if (!fds[i])
			continue;
		socks[i].fd_count = fds[i]->fd_count;
		for (u_int j = 0; j < fds[i]->fd_count; j++) {
			SOCKET s = frr_win_fd2sock((int)fds[i]->fd_array[j]);

			if (s == INVALID_SOCKET) {
				errno = EBADF;
				return -1;
			}
			socks[i].fd_array[j] = s;
		}
	}

	rc = select(0, rd ? &socks[0] : NULL, wr ? &socks[1] : NULL, ex ? &socks[2] : NULL, tv);
	if (rc == SOCKET_ERROR)
		return sock_fail();

	for (int i = 0; i < 3; i++) {
		u_int n = 0;

		if (!fds[i])
			continue;
		for (u_int j = 0; j < fds[i]->fd_count; j++) {
			SOCKET s = frr_win_fd2sock((int)fds[i]->fd_array[j]);

			if (FD_ISSET(s, &socks[i]))
				fds[i]->fd_array[n++] = fds[i]->fd_array[j];
		}
		fds[i]->fd_count = n;
	}
	return rc;
}

#define POLL_STACK 64

#define STDIN_POLL_MSEC 10

static bool stdin_readable(void)
{
	HANDLE h = (HANDLE)_get_osfhandle(STDIN_FILENO);
	INPUT_RECORD record;
	DWORD mode, count;

	if (GetConsoleMode(h, &mode)) {
		/* a key that reading the console returns, behind events that
		 * it skips: releases, modifiers, focus, mouse, window size
		 */
		while (PeekConsoleInputW(h, &record, 1, &count) && count) {
			const KEY_EVENT_RECORD *key = &record.Event.KeyEvent;

			if (record.EventType == KEY_EVENT && key->bKeyDown &&
			    (key->uChar.UnicodeChar ||
			     (key->wVirtualKeyCode >= VK_PRIOR &&
			      key->wVirtualKeyCode <= VK_DOWN) ||
			     key->wVirtualKeyCode == VK_INSERT ||
			     key->wVirtualKeyCode == VK_DELETE ||
			     (key->wVirtualKeyCode >= VK_F1 &&
			      key->wVirtualKeyCode <= VK_F12)))
				return true;
			ReadConsoleInputW(h, &record, 1, &count);
		}
		return false;
	}
	if (PeekNamedPipe(h, NULL, 0, NULL, &count, NULL))
		return count > 0;
	/* the end of a pipe, or a file: reading won't block */
	return true;
}

static bool icmp6_filter_pass(const struct icmp6_filter *filter, SOCKET s)
{
	WSAPOLLFD pfd = { .fd = s, .events = POLLRDNORM };
	uint8_t type;
	int rc;

	for (;;) {
		rc = recv(s, (char *)&type, sizeof(type), MSG_PEEK);
		if (rc == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			return false;
		if (rc != 1 && !(rc == SOCKET_ERROR && WSAGetLastError() == WSAEMSGSIZE))
			return true;
		if (!(filter->icmp6_filt[type >> 5] & (1U << (type & 31))))
			return true;

		recv(s, (char *)&type, sizeof(type), 0);
		if (WSAPoll(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLRDNORM))
			return false;
	}
}

int frr_win_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	WSAPOLLFD stack[POLL_STACK], *wfds;
	int stackidx[POLL_STACK], *idx;
	nfds_t i, n = 0;
	int invalid = 0, count = 0, rc;
	int stdin_idx = -1, stdin_ready = 0;
	bool want_sig = GetCurrentThreadId() == sig_thread &&
			sigwake[0] != INVALID_SOCKET;
	bool got_sig = false;

	if (nfds + 1 <= POLL_STACK) {
		wfds = stack;
		idx = stackidx;
	} else {
		wfds = malloc((nfds + 1) * sizeof(*wfds));
		idx = malloc((nfds + 1) * sizeof(*idx));
		if (!wfds || !idx) {
			free(wfds);
			free(idx);
			errno = ENOMEM;
			return -1;
		}
	}

	for (i = 0; i < nfds; i++) {
		SOCKET s;

		fds[i].revents = 0;
		if (fds[i].fd < 0)
			continue;
		s = frr_win_fd2sock(fds[i].fd);
		if (s == INVALID_SOCKET && fds[i].fd == STDIN_FILENO &&
		    (fds[i].events & POLLRDNORM)) {
			stdin_idx = (int)i;
			continue;
		}
		if (s == INVALID_SOCKET) {
			fds[i].revents = POLLNVAL;
			invalid++;
			continue;
		}
		wfds[n].fd = s;
		wfds[n].events = fds[i].events &
				 (POLLRDNORM | POLLRDBAND | POLLWRNORM);
		wfds[n].revents = 0;
		idx[n++] = (int)i;
	}
	if (want_sig) {
		wfds[n].fd = sigwake[0];
		wfds[n].events = POLLRDNORM;
		wfds[n].revents = 0;
		idx[n++] = -1;
	}
	if (invalid)
		timeout = 0;

	if (stdin_idx >= 0) {
		ULONGLONG end = GetTickCount64() + (timeout > 0 ? timeout : 0);

		for (;;) {
			int slice = STDIN_POLL_MSEC;

			if (stdin_readable()) {
				fds[stdin_idx].revents = POLLRDNORM;
				stdin_ready = 1;
				slice = 0;
			} else if (timeout >= 0) {
				ULONGLONG now = GetTickCount64();

				slice = STDIN_POLL_MSEC;
				if (now >= end)
					slice = 0;
				else if (end - now < STDIN_POLL_MSEC)
					slice = (int)(end - now);
			}
			rc = 0;
			if (n)
				rc = WSAPoll(wfds, (ULONG)n, slice);
			else if (slice)
				Sleep(slice);
			if (rc || !slice)
				break;
		}
	} else if (n == 0) {
		if (timeout > 0)
			Sleep(timeout);
		rc = invalid;
		goto out;
	} else {
		rc = WSAPoll(wfds, (ULONG)n, timeout);
	}
	if (rc == SOCKET_ERROR) {
		sock_fail();
		rc = -1;
		goto out;
	}

	count = invalid + stdin_ready;
	for (i = 0; i < n; i++) {
		if (!wfds[i].revents)
			continue;
		if (idx[i] < 0) {
			char buf[64];

			while (recv(sigwake[0], buf, sizeof(buf), 0) > 0)
				;
			got_sig = true;
			continue;
		}
		if ((wfds[i].revents & POLLRDNORM) && fd_icmp6_filter[fds[idx[i]].fd] &&
		    !icmp6_filter_pass(fd_icmp6_filter[fds[idx[i]].fd], wfds[i].fd)) {
			wfds[i].revents &= ~(POLLRDNORM | POLLRDBAND);
			if (!wfds[i].revents)
				continue;
		}
		fds[idx[i]].revents = wfds[i].revents;
		count++;
	}
	if (got_sig) {
		errno = EINTR;
		rc = -1;
	} else
		rc = count;
out:
	if (wfds != stack) {
		free(wfds);
		free(idx);
	}
	return rc;
}

int frr_win_open(const char *path, int flags, ...)
{
	int mode = 0;

	if (flags & _O_CREAT) {
		va_list ap;

		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	if (!strcmp(path, "/dev/null"))
		path = "NUL";
	if (!strcmp(path, "/dev/urandom") || !strcmp(path, "/dev/random")) {
		unsigned int bytes[64];
		int fds[2];

		for (size_t i = 0; i < sizeof(bytes) / sizeof(bytes[0]); i++)
			rand_s(&bytes[i]);
		if (_pipe(fds, sizeof(bytes), _O_BINARY | _O_NOINHERIT))
			return -1;
		_write(fds[1], bytes, sizeof(bytes));
		_close(fds[1]);
		return fds[0];
	}
	if (flags & O_DIRECTORY) {
		DWORD attr = GetFileAttributesA(path);

		if (attr == INVALID_FILE_ATTRIBUTES) {
			errno = win_errno(GetLastError());
			return -1;
		}
		if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
			errno = ENOTDIR;
			return -1;
		}
		/* a directory fd, good for openat() & co. and for close() */
		return _open(path, _O_RDONLY | _O_BINARY | _O_OBTAIN_DIR);
	}
	flags &= ~O_NONBLOCK;
	if (!(flags & _O_TEXT))
		flags |= _O_BINARY;
	return _open(path, flags, mode ? (mode & (_S_IREAD | _S_IWRITE))
				       : (_S_IREAD | _S_IWRITE));
}

int frr_win_mkdir(const char *path, mode_t mode)
{
	char parents[MAX_PATH];
	char *p;

	(void)mode;

	if (strlen(path) < sizeof(parents)) {
		strcpy(parents, path);
		for (p = strpbrk(parents, "/\\"); p; p = strpbrk(p + 1, "/\\")) {
			if (p == parents || p[-1] == ':')
				continue;
			*p = '\0';
			_mkdir(parents);
			*p = '/';
		}
	}
	return _mkdir(path);
}

int frr_win_rename(const char *from, const char *to)
{
	if (MoveFileExA(from, to,
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
		return 0;
	errno = win_errno(GetLastError());
	return -1;
}

int frr_win_unlink(const char *path)
{
	if (!_unlink(path))
		return 0;
	/* AF_UNIX socket files are reparse points; DeleteFile copes */
	if (DeleteFileA(path))
		return 0;
	errno = win_errno(GetLastError());
	return -1;
}

static int at_path(int dirfd, const char *path, char *out, size_t outsz)
{
	HANDLE h;
	DWORD len;

	if (dirfd == AT_FDCWD || path[0] == '/' || path[0] == '\\' ||
	    (path[0] && path[1] == ':')) {
		if (strlen(path) >= outsz) {
			errno = ENAMETOOLONG;
			return -1;
		}
		strcpy(out, path);
		return 0;
	}
	if (dirfd < 0 || frr_win_is_socket(dirfd)) {
		errno = EBADF;
		return -1;
	}
	h = (HANDLE)_get_osfhandle(dirfd);
	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	len = GetFinalPathNameByHandleA(h, out, (DWORD)outsz,
					FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	if (!len) {
		errno = win_errno(GetLastError());
		return -1;
	}
	if (len + 1 + strlen(path) >= outsz) {
		errno = ENAMETOOLONG;
		return -1;
	}
	out[len] = '\\';
	strcpy(out + len + 1, path);
	return 0;
}

int openat(int dirfd, const char *path, int flags, ...)
{
	char full[MAX_PATH + 8];
	int mode = 0;

	if (flags & _O_CREAT) {
		va_list ap;

		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	if (at_path(dirfd, path, full, sizeof(full)) < 0)
		return -1;
	return frr_win_open(full, flags, mode);
}

int renameat(int olddirfd, const char *oldpath, int newdirfd,
	     const char *newpath)
{
	char from[MAX_PATH + 8], to[MAX_PATH + 8];

	if (at_path(olddirfd, oldpath, from, sizeof(from)) < 0 ||
	    at_path(newdirfd, newpath, to, sizeof(to)) < 0)
		return -1;
	return frr_win_rename(from, to);
}

int unlinkat(int dirfd, const char *path, int flags)
{
	char full[MAX_PATH + 8];

	if (at_path(dirfd, path, full, sizeof(full)) < 0)
		return -1;
	if (flags & AT_REMOVEDIR)
		return _rmdir(full);
	return frr_win_unlink(full);
}

int fsync(int fd)
{
	BY_HANDLE_FILE_INFORMATION info;
	HANDLE h = (HANDLE)_get_osfhandle(fd);

	/* a directory fd is open for reading only and cannot be flushed;
	 * NTFS logs directory updates
	 */
	if (h != INVALID_HANDLE_VALUE && GetFileInformationByHandle(h, &info) &&
	    (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return 0;
	return _commit(fd);
}

int ftruncate(int fd, long long len)
{
	errno_t e = _chsize_s(fd, len);

	if (e) {
		errno = e;
		return -1;
	}
	return 0;
}

int frr_win_isatty(int fd)
{
	if (frr_win_is_socket(fd))
		return 0;
	return _isatty(fd);
}

#ifndef IO_REPARSE_TAG_AF_UNIX
#define IO_REPARSE_TAG_AF_UNIX 0x80000023L
#endif

int frr_win_stat(const char *path, struct stat *st)
{
	WIN32_FIND_DATAA found;
	HANDLE find;
	int ret = stat(path, st);

	find = FindFirstFileA(path, &found);
	if (find == INVALID_HANDLE_VALUE)
		return ret;
	FindClose(find);

	if ((found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
	    found.dwReserved0 == IO_REPARSE_TAG_AF_UNIX) {
		if (ret)
			memset(st, 0, sizeof(*st));
		st->st_mode = S_IFSOCK | _S_IREAD | _S_IWRITE;
		return 0;
	}
	return ret;
}

int flock(int fd, int operation)
{
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	ULARGE_INTEGER off = { .QuadPart = WHOLE_FILE_LOCK_OFFSET + 1 };
	OVERLAPPED ov = { .Offset = off.LowPart, .OffsetHigh = off.HighPart };
	DWORD flags = 0;

	if (h == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return -1;
	}
	if (operation & LOCK_UN)
		return UnlockFileEx(h, 0, 1, 0, &ov) ? 0 : -1;

	if (operation & LOCK_EX)
		flags |= LOCKFILE_EXCLUSIVE_LOCK;
	if (operation & LOCK_NB)
		flags |= LOCKFILE_FAIL_IMMEDIATELY;
	if (LockFileEx(h, flags, 0, 1, 0, &ov))
		return 0;
	errno = GetLastError() == ERROR_LOCK_VIOLATION ? EWOULDBLOCK : EINVAL;
	return -1;
}

struct passwd *getpwuid(uid_t uid)
{
	static struct passwd pw;
	static char name[257], home[MAX_PATH];
	DWORD len = sizeof(name);
	const char *profile = getenv("USERPROFILE");

	(void)uid;
	if (!GetUserNameA(name, &len))
		return NULL;
	snprintf(home, sizeof(home), "%s", profile ? profile : "");
	pw.pw_name = name;
	pw.pw_passwd = (char *)"";
	pw.pw_gecos = name;
	pw.pw_dir = home;
	pw.pw_shell = (char *)"cmd.exe";
	return &pw;
}

char *realpath(const char *path, char *resolved)
{
	char *r = _fullpath(resolved, path, MAX_PATH);

	if (!r)
		return NULL;
	if (GetFileAttributesA(r) == INVALID_FILE_ATTRIBUTES) {
		errno = ENOENT;
		if (!resolved)
			free(r);
		return NULL;
	}
	return r;
}

int chown(const char *path, uid_t u, gid_t g)
{
	(void)path;
	(void)u;
	(void)g;
	return 0;
}

int fchown(int fd, uid_t u, gid_t g)
{
	(void)fd;
	(void)u;
	(void)g;
	return 0;
}

int fchmod(int fd, mode_t mode)
{
	(void)fd;
	(void)mode;
	return 0;
}

ssize_t readlink(const char *path, char *buf, size_t len)
{
	(void)path;
	(void)buf;
	(void)len;
	errno = EINVAL;
	return -1;
}

int symlink(const char *target, const char *path)
{
	(void)target;
	(void)path;
	errno = ENOSYS;
	return -1;
}

int link(const char *target, const char *path)
{
	if (CreateHardLinkA(path, target, NULL))
		return 0;
	errno = win_errno(GetLastError());
	return -1;
}

int frr_win_mkstemp(char *tmpl)
{
	if (_mktemp_s(tmpl, strlen(tmpl) + 1)) {
		errno = EINVAL;
		return -1;
	}
	return _open(tmpl, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY,
		     _S_IREAD | _S_IWRITE);
}

char *basename(char *path)
{
	static char dot[] = ".";
	char *p, *end;

	if (!path || !*path)
		return dot;
	end = path + strlen(path) - 1;
	while (end > path && (*end == '/' || *end == '\\'))
		*end-- = '\0';
	for (p = end; p > path; p--)
		if (p[-1] == '/' || p[-1] == '\\')
			return p;
	return path;
}

char *dirname(char *path)
{
	static char dot[] = ".";
	char *end;

	if (!path || !*path)
		return dot;
	end = path + strlen(path) - 1;
	while (end > path && (*end == '/' || *end == '\\'))
		end--;
	while (end > path && *end != '/' && *end != '\\')
		end--;
	if (end == path)
		return (*end == '/' || *end == '\\') ? (end[1] = '\0', path)
						     : dot;
	while (end > path && (end[-1] == '/' || end[-1] == '\\'))
		end--;
	*end = '\0';
	return path;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, long long off)
{
	HANDLE fh, map;
	DWORD protect, access;
	void *p;

	(void)addr;
	if (flags & MAP_ANONYMOUS) {
		p = VirtualAlloc(NULL, len, MEM_RESERVE | MEM_COMMIT,
				 PAGE_READWRITE);
		if (!p) {
			errno = ENOMEM;
			return MAP_FAILED;
		}
		return p;
	}

	fh = (HANDLE)_get_osfhandle(fd);
	if (fh == INVALID_HANDLE_VALUE) {
		errno = EBADF;
		return MAP_FAILED;
	}
	protect = (prot & PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
	access = (prot & PROT_WRITE) ? FILE_MAP_WRITE : FILE_MAP_READ;
	map = CreateFileMappingA(fh, NULL, protect, 0, 0, NULL);
	if (!map) {
		errno = win_errno(GetLastError());
		return MAP_FAILED;
	}
	p = MapViewOfFile(map, access, (DWORD)(off >> 32), (DWORD)off, len);
	CloseHandle(map);
	if (!p) {
		errno = win_errno(GetLastError());
		return MAP_FAILED;
	}
	return p;
}

int munmap(void *addr, size_t len)
{
	(void)len;
	if (UnmapViewOfFile(addr) || VirtualFree(addr, 0, MEM_RELEASE))
		return 0;
	errno = EINVAL;
	return -1;
}

int msync(void *addr, size_t len, int flags)
{
	(void)flags;
	return FlushViewOfFile(addr, len) ? 0 : -1;
}

#define EPOCH_DIFF_100NS 116444736000000000ULL

static void filetime_to_ts(ULONGLONG t100ns, struct timespec *ts)
{
	ts->tv_sec = (time_t)(t100ns / 10000000ULL);
	ts->tv_nsec = (long)((t100ns % 10000000ULL) * 100);
}

int clock_gettime(clockid_t clk, struct timespec *ts)
{
	FILETIME ct, et, kt, ut;
	ULARGE_INTEGER k, u;

	switch (clk) {
	case CLOCK_REALTIME:
	case CLOCK_REALTIME_COARSE: {
		FILETIME ft;
		ULARGE_INTEGER t;

		GetSystemTimePreciseAsFileTime(&ft);
		t.LowPart = ft.dwLowDateTime;
		t.HighPart = ft.dwHighDateTime;
		filetime_to_ts(t.QuadPart - EPOCH_DIFF_100NS, ts);
		return 0;
	}
	case CLOCK_MONOTONIC:
	case CLOCK_MONOTONIC_RAW:
	case CLOCK_MONOTONIC_COARSE:
	case CLOCK_BOOTTIME: {
		static LARGE_INTEGER freq;
		LARGE_INTEGER c;

		if (!freq.QuadPart)
			QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&c);
		ts->tv_sec = (time_t)(c.QuadPart / freq.QuadPart);
		ts->tv_nsec = (long)((c.QuadPart % freq.QuadPart) *
				     1000000000LL / freq.QuadPart);
		return 0;
	}
	case CLOCK_PROCESS_CPUTIME_ID:
		if (!GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut))
			break;
		goto cpu;
	case CLOCK_THREAD_CPUTIME_ID:
		if (!GetThreadTimes(GetCurrentThread(), &ct, &et, &kt, &ut))
			break;
	cpu:
		k.LowPart = kt.dwLowDateTime;
		k.HighPart = kt.dwHighDateTime;
		u.LowPart = ut.dwLowDateTime;
		u.HighPart = ut.dwHighDateTime;
		filetime_to_ts(k.QuadPart + u.QuadPart, ts);
		return 0;
	}
	errno = EINVAL;
	return -1;
}

long frr_win_gmtoff(const struct tm *tm)
{
	struct tm t = *tm;
	time_t as_utc, as_local;

	/* same broken-down time read as UTC and as local time */
	as_utc = _mkgmtime(&t);
	t = *tm;
	as_local = mktime(&t);
	if (as_utc == (time_t)-1 || as_local == (time_t)-1)
		return 0;
	return (long)(as_utc - as_local);
}

int clock_getres(clockid_t clk, struct timespec *ts)
{
	(void)clk;
	ts->tv_sec = 0;
	ts->tv_nsec = 100;
	return 0;
}

int gettimeofday(struct timeval *tv, void *tz)
{
	struct timespec ts;

	(void)tz;
	clock_gettime(CLOCK_REALTIME, &ts);
	tv->tv_sec = (long)ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	return 0;
}

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	long long ms = req->tv_sec * 1000LL + (req->tv_nsec + 999999) / 1000000;

	Sleep((DWORD)ms);
	if (rem)
		rem->tv_sec = rem->tv_nsec = 0;
	return 0;
}

int usleep(useconds_t usec)
{
	Sleep((usec + 999) / 1000);
	return 0;
}

unsigned int sleep(unsigned int seconds)
{
	Sleep(seconds * 1000);
	return 0;
}

int getrusage(int who, struct rusage *ru)
{
	FILETIME ct, et, kt, ut;
	ULARGE_INTEGER k, u;
	BOOL ok;

	memset(ru, 0, sizeof(*ru));
	if (who == RUSAGE_THREAD)
		ok = GetThreadTimes(GetCurrentThread(), &ct, &et, &kt, &ut);
	else
		ok = GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut);
	if (!ok) {
		errno = EINVAL;
		return -1;
	}
	k.LowPart = kt.dwLowDateTime;
	k.HighPart = kt.dwHighDateTime;
	u.LowPart = ut.dwLowDateTime;
	u.HighPart = ut.dwHighDateTime;
	ru->ru_utime.tv_sec = (long)(u.QuadPart / 10000000ULL);
	ru->ru_utime.tv_usec = (long)((u.QuadPart % 10000000ULL) / 10);
	ru->ru_stime.tv_sec = (long)(k.QuadPart / 10000000ULL);
	ru->ru_stime.tv_usec = (long)((k.QuadPart % 10000000ULL) / 10);
	return 0;
}

int getrlimit(int resource, struct rlimit *rlim)
{
	switch (resource) {
	case RLIMIT_NOFILE:
		rlim->rlim_cur = rlim->rlim_max = FD_TABLE_SIZE;
		return 0;
	case RLIMIT_CORE:
		rlim->rlim_cur = rlim->rlim_max = 0;
		return 0;
	default:
		rlim->rlim_cur = rlim->rlim_max = RLIM_INFINITY;
		return 0;
	}
}

int setrlimit(int resource, const struct rlimit *rlim)
{
	(void)resource;
	(void)rlim;
	return 0;
}

long sysconf(int name)
{
	SYSTEM_INFO si;

	GetNativeSystemInfo(&si);
	switch (name) {
	case _SC_OPEN_MAX:
		return FD_TABLE_SIZE;
	case _SC_PAGESIZE:
		return si.dwPageSize;
	case _SC_NPROCESSORS_ONLN:
		return si.dwNumberOfProcessors;
	case _SC_CLK_TCK:
		return 100;
	default:
		errno = EINVAL;
		return -1;
	}
}

int getloadavg(double loadavg[], int nelem)
{
	(void)loadavg;
	(void)nelem;
	return -1;
}

int uname(struct utsname *name)
{
	typedef LONG(WINAPI * rtlgetversion_t)(PRTL_OSVERSIONINFOW);
	RTL_OSVERSIONINFOW vi = { .dwOSVersionInfoSize = sizeof(vi) };
	rtlgetversion_t rgv;
	SYSTEM_INFO si;
	DWORD len = sizeof(name->nodename);

	memset(name, 0, sizeof(*name));
	strcpy_s(name->sysname, sizeof(name->sysname), "Windows");
	if (!GetComputerNameExA(ComputerNameDnsHostname, name->nodename, &len))
		strcpy_s(name->nodename, sizeof(name->nodename), "localhost");

	rgv = (rtlgetversion_t)(void *)GetProcAddress(
		GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
	if (rgv && rgv(&vi) == 0) {
		snprintf(name->release, sizeof(name->release), "%lu.%lu",
			 vi.dwMajorVersion, vi.dwMinorVersion);
		snprintf(name->version, sizeof(name->version), "build %lu",
			 vi.dwBuildNumber);
	}

	GetNativeSystemInfo(&si);
	switch (si.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_ARM64:
		strcpy_s(name->machine, sizeof(name->machine), "aarch64");
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
		strcpy_s(name->machine, sizeof(name->machine), "x86_64");
		break;
	case PROCESSOR_ARCHITECTURE_INTEL:
		strcpy_s(name->machine, sizeof(name->machine), "i686");
		break;
	default:
		strcpy_s(name->machine, sizeof(name->machine), "unknown");
	}
	return 0;
}

int frr_win_gethostname(char *name, size_t len)
{
	return gethostname(name, (int)len) ? sock_fail() : 0;
}

int frr_win_inet_aton(const char *cp, struct in_addr *inp)
{
	unsigned long a;

	if (inet_pton(AF_INET, cp, inp) == 1)
		return 1;
	/* inet_aton also accepts the short forms "10.1", "0x0a000001", ... */
	a = inet_addr(cp);
	if (a == INADDR_NONE && strcmp(cp, "255.255.255.255"))
		return 0;
	inp->s_addr = a;
	return 1;
}

char *strndup(const char *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *r = malloc(len + 1);

	if (!r)
		return NULL;
	memcpy(r, s, len);
	r[len] = '\0';
	return r;
}

char *strsep(char **stringp, const char *delim)
{
	char *s = *stringp, *e;

	if (!s)
		return NULL;
	e = s + strcspn(s, delim);
	if (*e)
		*e++ = '\0';
	else
		e = NULL;
	*stringp = e;
	return s;
}

char *strcasestr(const char *haystack, const char *needle)
{
	size_t nl = strlen(needle);

	for (; *haystack; haystack++)
		if (!_strnicmp(haystack, needle, nl))
			return (char *)haystack;
	return nl ? NULL : (char *)haystack;
}

void *memmem(const void *h, size_t hl, const void *n, size_t nl)
{
	const unsigned char *p = h;

	if (!nl)
		return (void *)h;
	for (; hl >= nl; p++, hl--)
		if (*p == *(const unsigned char *)n && !memcmp(p, n, nl))
			return (void *)p;
	return NULL;
}

void *memrchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (const unsigned char *)s + n;

	while (n--)
		if (*--p == (unsigned char)c)
			return (void *)p;
	return NULL;
}

char *stpcpy(char *dst, const char *src)
{
	size_t len = strlen(src);

	memcpy(dst, src, len + 1);
	return dst + len;
}

int vasprintf(char **strp, const char *fmt, va_list ap)
{
	va_list ap2;
	int len;

	va_copy(ap2, ap);
	len = _vscprintf(fmt, ap2);
	va_end(ap2);
	if (len < 0 || !(*strp = malloc((size_t)len + 1)))
		return -1;
	return vsnprintf(*strp, (size_t)len + 1, fmt, ap);
}

int asprintf(char **strp, const char *fmt, ...)
{
	va_list ap;
	int rv;

	va_start(ap, fmt);
	rv = vasprintf(strp, fmt, ap);
	va_end(ap);
	return rv;
}

/* syslog: XXX todo use Windows' log services */

/* syslog() like API to write to the Application event log of Windows */
#define EVENTLOG_SOURCES L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\"

/* MessageId of the message of lib/win32/eventlog.mc */
#define FRR_EVENT_TEXT 1

/* longest message, in characters (the Event Log supports up to 31839) */
#define SYSLOG_MSG_MAX 4096

static char syslog_ident[64] = "frr";
static HANDLE syslog_handle;
static int syslog_mask = 0xff;
static SRWLOCK syslog_lock = SRWLOCK_INIT;

/* the registry key of an event source, pointing to this executable */
static void syslog_register(const wchar_t *source)
{
	DWORD types = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
	wchar_t exe[MAX_PATH], cur[MAX_PATH], key[256];
	DWORD len, size = sizeof(cur);
	HKEY k;

	len = GetModuleFileNameW(NULL, exe, MAX_PATH);
	if (!len || len >= MAX_PATH)
		return;
	if (swprintf(key, _countof(key), EVENTLOG_SOURCES L"%ls", source) < 0)
		return;

	/* already right: nothing to write, which needs no privilege */
	if (RegGetValueW(HKEY_LOCAL_MACHINE, key, L"EventMessageFile",
			 RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, NULL, cur,
			 &size) == ERROR_SUCCESS &&
	    !_wcsicmp(cur, exe))
		return;

	if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL))
		return;
	RegSetValueExW(k, L"EventMessageFile", 0, REG_EXPAND_SZ, (const BYTE *)exe,
		       (len + 1) * sizeof(wchar_t));
	RegSetValueExW(k, L"TypesSupported", 0, REG_DWORD, (const BYTE *)&types, sizeof(types));
	RegCloseKey(k);
}

void openlog(const char *ident, int option, int facility)
{
	wchar_t source[96];

	(void)option;
	(void)facility;

	AcquireSRWLockExclusive(&syslog_lock);
	if (ident) {
		const char *base = ident, *p;
		size_t len;

		/* argv[0] for FRR, a path on Windows: the program name only */
		for (p = ident; *p; p++)
			if (*p == '/' || *p == '\\' || *p == ':')
				base = p + 1;
		len = strlen(base);
		if (len > 4 && !_stricmp(base + len - 4, ".exe"))
			len -= 4;
		if (len >= sizeof(syslog_ident))
			len = sizeof(syslog_ident) - 1;
		strncpy_s(syslog_ident, sizeof(syslog_ident), base, len);
	}
	if (syslog_handle)
		DeregisterEventSource(syslog_handle);
	syslog_handle = NULL;
	if (swprintf(source, _countof(source), L"FRR %hs", syslog_ident) > 0) {
		syslog_register(source);
		syslog_handle = RegisterEventSourceW(NULL, source);
	}
	ReleaseSRWLockExclusive(&syslog_lock);
}

void vsyslog(int priority, const char *fmt, va_list ap)
{
	char buf[SYSLOG_MSG_MAX];
	wchar_t text[SYSLOG_MSG_MAX];
	const wchar_t *strings[1] = { text };
	int level = LOG_PRI(priority);
	bool opened;
	WORD type;

	if (!(LOG_MASK(level) & syslog_mask))
		return;

	vsnprintf(buf, sizeof(buf), fmt, ap);

	/* still visible in a debugger, as before */
	OutputDebugStringA(buf);

	if (!MultiByteToWideChar(CP_UTF8, 0, buf, -1, text, _countof(text)) &&
	    !MultiByteToWideChar(CP_ACP, 0, buf, -1, text, _countof(text)))
		return;
	text[_countof(text) - 1] = L'\0';

	if (level <= LOG_ERR)
		type = EVENTLOG_ERROR_TYPE;
	else if (level == LOG_WARNING)
		type = EVENTLOG_WARNING_TYPE;
	else
		type = EVENTLOG_INFORMATION_TYPE;

	/* syslog() without openlog() first */
	AcquireSRWLockShared(&syslog_lock);
	opened = syslog_handle != NULL;
	ReleaseSRWLockShared(&syslog_lock);
	if (!opened)
		openlog(NULL, 0, 0);

	AcquireSRWLockShared(&syslog_lock);
	if (syslog_handle)
		ReportEventW(syslog_handle, type, 0, FRR_EVENT_TEXT, NULL, 1, 0, strings, NULL);
	ReleaseSRWLockShared(&syslog_lock);
}

void syslog(int priority, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(priority, fmt, ap);
	va_end(ap);
}

void closelog(void)
{
	AcquireSRWLockExclusive(&syslog_lock);
	if (syslog_handle)
		DeregisterEventSource(syslog_handle);
	syslog_handle = NULL;
	ReleaseSRWLockExclusive(&syslog_lock);
}

int setlogmask(int mask)
{
	int old = syslog_mask;

	if (mask)
		syslog_mask = mask;
	return old;
}

static char dl_err[256];

static void dl_seterr(const char *what)
{
	DWORD e = GetLastError();
	int n = snprintf(dl_err, sizeof(dl_err), "%s: ", what);

	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
			       FORMAT_MESSAGE_IGNORE_INSERTS,
		       NULL, e, 0, dl_err + n, (DWORD)(sizeof(dl_err) - n),
		       NULL);
}

void *dlopen(const char *file, int mode)
{
	HMODULE h;

	(void)mode;
	h = file ? LoadLibraryA(file) : GetModuleHandleA(NULL);
	if (!h)
		dl_seterr(file ? file : "(self)");
	return h;
}

void *dlsym(void *handle, const char *name)
{
	FARPROC p = GetProcAddress(handle ? handle : GetModuleHandleA(NULL),
				   name);

	if (!p)
		dl_seterr(name);
	return (void *)p;
}

int dlclose(void *handle)
{
	return FreeLibrary(handle) ? 0 : -1;
}

char *dlerror(void)
{
	static char ret[256];

	if (!dl_err[0])
		return NULL;
	memcpy(ret, dl_err, sizeof(ret));
	dl_err[0] = '\0';
	return ret;
}

int dladdr(const void *addr, Dl_info *info)
{
	static char path[MAX_PATH];
	HMODULE h;

	memset(info, 0, sizeof(*info));
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			       addr, &h))
		return 0;
	GetModuleFileNameA(h, path, sizeof(path));
	info->dli_fname = path;
	info->dli_fbase = h;
	return 1;
}

char *optarg;
int optind = 1, opterr = 1, optopt = '?';
static int optpos;

static int getopt_internal(int argc, char *const argv[], const char *optstring,
			   const struct option *longopts, int *longindex)
{
	bool colon;
	const char *arg, *spec;
	int c;

	if (*optstring == '+' || *optstring == '-')
		optstring++;
	colon = *optstring == ':';

	optarg = NULL;
	if (optind == 0) {
		optind = 1;
		optpos = 0;
	}
	if (optind >= argc || !argv[optind])
		return -1;
	arg = argv[optind];

	if (!optpos) {
		if (arg[0] != '-' || !arg[1])
			return -1;
		if (!strcmp(arg, "--")) {
			optind++;
			return -1;
		}
		if (arg[1] == '-' && longopts) {
			const char *name = arg + 2, *eq = strchr(name, '=');
			size_t nlen = eq ? (size_t)(eq - name) : strlen(name);
			int match = -1, nmatch = 0;

			for (int i = 0; longopts[i].name; i++) {
				if (strncmp(longopts[i].name, name, nlen))
					continue;
				if (strlen(longopts[i].name) == nlen) {
					match = i;
					nmatch = 1;
					break;
				}
				match = i;
				nmatch++;
			}
			optind++;
			if (nmatch != 1) {
				if (opterr && !colon)
					fprintf(stderr, "%s: %s option '%s'\n",
						argv[0],
						nmatch ? "ambiguous"
						       : "unrecognized",
						arg);
				optopt = 0;
				return '?';
			}
			if (longindex)
				*longindex = match;
			if (longopts[match].has_arg == no_argument && eq) {
				if (opterr && !colon)
					fprintf(stderr,
						"%s: option '--%s' doesn't allow an argument\n",
						argv[0], longopts[match].name);
				optopt = longopts[match].val;
				return '?';
			}
			if (eq)
				optarg = (char *)eq + 1;
			else if (longopts[match].has_arg == required_argument) {
				if (optind >= argc) {
					if (opterr && !colon)
						fprintf(stderr,
							"%s: option '--%s' requires an argument\n",
							argv[0],
							longopts[match].name);
					optopt = longopts[match].val;
					return colon ? ':' : '?';
				}
				optarg = argv[optind++];
			}
			if (longopts[match].flag) {
				*longopts[match].flag = longopts[match].val;
				return 0;
			}
			return longopts[match].val;
		}
		optpos = 1;
	}

	c = (unsigned char)arg[optpos++];
	spec = c != ':' ? strchr(optstring, c) : NULL;
	if (!spec) {
		if (opterr && !colon)
			fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0],
				c);
		optopt = c;
		if (!arg[optpos]) {
			optind++;
			optpos = 0;
		}
		return '?';
	}
	if (spec[1] == ':') {
		if (arg[optpos]) {
			optarg = (char *)arg + optpos;
		} else if (spec[2] != ':') {
			if (optind + 1 >= argc) {
				if (opterr && !colon)
					fprintf(stderr,
						"%s: option requires an argument -- '%c'\n",
						argv[0], c);
				optopt = c;
				optind++;
				optpos = 0;
				return colon ? ':' : '?';
			}
			optarg = argv[++optind];
		}
		optind++;
		optpos = 0;
	} else if (!arg[optpos]) {
		optind++;
		optpos = 0;
	}
	return c;
}

int getopt(int argc, char *const argv[], const char *optstring)
{
	return getopt_internal(argc, argv, optstring, NULL, NULL);
}

int getopt_long(int argc, char *const argv[], const char *optstring,
		const struct option *longopts, int *longindex)
{
	return getopt_internal(argc, argv, optstring, longopts, longindex);
}

int getopt_long_only(int argc, char *const argv[], const char *optstring,
		     const struct option *longopts, int *longindex)
{
	return getopt_internal(argc, argv, optstring, longopts, longindex);
}

/* process start-up, runs before any FRR constructor */

static void invalid_param(const wchar_t *expr, const wchar_t *func,
			  const wchar_t *file, unsigned int line,
			  uintptr_t reserved)
{
	/* make the CRT return EBADF/EINVAL like POSIX instead of aborting */
	(void)expr;
	(void)func;
	(void)file;
	(void)line;
	(void)reserved;
}

static void frr_win_startup(void)
{
	WSADATA wsa;
	GUID g = WSAID_WSARECVMSG;
	DWORD n;
	SOCKET s;

	_set_invalid_parameter_handler(invalid_param);
	_CrtSetReportMode(_CRT_ASSERT, 0);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

	if (__argc > 0 && __argv && __argv[0]) {
		char *a0 = __argv[0], *p;
		size_t len;

		for (p = a0; *p; p++)
			if (*p == '\\')
				*p = '/';
		len = strlen(a0);
		if (len > 4 && !_stricmp(a0 + len - 4, ".exe"))
			a0[len - 4] = '\0';
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
		fprintf(stderr, "WSAStartup failed\n");
		_exit(1);
	}

	s = WSASocketW(AF_INET, SOCK_DGRAM, 0, NULL, 0, 0);
	if (s != INVALID_SOCKET) {
		WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &g, sizeof(g),
			 &wsarecvmsg_fn, sizeof(wsarecvmsg_fn), &n, NULL,
			 NULL);
		closesocket(s);
	}

	if (sockpair(sigwake) == 0) {
		u_long nb = 1;

		ioctlsocket(sigwake[0], FIONBIO, &nb);
		ioctlsocket(sigwake[1], FIONBIO, &nb);
	}
	SetConsoleCtrlHandler(console_ctrl, TRUE);
	SetUnhandledExceptionFilter(crash_filter);
	svc_init();
}

#pragma section(".CRT$XCT00010", read)
__declspec(allocate(".CRT$XCT00010")) void (*frr_win_startup_ptr)(void) =
	frr_win_startup;
