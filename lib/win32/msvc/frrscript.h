// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variant of frrscript_call() (lib/frrscript.h)
 * Included at the end of lib/frrscript.h, do not include directly.
 *
 * The call is a comma expression around helpers of lib/win32/msvc.c, the
 * state of the Lua function in a variable of each source file, of each
 * thread. The arguments are mapped with commas rather than the semicolons
 * of MAP_LISTS(), each one ending with its comma so that a call without
 * argument works too.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_FRRSCRIPT_H
#define _FRR_WIN_MSVC_FRRSCRIPT_H

extern struct lua_function_state *_frr_win_frrscript_lfs(struct frrscript *fs, const char *f);
extern int _frr_win_frrscript_failed(struct frrscript *fs, const char *f);
extern bool _frr_win_frrscript_field(struct lua_function_state *lfs, const char *name);

static __declspec(thread) struct lua_function_state *_frr_win_lfs;

#define _FRR_WIN_ENCODE(name, value) ENCODE_ARGS_WITH_STATE(_frr_win_lfs->L, (value)),
#define _FRR_WIN_DECODE(name, value)                                           \
	(_frr_win_frrscript_field(_frr_win_lfs, (name))                         \
		 ? (void)DECODE_ARGS_WITH_STATE(_frr_win_lfs->L, (value))        \
		 : (void)0),

#define _FRR_WIN_MAP(M, ...)                                                   \
	_CONCAT(_FRR_WIN_MAP_, PP_NARG(__VA_ARGS__))(M, ##__VA_ARGS__)
#define _FRR_WIN_MAP_0(M)
#define _FRR_WIN_MAP_1(M, _1) ESC(M _1)
#define _FRR_WIN_MAP_2(M, _1, _2) ESC(M _1 M _2)
#define _FRR_WIN_MAP_3(M, _1, _2, _3) ESC(M _1 M _2 M _3)
#define _FRR_WIN_MAP_4(M, _1, _2, _3, _4) ESC(M _1 M _2 M _3 M _4)
#define _FRR_WIN_MAP_5(M, _1, _2, _3, _4, _5) ESC(M _1 M _2 M _3 M _4 M _5)
#define _FRR_WIN_MAP_6(M, _1, _2, _3, _4, _5, _6)                              \
	ESC(M _1 M _2 M _3 M _4 M _5 M _6)
#define _FRR_WIN_MAP_7(M, _1, _2, _3, _4, _5, _6, _7)                          \
	ESC(M _1 M _2 M _3 M _4 M _5 M _6 M _7)
#define _FRR_WIN_MAP_8(M, _1, _2, _3, _4, _5, _6, _7, _8)                      \
	ESC(M _1 M _2 M _3 M _4 M _5 M _6 M _7 M _8)

#undef frrscript_call
#define frrscript_call(fs, f, ...)                                             \
	((_frr_win_lfs = _frr_win_frrscript_lfs((fs), (f))) == NULL             \
		 ? 1                                                           \
		 : (_FRR_WIN_MAP(_FRR_WIN_ENCODE, ##__VA_ARGS__)                 \
			    _frrscript_call_lua(_frr_win_lfs, PP_NARG(__VA_ARGS__))) \
			   ? _frr_win_frrscript_failed((fs), (f))              \
			   : (_FRR_WIN_MAP(_FRR_WIN_DECODE, ##__VA_ARGS__) 0))

#endif /* _FRR_WIN_MSVC_FRRSCRIPT_H */
