// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows portability prelude.
 *
 * This header is force-included (/FI) ahead of every source file when FRR
 * is built for Windows.
 * wrapper for lib/win32/compat.c and lib/win32/pthread.c.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN32_H
#define _FRR_WIN32_H

#if defined(_WIN32) && !defined(__cplusplus)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 /* Windows 10: AF_UNIX, WSAPoll fixes */
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_NONSTDC_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#endif
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 1
#endif
#ifndef _CRT_RAND_S
#define _CRT_RAND_S
#endif
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <stdint.h>

/*
 * struct in6_addr with a 32-bit view. FRR uses s6_addr32 all over the
 * place
 */
typedef struct in6_addr {
	union {
		unsigned char Byte[16];
		unsigned short Word[8];
		uint32_t Dword[4];
	} u;
} IN6_ADDR, *PIN6_ADDR, *LPIN6_ADDR;
#define in_addr6 in6_addr
#define _S6_un u
#define _S6_u8 Byte
#define s6_addr _S6_un._S6_u8
#define s6_bytes u.Byte
#define s6_words u.Word
#define s6_addr8 u.Byte
#define s6_addr16 u.Word
#define s6_addr32 u.Dword
#define __u6_addr u
#define __u6_addr32 Dword

/* FRR has its own htonll/ntohll/htonf/ntohf */
#define NO_EXTRA_HTON_FUNCTIONS

/* winsock's struct pollfd has a SOCKET member; FRR needs an int one */
#define pollfd _frr_wsapollfd

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <afunix.h>
#include <windows.h>

#undef pollfd

#include <io.h>
#include <direct.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <malloc.h>
#include <limits.h>
#include <stdarg.h>
#include <intrin.h>
#include <stdatomic.h>

/* SDK macros that clash with FRR identifiers */
#undef interface
#undef DELETE
#undef IN
#undef OUT
#undef OPTIONAL
#undef ERROR
#undef small
#undef near
#undef far
/* XXX: no SO_REUSEPORT on Windows: stub it */
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x0200
#endif
#undef FAR
#define FAR
#undef NEAR
#define NEAR
#undef RELATIVE
#undef ABSOLUTE
#undef OPAQUE
#undef TRANSPARENT
#undef STRICT
#undef IGNORE
#undef NO_ERROR
#undef DOMAIN
#undef s_host
#undef s_net
#undef s_imp
#undef s_impno
#undef s_lh
#undef CONST
#undef VOID
/* Windows defines AF_LINK, FRR takes that to mean BSD sockaddr_dl */
#undef AF_LINK
#undef max
#undef min

/* uint32_t as in POSIX, Winsock's are u_long */
#define htonl(x) ((uint32_t)(htonl)(x))
#define ntohl(x) ((uint32_t)(ntohl)(x))

typedef SSIZE_T ssize_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef int mode_t;
typedef unsigned short sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
typedef long suseconds_t;
typedef unsigned int useconds_t;
typedef long long blkcnt_t;
typedef unsigned int nfds_t;
typedef unsigned long long rlim_t;
typedef long long frr_off_t;

#define SSIZE_MAX INTPTR_MAX
#define NGROUPS_MAX 64

#ifdef _WIN64
#define __SIZEOF_POINTER__ 8
#else
#define __SIZEOF_POINTER__ 4
#endif

/* glibc / BSD convenience types */
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
typedef char *caddr_t;

/* errno */

/* the UCRT has the POSIX supplement errno values (EWOULDBLOCK & co.) */
#ifndef ENOTSUP
#define ENOTSUP EOPNOTSUPP
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN 150
#endif
#ifndef EHOSTDOWN
#define EHOSTDOWN 151
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT 152
#endif
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT 153
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS 154
#endif
#ifndef EUSERS
#define EUSERS 155
#endif
#ifndef EDQUOT
#define EDQUOT 156
#endif
#ifndef ESTALE
#define ESTALE 157
#endif
#ifndef EREMOTE
#define EREMOTE 158
#endif
#ifndef EPROCLIM
#define EPROCLIM 159
#endif

extern char *frr_win_strerror(int errnum);
#define strerror(e) frr_win_strerror(e)
extern int frr_win_strerror_r(int errnum, char *buf, size_t buflen);
#define strerror_r(e, b, l) frr_win_strerror_r(e, b, l)

#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strtok_r strtok_s
#define bzero(p, n) memset((p), 0, (n))
#define bcopy(s, d, n) memmove((d), (s), (n))
#define bcmp(a, b, n) memcmp((a), (b), (n))

extern char *strndup(const char *s, size_t n);
/* lib/strlcpy.c, lib/strlcat.c */
extern size_t strlcpy(char *__restrict dst, const char *__restrict src,
		      size_t size);
extern size_t strlcat(char *__restrict dst, const char *__restrict src,
		      size_t size);
extern char *strsep(char **stringp, const char *delim);
#define strcasestr frr_win_strcasestr
extern char *strcasestr(const char *haystack, const char *needle);
extern void *memmem(const void *h, size_t hl, const void *n, size_t nl);
extern void *memrchr(const void *s, int c, size_t n);
extern char *stpcpy(char *dst, const char *src);
extern int vasprintf(char **strp, const char *fmt, va_list ap);
extern int asprintf(char **strp, const char *fmt, ...);
extern char *strsignal(int sig);
extern void explicit_bzero(void *buf, size_t len);
/* lib/win32/crypt.c: DES, $1$, $5$ and $6$ hashes, as glibc makes them */
extern char *crypt(const char *key, const char *salt);
static inline int ffs(int i)
{
	unsigned long r;

	return _BitScanForward(&r, (unsigned long)i) ? (int)r + 1 : 0;
}
static inline int setenv(const char *n, const char *v, int overwrite)
{
	if (!overwrite && getenv(n))
		return 0;
	return _putenv_s(n, v) ? -1 : 0;
}
static inline int unsetenv(const char *n)
{
	return _putenv_s(n, "") ? -1 : 0;
}

static inline long random(void)
{
	unsigned int r = 0;

	rand_s(&r);
	return (long)(r & 0x7fffffff);
}
static inline void srandom(unsigned int seed)
{
	(void)seed;
}
#undef RAND_MAX
#define RAND_MAX 0x7fffffff
#define rand() ((int)random())
#define srand(seed) srandom(seed)

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID 3
#define CLOCK_BOOTTIME 7
#define CLOCK_MONOTONIC_RAW 4
#define CLOCK_REALTIME_COARSE 5
#define CLOCK_MONOTONIC_COARSE 6
#endif
typedef int clockid_t;

extern int clock_gettime(clockid_t clk, struct timespec *ts);
extern int clock_getres(clockid_t clk, struct timespec *ts);
extern int gettimeofday(struct timeval *tv, void *tz);
extern int nanosleep(const struct timespec *req, struct timespec *rem);
extern int usleep(useconds_t usec);
extern unsigned int sleep(unsigned int seconds);
extern unsigned int alarm(unsigned int seconds);

static inline struct tm *localtime_r(const time_t *t, struct tm *tm)
{
	return localtime_s(tm, t) ? NULL : tm;
}
static inline struct tm *gmtime_r(const time_t *t, struct tm *tm)
{
	return gmtime_s(tm, t) ? NULL : tm;
}
static inline char *ctime_r(const time_t *t, char *buf)
{
	return ctime_s(buf, 26, t) ? NULL : buf;
}
static inline char *asctime_r(const struct tm *tm, char *buf)
{
	return asctime_s(buf, 26, tm) ? NULL : buf;
}
#define timegm _mkgmtime
extern long frr_win_gmtoff(const struct tm *tm);
#define TM_GMTOFF(tmp) frr_win_gmtoff(tmp)
#define tzset _tzset

#undef timerisset
#undef timerclear
#undef timercmp
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define timercmp(a, b, CMP)                                                    \
	(((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP(b)->tv_usec)         \
				      : ((a)->tv_sec CMP(b)->tv_sec))
#define timeradd(a, b, result)                                                 \
	do {                                                                   \
		(result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                  \
		(result)->tv_usec = (a)->tv_usec + (b)->tv_usec;               \
		if ((result)->tv_usec >= 1000000) {                            \
			++(result)->tv_sec;                                    \
			(result)->tv_usec -= 1000000;                          \
		}                                                              \
	} while (0)
#define timersub(a, b, result)                                                 \
	do {                                                                   \
		(result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                  \
		(result)->tv_usec = (a)->tv_usec - (b)->tv_usec;               \
		if ((result)->tv_usec < 0) {                                   \
			--(result)->tv_sec;                                    \
			(result)->tv_usec += 1000000;                          \
		}                                                              \
	} while (0)

#define getpid _getpid
extern pid_t getppid(void);
static inline uid_t getuid(void) { return 0; }
static inline uid_t geteuid(void) { return 0; }
static inline gid_t getgid(void) { return 0; }
static inline gid_t getegid(void) { return 0; }
static inline int setuid(uid_t u) { (void)u; return 0; }
static inline int setgid(gid_t g) { (void)g; return 0; }
static inline int seteuid(uid_t u) { (void)u; return 0; }
static inline int setegid(gid_t g) { (void)g; return 0; }
static inline int setreuid(uid_t r, uid_t e) { (void)r; (void)e; return 0; }
static inline int setregid(gid_t r, gid_t e) { (void)r; (void)e; return 0; }
static inline int mkfifo(const char *p, mode_t m)
{
	(void)p;
	(void)m;
	errno = ENOSYS;
	return -1;
}
static inline int setgroups(size_t n, const gid_t *l)
{
	(void)n;
	(void)l;
	return 0;
}
static inline int getgroups(int n, gid_t *l)
{
	(void)n;
	(void)l;
	return 0;
}
extern int kill(pid_t pid, int sig);
/* no fork() on Windows; daemonizing (-d) is not supported */
static inline pid_t fork(void)
{
	errno = ENOSYS;
	return -1;
}
static inline pid_t setsid(void)
{
	errno = ENOSYS;
	return -1;
}
static inline int setpgid(pid_t pid, pid_t pgid)
{
	(void)pid;
	(void)pgid;
	errno = ENOSYS;
	return -1;
}
extern pid_t frr_win_spawn_shell(const char *cmd);
extern long sysconf(int name);
#define _SC_OPEN_MAX 4
#define _SC_PAGESIZE 30
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_NPROCESSORS_ONLN 84
#define _SC_CLK_TCK 2
extern int getloadavg(double loadavg[], int nelem);

#define SIGHUP 1
#define SIGQUIT 3
#define SIGTRAP 5
#define SIGBUS 7
#define SIGKILL 9
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 26
#define SIGTTOU 27
#define SIGWINCH 28
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGSYS 31
#define FRR_WIN_NSIG 32

typedef unsigned long long sigset_t;

union sigval {
	int sival_int;
	void *sival_ptr;
};

typedef struct {
	int si_signo;
	int si_errno;
	int si_code;
	pid_t si_pid;
	uid_t si_uid;
	void *si_addr;
	int si_status;
	long si_band;
	union sigval si_value;
} siginfo_t;

struct sigaction {
	union {
		void (*sa_handler)(int);
		void (*sa_sigaction)(int, siginfo_t *, void *);
	};
	sigset_t sa_mask;
	int sa_flags;
};

#define SA_SIGINFO 0x4
#define SA_RESTART 0x10000000
#define SA_RESETHAND 0x80000000
#define SA_NODEFER 0x40000000
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

static inline int sigemptyset(sigset_t *s)
{
	*s = 0;
	return 0;
}
static inline int sigfillset(sigset_t *s)
{
	*s = ~0ULL;
	return 0;
}
static inline int sigaddset(sigset_t *s, int sig)
{
	*s |= 1ULL << (sig & 63);
	return 0;
}
static inline int sigdelset(sigset_t *s, int sig)
{
	*s &= ~(1ULL << (sig & 63));
	return 0;
}
static inline int sigismember(const sigset_t *s, int sig)
{
	return (*s >> (sig & 63)) & 1;
}
extern int sigaction(int sig, const struct sigaction *act,
		     struct sigaction *oact);
extern int sigprocmask(int how, const sigset_t *set, sigset_t *oset);
extern int pthread_sigmask(int how, const sigset_t *set, sigset_t *oset);
extern int sigpending(sigset_t *set);

/* deliver an emulated signal (console control handler, kill(getpid())) */
extern void frr_win_raise_signal(int sig);
typedef void (*frr_win_sighandler_t)(int);
extern frr_win_sighandler_t frr_win_signal(int sig, frr_win_sighandler_t h);

#ifndef O_CLOEXEC
#define O_CLOEXEC _O_NOINHERIT
#endif
#define O_NOCTTY 0
#define O_NOFOLLOW 0
#define O_NONBLOCK 0x4000000
#define O_DIRECTORY 0x8000000
#define O_SYNC 0
#define O_NDELAY O_NONBLOCK

/* record locks, emulated with LockFileEx() */
struct flock {
	short l_type;
	short l_whence;
	long long l_start;
	long long l_len;
	pid_t l_pid;
};
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7
#define F_GETFD 1
#define F_SETFD 2
#define F_GETFL 3
#define F_SETFL 4
#define FD_CLOEXEC 1

#define S_IRWXU (_S_IREAD | _S_IWRITE | _S_IEXEC)
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IXUSR _S_IEXEC
#define S_IRWXG 0
#define S_IRGRP 0
#define S_IWGRP 0
#define S_IXGRP 0
#define S_IRWXO 0
#define S_IROTH 0
#define S_IWOTH 0
#define S_IXOTH 0
#define S_ISUID 0
#define S_ISGID 0
#define S_ISVTX 0
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#define S_ISLNK(m) 0
/* files AF_UNIX sockets are bound to, see frr_win_stat() */
#define S_IFSOCK 0xC000
#define S_ISSOCK(m) (((m) & _S_IFMT) == S_IFSOCK)
#define S_ISFIFO(m) (((m) & _S_IFMT) == _S_IFIFO)
#define S_ISCHR(m) (((m) & _S_IFMT) == _S_IFCHR)
#define S_ISBLK(m) 0

#define R_OK 4
#define W_OK 2
#define X_OK 0 /* not meaningful on Windows, treat as existence check */
#define F_OK 0

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define PATH_MAX MAX_PATH
#define MAXPATHLEN MAX_PATH
/* "C:/..." and "\\server\..." are absolute paths too */
#define IS_ABSOLUTE_PATH(p)                                                    \
	((p)[0] == '/' || (p)[0] == '\\' ||                                    \
	 (isalpha((unsigned char)(p)[0]) && (p)[1] == ':'))

extern int frr_win_open(const char *path, int flags, ...);
extern int frr_win_mkdir(const char *path, mode_t mode);
extern int frr_win_rename(const char *from, const char *to);
extern int frr_win_unlink(const char *path);
extern int fsync(int fd);
extern int ftruncate(int fd, long long len);
extern int frr_win_isatty(int fd);
struct stat;
extern int frr_win_stat(const char *path, struct stat *st);
extern char *realpath(const char *path, char *resolved);
extern int chown(const char *path, uid_t u, gid_t g);
extern int fchown(int fd, uid_t u, gid_t g);
extern int fchmod(int fd, mode_t mode);
extern ssize_t readlink(const char *path, char *buf, size_t len);
extern int symlink(const char *target, const char *path);
extern int link(const char *target, const char *path);
#define lstat stat
/* *at() calls on a directory fd from open(O_DIRECTORY) */
#define AT_FDCWD (-100)
#define AT_REMOVEDIR 0x200
extern int openat(int dirfd, const char *path, int flags, ...);
extern int renameat(int olddirfd, const char *oldpath, int newdirfd,
		    const char *newpath);
extern int unlinkat(int dirfd, const char *path, int flags);
extern int frr_win_mkstemp(char *tmpl);
#define mkstemp frr_win_mkstemp

#define MSG_DONTWAIT 0x40000000 /* emulated: socket temporarily non-blocking */
#define MSG_NOSIGNAL 0
#define MSG_EOR 0
/* no descriptor passing over AF_UNIX on Windows; sendmsg() drops it */
#define SCM_RIGHTS 1
#ifndef MSG_TRUNC
#define MSG_TRUNC 0x0100
#endif
#ifndef MSG_CTRUNC
#define MSG_CTRUNC 0x0200
#endif
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#define SOCK_NONBLOCK 0x800000
#define SOCK_CLOEXEC 0x400000
#define AF_LOCAL AF_UNIX
#define PF_LOCAL AF_UNIX
#define PF_UNIX AF_UNIX
#ifndef AF_PACKET
#define AF_PACKET 17
#endif

struct iovec {
	void *iov_base;
	size_t iov_len;
};
#define IOV_MAX 1024
#define UIO_MAXIOV IOV_MAX

/* POSIX msghdr; the control buffer layout matches WSACMSGHDR */
struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
	size_t msg_iovlen;
	void *msg_control;
	size_t msg_controllen;
	int msg_flags;
};

#undef CMSG_FIRSTHDR
#undef CMSG_NXTHDR
#undef CMSG_DATA
#undef CMSG_SPACE
#undef CMSG_LEN
#undef CMSG_ALIGN
#define CMSG_ALIGN(len) WSA_CMSGDATA_ALIGN(len)
#define CMSG_DATA(cmsg) ((unsigned char *)(cmsg) + WSA_CMSGDATA_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_SPACE(len) (WSA_CMSGDATA_ALIGN(sizeof(struct cmsghdr) + WSA_CMSGHDR_ALIGN(len)))
#define CMSG_LEN(len) (WSA_CMSGDATA_ALIGN(sizeof(struct cmsghdr)) + (len))
#define CMSG_FIRSTHDR(msg)                                                     \
	((msg)->msg_controllen >= sizeof(struct cmsghdr)                       \
		 ? (struct cmsghdr *)(msg)->msg_control                        \
		 : (struct cmsghdr *)NULL)
extern struct cmsghdr *frr_win_cmsg_nxthdr(struct msghdr *msg,
					   struct cmsghdr *cmsg);
#define CMSG_NXTHDR(msg, cmsg) frr_win_cmsg_nxthdr((msg), (cmsg))

struct pollfd {
	int fd;
	short events;
	short revents;
};

/* fd <-> SOCKET mapping */
extern SOCKET frr_win_fd2sock(int fd);
extern int frr_win_sock2fd(SOCKET s);
extern SOCKET frr_win_fd_detach(int fd);
extern int frr_win_fd_adopt(SOCKET s);
extern void frr_win_fd_unadopt(int fd);

extern void frr_win_module_builtin(const char *name, void *frr_module);
extern int frr_win_select(int nfds, fd_set *rd, fd_set *wr, fd_set *ex, struct timeval *tv);
extern int frr_win_is_socket(int fd);

extern int frr_win_socket(int domain, int type, int protocol);
extern int frr_win_accept(int fd, struct sockaddr *addr, socklen_t *len);
extern int frr_win_bind(int fd, const struct sockaddr *addr, socklen_t len);
extern int frr_win_connect(int fd, const struct sockaddr *addr, socklen_t len);
extern int frr_win_listen(int fd, int backlog);
extern int frr_win_getsockopt(int fd, int level, int opt, void *val,
			      socklen_t *len);
extern int frr_win_setsockopt(int fd, int level, int opt, const void *val,
			      socklen_t len);
extern int frr_win_getsockname(int fd, struct sockaddr *addr, socklen_t *len);
extern int frr_win_getpeername(int fd, struct sockaddr *addr, socklen_t *len);
extern int frr_win_shutdown(int fd, int how);
extern ssize_t frr_win_send(int fd, const void *buf, size_t len, int flags);
extern ssize_t frr_win_recv(int fd, void *buf, size_t len, int flags);
extern ssize_t frr_win_sendto(int fd, const void *buf, size_t len, int flags,
			      const struct sockaddr *to, socklen_t tolen);
extern ssize_t frr_win_recvfrom(int fd, void *buf, size_t len, int flags,
				struct sockaddr *from, socklen_t *fromlen);
extern ssize_t frr_win_sendmsg(int fd, const struct msghdr *msg, int flags);
extern ssize_t frr_win_recvmsg(int fd, struct msghdr *msg, int flags);
extern int frr_win_socketpair(int domain, int type, int protocol, int sv[2]);
extern int frr_win_pipe(int fds[2]);
extern int frr_win_poll(struct pollfd *fds, nfds_t nfds, int timeout);
extern ssize_t frr_win_read(int fd, void *buf, size_t len);
extern ssize_t frr_win_write(int fd, const void *buf, size_t len);
extern ssize_t frr_win_readv(int fd, const struct iovec *iov, int iovcnt);
extern ssize_t frr_win_writev(int fd, const struct iovec *iov, int iovcnt);
extern int frr_win_close(int fd);
extern int frr_win_fcntl(int fd, int cmd, ...);
extern int frr_win_ioctl(int fd, unsigned long req, ...);
/* not available: ioctl() fails with ENOTTY, like on a non-tty elsewhere */
#define TIOCOUTQ 0x5411
extern int frr_win_dup(int fd);
extern int frr_win_dup2(int fd, int fd2);
extern int frr_win_gethostname(char *name, size_t len);
extern int frr_win_inet_aton(const char *cp, struct in_addr *inp);

#ifndef FRR_WIN32_COMPAT_IMPL
#define socket(...) frr_win_socket(__VA_ARGS__)
#define accept(...) frr_win_accept(__VA_ARGS__)
#define bind(...) frr_win_bind(__VA_ARGS__)
#define connect(...) frr_win_connect(__VA_ARGS__)
#define listen(...) frr_win_listen(__VA_ARGS__)
#define getsockopt(...) frr_win_getsockopt(__VA_ARGS__)
#define setsockopt(...) frr_win_setsockopt(__VA_ARGS__)
#define getsockname(...) frr_win_getsockname(__VA_ARGS__)
#define getpeername(...) frr_win_getpeername(__VA_ARGS__)
#define shutdown(...) frr_win_shutdown(__VA_ARGS__)
#define send(...) frr_win_send(__VA_ARGS__)
#define recv(...) frr_win_recv(__VA_ARGS__)
#define sendto(...) frr_win_sendto(__VA_ARGS__)
#define recvfrom(...) frr_win_recvfrom(__VA_ARGS__)
#define sendmsg(...) frr_win_sendmsg(__VA_ARGS__)
#define recvmsg(...) frr_win_recvmsg(__VA_ARGS__)
#define socketpair(...) frr_win_socketpair(__VA_ARGS__)
#define pipe(...) frr_win_pipe(__VA_ARGS__)
#define poll(...) frr_win_poll(__VA_ARGS__)
#define select(...) frr_win_select(__VA_ARGS__)
#define read(...) frr_win_read(__VA_ARGS__)
#define write(...) frr_win_write(__VA_ARGS__)
#define readv(...) frr_win_readv(__VA_ARGS__)
#define writev(...) frr_win_writev(__VA_ARGS__)
#define close(...) frr_win_close(__VA_ARGS__)
#define fcntl(...) frr_win_fcntl(__VA_ARGS__)
#define ioctl(...) frr_win_ioctl(__VA_ARGS__)
#define dup(...) frr_win_dup(__VA_ARGS__)
#define dup2(...) frr_win_dup2(__VA_ARGS__)
#define open(...) frr_win_open(__VA_ARGS__)
#define mkdir(...) frr_win_mkdir(__VA_ARGS__)
#define rename(...) frr_win_rename(__VA_ARGS__)
#define unlink(...) frr_win_unlink(__VA_ARGS__)
#define isatty(...) frr_win_isatty(__VA_ARGS__)
#define stat(path, st) frr_win_stat(path, st)
#define gethostname(...) frr_win_gethostname(__VA_ARGS__)
#define inet_aton(...) frr_win_inet_aton(__VA_ARGS__)
#define lseek(...) _lseek(__VA_ARGS__)
#define fileno(...) _fileno(__VA_ARGS__)
#define fdopen(...) _fdopen(__VA_ARGS__)
#define umask(...) _umask(__VA_ARGS__)
#define chmod(...) _chmod(__VA_ARGS__)
#define access(...) _access(__VA_ARGS__)
#define rmdir(...) _rmdir(__VA_ARGS__)
#define getcwd(...) _getcwd(__VA_ARGS__)
#define chdir(...) _chdir(__VA_ARGS__)
#define popen(...) _popen(__VA_ARGS__)
#define setlinebuf(stream) setvbuf((stream), NULL, _IONBF, 0)
#define pclose(...) _pclose(__VA_ARGS__)
#define signal(...) frr_win_signal(__VA_ARGS__)
#endif /* FRR_WIN32_COMPAT_IMPL */

#endif /* _WIN32 && !__cplusplus */
#endif /* _FRR_WIN32_H */
