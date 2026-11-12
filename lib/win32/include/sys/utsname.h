// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sys/utsname.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SYS_UTSNAME_H
#define _FRR_WIN_SYS_UTSNAME_H

struct utsname {
	char sysname[65];
	char nodename[257];
	char release[65];
	char version[65];
	char machine[65];
};
extern int uname(struct utsname *name);

#endif
