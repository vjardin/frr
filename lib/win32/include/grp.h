// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <grp.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_GRP_H
#define _FRR_WIN_GRP_H
struct group { char *gr_name; char *gr_passwd; gid_t gr_gid; char **gr_mem; };
static inline struct group *getgrnam(const char *n) { (void)n; return NULL; }
static inline struct group *getgrgid(gid_t g) { (void)g; return NULL; }
static inline void setgrent(void) { }
static inline struct group *getgrent(void) { return NULL; }
static inline void endgrent(void) { }
#endif
