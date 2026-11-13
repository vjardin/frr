// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sys/mman.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SYS_MMAN_H
#define _FRR_WIN_SYS_MMAN_H

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS
#define MAP_FAILED ((void *)-1)
#define MS_ASYNC 1
#define MS_SYNC 4
#define MS_INVALIDATE 2
extern void *mmap(void *addr, size_t len, int prot, int flags, int fd, long long off);
extern int munmap(void *addr, size_t len);
extern int msync(void *addr, size_t len, int flags);

#endif
