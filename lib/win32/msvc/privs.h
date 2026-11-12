// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variant of frr_with_privs()
 * Included at the end of lib/privs.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_PRIVS_H
#define _FRR_WIN_MSVC_PRIVS_H

#undef frr_with_privs
#define frr_with_privs(privs)                                                  \
	for (int _once = ((void)(privs), 0); !_once; _once = 1)

#endif /* _FRR_WIN_MSVC_PRIVS_H */
