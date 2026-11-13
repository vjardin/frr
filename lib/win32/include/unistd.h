// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <unistd.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_UNISTD_H
#define _FRR_WIN_UNISTD_H

#include "getopt.h"

/* POSIX exec*() take char *const argv[], the C runtime's const char *const * */
#define execv(path, argv) _execv(path, (const char *const *)(argv))
#define execvp(file, argv) _execvp(file, (const char *const *)(argv))

#endif
