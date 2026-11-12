// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_COMPILER_H
#define _FRR_WIN_MSVC_COMPILER_H

/* libfrr objects are linked straight into each daemon: nothing to hide */
#define DSO_PUBLIC
#define DSO_SELF
#define DSO_LOCAL

/* constructors are function pointers in the CRT initializer table
 * (.CRT$XC*), which the CRT walks in section name order. Priorities map
 * to zero-padded section names so they sort like GCC priorities, every
 * priority in use needs a line here. Destructors are cleanup only and are
 * not run.
 */
#define _FRR_CTOR_SECT_450 ".CRT$XCT00450"
#define _FRR_CTOR_SECT_475 ".CRT$XCT00475"
#define _FRR_CTOR_SECT_500 ".CRT$XCT00500"
#define _FRR_CTOR_SECT_1000 ".CRT$XCT01000"
#define _FRR_CTOR_SECT_1001 ".CRT$XCT01001"
#define _FRR_CTOR_SECT_1100 ".CRT$XCT01100"
#define _FRR_CTOR_SECT_2000 ".CRT$XCT02000"
#define _FRR_CTOR_SECT_DEFAULT ".CRT$XCU"
#define _FRR_CTOR(sect, fn)                                                    \
	static void fn(void);                                                  \
	__pragma(section(sect, read))                                          \
	__declspec(allocate(sect)) static void (*const _frr_ctor_##fn)(void) = \
		fn
#undef FRR_CONSTRUCTOR
#define FRR_CONSTRUCTOR(prio, fn) _FRR_CTOR(_FRR_CTOR_SECT_##prio, fn)
#undef FRR_CONSTRUCTOR_DEFAULT
#define FRR_CONSTRUCTOR_DEFAULT(fn) _FRR_CTOR(_FRR_CTOR_SECT_DEFAULT, fn)
#undef FRR_DESTRUCTOR
#define FRR_DESTRUCTOR(prio, fn) static void fn(void)

/* MAX()/MIN()/numcmp(): each argument is evaluated once by a helper */
#define _FRR_MINMAX_T(X)                                                       \
	X(i, int) X(u, unsigned int) X(l, long) X(ul, unsigned long)           \
	X(ll, long long) X(ull, unsigned long long) X(f, float) X(d, double)
#define _FRR_MINMAX_FN(sfx, T)                                                 \
	static inline T _frr_max_##sfx(T a, T b) { return a > b ? a : b; }     \
	static inline T _frr_min_##sfx(T a, T b) { return a < b ? a : b; }     \
	static inline int _frr_cmp_##sfx(T a, T b)                             \
	{                                                                      \
		return (a < b) ? -1 : ((a > b) ? 1 : 0);                       \
	}
_FRR_MINMAX_T(_FRR_MINMAX_FN)
#define _FRR_MINMAX(op, a, b)                                                  \
	_Generic((a) + (b),                                                    \
		int: _frr_##op##_i, unsigned int: _frr_##op##_u,               \
		long: _frr_##op##_l, unsigned long: _frr_##op##_ul,            \
		long long: _frr_##op##_ll, unsigned long long: _frr_##op##_ull,\
		float: _frr_##op##_f, double: _frr_##op##_d)((a), (b))
#undef MAX
#define MAX(a, b) _FRR_MINMAX(max, a, b)
#undef MIN
#define MIN(a, b) _FRR_MINMAX(min, a, b)
#undef numcmp
#define numcmp(a, b) _FRR_MINMAX(cmp, a, b)

/* plain pointer arithmetic, const-ness is not tracked. container_of_null()
 * evaluates its argument only once (it is used on _pop() & co.)
 */
#undef container_of
#define container_of(ptr, type, member)                                        \
	((type *)((char *)(ptr) - offsetof(type, member)))
static inline void *_frr_container_of_null(const void *ptr, size_t offset)
{
	return ptr ? (void *)((const char *)ptr - offset) : NULL;
}
#undef container_of_null
#define container_of_null(ptr, type, member)                                   \
	((type *)_frr_container_of_null((ptr), offsetof(type, member)))

/* no transparent unions: functions taking one get a wrapper macro that
 * builds the union at the call site, see lib/win32/msvc/prefix.h
 */
#define FRR_NO_TRANSPARENT_UNION 1

#undef FRR_EMPTY_STRUCT_MEMBER
#define FRR_EMPTY_STRUCT_MEMBER char _empty;

/* No VLAs; the array lives until the function returns. An array of no
 * element still gets one: the stack frame of a GCC VLA of 0 elements has
 * room for a write past its end, that of _alloca(0) has none.
 */
#undef FRR_VLA
#define FRR_VLA(type, name, count)                                             \
	type *name = _alloca(sizeof(type) * ((count) > 0 ? (count) : 1))

#undef FRR_NORETURN
#define FRR_NORETURN __declspec(noreturn)

#endif /* _FRR_WIN_MSVC_COMPILER_H */
