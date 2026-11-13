// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sys/file.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SYS_FILE_H
#define _FRR_WIN_SYS_FILE_H

#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
extern int flock(int fd, int operation);

#endif
