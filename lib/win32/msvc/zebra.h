// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of lib/zebra.h macros
 * Included at the end of lib/zebra.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */
#ifndef _FRR_WIN_MSVC_ZEBRA_H
#define _FRR_WIN_MSVC_ZEBRA_H

static inline uint32_t _frr_atomic_load_u32(_Atomic uint32_t *pv)
{
	return atomic_load_explicit(pv, memory_order_seq_cst);
}
#undef CHECK_FLAG_ATOMIC
#define CHECK_FLAG_ATOMIC(PV, F) (_frr_atomic_load_u32(PV) & (F))

#endif /* _FRR_WIN_MSVC_ZEBRA_H */
