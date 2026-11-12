// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Wrapper macros for the lib/table.h functions
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */
#ifndef _FRR_WIN_MSVC_TABLE_H
#define _FRR_WIN_MSVC_TABLE_H

#define route_node_get(a0, a1) route_node_get(a0, PFX_CPTR(a1))
#define route_node_lookup(a0, a1) route_node_lookup(a0, PFX_CPTR(a1))
#define route_node_lookup_maynull(a0, a1)                                      \
	route_node_lookup_maynull(a0, PFX_CPTR(a1))
#define route_node_match(a0, a1) route_node_match(a0, PFX_CPTR(a1))
#define route_table_get_next(a0, a1) route_table_get_next(a0, PFX_CPTR(a1))

#endif /* _FRR_WIN_MSVC_TABLE_H */
