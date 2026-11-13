// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <dlfcn.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_DLFCN_H
#define _FRR_WIN_DLFCN_H
#define RTLD_LAZY 1
#define RTLD_NOW 2
#define RTLD_GLOBAL 0x100
#define RTLD_LOCAL 0
#define RTLD_DEFAULT ((void *)0)
typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } Dl_info;
extern void *dlopen(const char *file, int mode);
extern void *dlsym(void *handle, const char *name);
extern int dlclose(void *handle);
extern char *dlerror(void);
extern int dladdr(const void *addr, Dl_info *info);
#endif
