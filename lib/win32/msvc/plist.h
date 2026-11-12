// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Wrapper macro for the lib/plist.h function
 * Included at the end of lib/plist.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_PLIST_H
#define _FRR_WIN_MSVC_PLIST_H

#define prefix_list_apply_ext(a0, a1, a2, a3)                                  \
	prefix_list_apply_ext(a0, a1, PFX_CPTR(a2), a3)

#endif /* _FRR_WIN_MSVC_PLIST_H */
