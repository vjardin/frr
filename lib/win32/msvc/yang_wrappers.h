// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Wrapper macros for the lib/yang_wrappers.h
 * Included at the end of lib/yang_wrappers.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_YANG_WRAPPERS_H
#define _FRR_WIN_MSVC_YANG_WRAPPERS_H

#define yang_str2prefix(a0, a1) yang_str2prefix(a0, PFX_PTR(a1))
#define yang_data_new_prefix(a0, a1) yang_data_new_prefix(a0, PFX_CPTR(a1))
#define yang_get_default_prefix(a0, a1, ...)                                   \
	yang_get_default_prefix(PFX_PTR(a0), a1, ##__VA_ARGS__)

#define yang_str2ipv4p(a0, a1) yang_str2ipv4p(a0, PFX_PTR(a1))
#define yang_data_new_ipv4p(a0, a1) yang_data_new_ipv4p(a0, PFX_CPTR(a1))
#define yang_dnode_get_ipv4p(a0, a1, a2, ...)                                  \
	yang_dnode_get_ipv4p(PFX_PTR(a0), a1, a2, ##__VA_ARGS__)
#define yang_get_default_ipv4p(a0, a1, ...)                                    \
	yang_get_default_ipv4p(PFX_PTR(a0), a1, ##__VA_ARGS__)

#define yang_str2ipv6p(a0, a1) yang_str2ipv6p(a0, PFX_PTR(a1))
#define yang_data_new_ipv6p(a0, a1) yang_data_new_ipv6p(a0, PFX_CPTR(a1))
#define yang_dnode_get_ipv6p(a0, a1, a2, ...)                                  \
	yang_dnode_get_ipv6p(PFX_PTR(a0), a1, a2, ##__VA_ARGS__)
#define yang_get_default_ipv6p(a0, a1, ...)                                    \
	yang_get_default_ipv6p(PFX_PTR(a0), a1, ##__VA_ARGS__)

#endif /* _FRR_WIN_MSVC_YANG_WRAPPERS_H */
