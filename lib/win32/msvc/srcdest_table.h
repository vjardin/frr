// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Wrapper macros for the lib/srcdest_table.h
 * Included at the end of lib/srcdest_table.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_SRCDEST_TABLE_H
#define _FRR_WIN_MSVC_SRCDEST_TABLE_H

#define srcdest_rnode_get(a0, a1, a2) srcdest_rnode_get(a0, PFX_CPTR(a1), a2)
#define srcdest_rnode_lookup(a0, a1, a2)                                       \
	srcdest_rnode_lookup(a0, PFX_CPTR(a1), a2)
#define srcdest_table_get_next(a0, a1, a2)                                     \
	srcdest_table_get_next(a0, PFX_CPTR(a1), a2)

#endif /* _FRR_WIN_MSVC_SRCDEST_TABLE_H */
