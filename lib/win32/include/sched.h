// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sched.h>n most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SCHED_H
#define _FRR_WIN_SCHED_H

static inline int sched_yield(void) { SwitchToThread(); return 0; }

#endif
