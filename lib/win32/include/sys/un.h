// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sys/un.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SYS_UN_H
#define _FRR_WIN_SYS_UN_H

#ifndef SUN_LEN
#define SUN_LEN(su) (offsetof(struct sockaddr_un, sun_path) + strlen((su)->sun_path))
#endif

#endif
