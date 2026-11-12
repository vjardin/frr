// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <libgen.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_LIBGEN_H
#define _FRR_WIN_LIBGEN_H
extern char *basename(char *path);
extern char *dirname(char *path);
#endif
