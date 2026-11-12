// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variant of FRR's assert()
 * instead of lib/assert, so <assert.h> lands here.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_ASSERT_H
#define _FRR_WIN_ASSERT_H

#include "lib/assert/assert.h"

extern __declspec(noreturn) void
_zlog_assert_failed(const struct xref_assert *xref, const char *extra, ...);

#undef assert
#define assert(expr_)                                                          \
	((expr_) ? (void)0                                                     \
		 : _zlog_assert_failed(&(const struct xref_assert){            \
			.xref = XREF_INIT(XREFT_ASSERT, NULL, __func__),       \
			.expr = #expr_,                                        \
		   }, NULL))

#undef assertf
#define assertf(expr_, extra_, ...)                                            \
	((expr_) ? (void)0                                                     \
		 : _zlog_assert_failed(&(const struct xref_assert){            \
			.xref = XREF_INIT(XREFT_ASSERT, NULL, __func__),       \
			.expr = #expr_,                                        \
			.extra = extra_,                                       \
			.args = #__VA_ARGS__,                                  \
		   }, extra_, ##__VA_ARGS__))

#endif /* _FRR_WIN_ASSERT_H */
