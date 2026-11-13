// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sys/resource.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SYS_RESOURCE_H
#define _FRR_WIN_SYS_RESOURCE_H

#define RLIMIT_CORE 4
#define RLIMIT_NOFILE 7
#define RLIM_INFINITY (~0ULL)
struct rlimit { rlim_t rlim_cur; rlim_t rlim_max; };
extern int getrlimit(int resource, struct rlimit *rlim);
extern int setrlimit(int resource, const struct rlimit *rlim);
#define RUSAGE_SELF 0
#define RUSAGE_CHILDREN (-1)
#define RUSAGE_THREAD 1
struct rusage { struct timeval ru_utime; struct timeval ru_stime; long ru_maxrss; long ru_minflt; long ru_majflt; long ru_nvcsw; long ru_nivcsw; };
extern int getrusage(int who, struct rusage *usage);

#endif
