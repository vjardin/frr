// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Wrapper macro for the lib/filter.h function that takes prefix unions, see
 * lib/win32/msvc/prefix.h.
 * Included at the end of lib/filter.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */
#ifndef _FRR_WIN_MSVC_FILTER_H
#define _FRR_WIN_MSVC_FILTER_H

#define access_list_apply_sadr(a0, a1, a2, a3)                                 \
	access_list_apply_sadr(a0, PFX_CPTR(a1), PFX_CPTR(a2), a3)

#endif /* _FRR_WIN_MSVC_FILTER_H */
