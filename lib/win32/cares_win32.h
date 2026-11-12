// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Forced ahead of lib/resolver.c on Windows (cl /FI, after frr_win32.h).
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_CARES_H
#define _FRR_WIN_CARES_H

#include <ares.h>

extern int frr_win_ares_init_options(ares_channel_t **channelptr,
				     const struct ares_options *options, int optmask);
#define ares_init_options frr_win_ares_init_options

#endif /* _FRR_WIN_CARES_H */
