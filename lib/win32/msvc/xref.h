// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of the lib/xref.h macros
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_XREF_H
#define _FRR_WIN_MSVC_XREF_H

#define XREF_WIN32_FILENAME_MAX 256
extern const char *frr_win_xref_filename(const char *file, char *buf, size_t len);

extern void frr_win_xref_setup(void);

#pragma section("xref_array$a", read)
#pragma section("xref_array$m", read)
#pragma section("xref_array$z", read)

#undef XREF_SETUP
#define XREF_SETUP()                                                           \
	static const struct xref _dummy_xref = {                               \
			/* .xrefdata = */ NULL,                                \
			/* .type = */ XREFT_NONE,                              \
			/* .line = */ __LINE__,                                \
			/* .file = */ __FILE__,                                \
			/* .func = */ "dummy",                                 \
	};                                                                     \
	__declspec(allocate("xref_array$m")) static const struct xref * const  \
			_dummy_xref_p = &_dummy_xref;                          \
	FRR_CONSTRUCTOR(1100, _xref_init);                                     \
	static void _xref_init(void) {                                         \
		frr_win_xref_setup();                                          \
	}                                                                      \
	MACRO_REQUIRE_SEMICOLON() /* end */

#undef XREF_LINK
#define XREF_LINK(dst)                                                         \
	__declspec(allocate("xref_array$m")) static const struct xref * const  \
			NAMECTR(xref_p_) = &(dst)                              \
	/* end */

#endif /* _FRR_WIN_MSVC_XREF_H */
