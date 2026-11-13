// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <pwd.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_PWD_H
#define _FRR_WIN_PWD_H

struct passwd { char *pw_name; char *pw_passwd; uid_t pw_uid; gid_t pw_gid; char *pw_gecos; char *pw_dir; char *pw_shell; };
static inline struct passwd *getpwnam(const char *n) { (void)n; return NULL; }
extern struct passwd *getpwuid(uid_t uid);
static inline int initgroups(const char *u, gid_t g) { (void)u; (void)g; return 0; }

#endif
