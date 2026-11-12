// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <execinfo.h>
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_EXECINFO_H
#define _FRR_WIN_EXECINFO_H

#ifdef __cplusplus
extern "C" {
#endif

extern int backtrace(void **buffer, int size);
extern char **backtrace_symbols(void *const *buffer, int size);
extern void backtrace_symbols_fd(void *const *buffer, int size, int fd);

#ifdef __cplusplus
}
#endif

#endif
