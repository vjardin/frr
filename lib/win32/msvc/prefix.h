// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC has no transparent unions
 * Included at the end of lib/prefix.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_PREFIX_H
#define _FRR_WIN_MSVC_PREFIX_H

static inline union prefixptr _pfx_ptr_p(void *p)
{
	union prefixptr u;

	u.p = p;
	return u;
}
static inline union prefixptr _pfx_ptr_u(union prefixptr u)
{
	return u;
}
static inline union prefixconstptr _pfx_cptr_p(const void *p)
{
	union prefixconstptr u;

	u.p = p;
	return u;
}
static inline union prefixconstptr _pfx_cptr_u(union prefixconstptr u)
{
	return u;
}
static inline union prefixconstptr _pfx_cptr_mu(union prefixptr u)
{
	union prefixconstptr r;

	r.p = u.p;
	return r;
}
#define PFX_PTR(x)                                                             \
	_Generic((x), union prefixptr: _pfx_ptr_u, default: _pfx_ptr_p)(x)
#define PFX_CPTR(x)                                                            \
	_Generic((x), union prefixconstptr: _pfx_cptr_u,                      \
		 union prefixptr: _pfx_cptr_mu, default: _pfx_cptr_p)(x)

#define prefix_afi(a0) prefix_afi(PFX_CPTR(a0))
#define prefix_family_str(a0) prefix_family_str(PFX_CPTR(a0))
#define prefix_blen(a0) prefix_blen(PFX_CPTR(a0))
#define prefix2str(a0, a1, a2) prefix2str(PFX_CPTR(a0), a1, a2)
#define prefix_match(a0, a1) prefix_match(PFX_CPTR(a0), PFX_CPTR(a1))
#define prefix_match_network_statement(a0, a1)                                 \
	prefix_match_network_statement(PFX_CPTR(a0), PFX_CPTR(a1))
#define prefix_same(a0, a1) prefix_same(PFX_CPTR(a0), PFX_CPTR(a1))
#define prefix_cmp(a0, a1) prefix_cmp(PFX_CPTR(a0), PFX_CPTR(a1))
#define prefix_common_bits(a0, a1)                                             \
	prefix_common_bits(PFX_CPTR(a0), PFX_CPTR(a1))
#define prefix_copy(a0, a1) prefix_copy(PFX_PTR(a0), PFX_CPTR(a1))
#define apply_mask(a0) apply_mask(PFX_PTR(a0))

#endif /* _FRR_WIN_MSVC_PREFIX_H */
