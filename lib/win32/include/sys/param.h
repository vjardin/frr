// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sys/param.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SYS_PARAM_H
#define _FRR_WIN_SYS_PARAM_H

#include <endian.h>
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif
#ifndef NBBY
#define NBBY 8
#endif

#endif
