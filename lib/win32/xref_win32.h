// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * xref_array on Windows (COFF)
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_XREF_WIN32_H
#define _FRR_XREF_WIN32_H

/* no __start_/__stop_ on COFF: the linker sorts xref_array$a, $m and $z */
#undef xref_array_attr
#define xref_array_attr used, section("xref_array$m")

#define XREF_WIN32_FILENAME_MAX 256
extern const char *frr_win_xref_filename(const char *file, char *buf, size_t len);

extern void frr_win_xref_setup(void);

#undef XREF_SETUP
#define XREF_SETUP()                                                           \
	static const struct xref _dummy_xref = {                               \
			/* .xrefdata = */ NULL,                                \
			/* .type = */ XREFT_NONE,                              \
			/* .line = */ __LINE__,                                \
			/* .file = */ __FILE__,                                \
			/* .func = */ "dummy",                                 \
	};                                                                     \
	static const struct xref * const _dummy_xref_p                         \
			__attribute__((xref_array_attr)) = &_dummy_xref;       \
	static void __attribute__((used, _CONSTRUCTOR(1100)))                  \
			_xref_init(void) {                                     \
		frr_win_xref_setup();                                          \
	}                                                                      \
	MACRO_REQUIRE_SEMICOLON() /* end */

#endif /* _FRR_XREF_WIN32_H */
