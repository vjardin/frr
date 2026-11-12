// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of the lib/darr.h macros that yield a value
 * Included at the end of lib/darr.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_DARR_H
#define _FRR_WIN_MSVC_DARR_H

void *_darr__ensure_avail(void **ap, ssize_t avail, size_t esize,
			  struct memtype *mt);
void *_darr__ensure_cap(void **ap, size_t cap, size_t esize, struct memtype *mt);
void *_darr__ensure_i(void **ap, int i, size_t esize, struct memtype *mt);
void *_darr__append_n(void **ap, uint count, size_t esize, bool zero,
		      struct memtype *mt);
void *_darr__pop(void *a, size_t esize);
char *_darr__in_strcat(char **dp, const char *s);
char *_darr__in_strcat_tail(char **dp, const char *s);
char *_darr__in_strdup_cap(char **dp, const char *s, size_t cap);
char *_darr__strdup_cap(const char *s, size_t cap);
char *_darr__sprintf(const char *fmt, ...) PRINTFRR(1, 2);
char *_darr__vsprintf(const char *fmt, va_list ap) PRINTFRR(1, 0);
uint _darr__strlen(const char *s);

#undef _darr_resize_mt
#define _darr_resize_mt(A, C, MT) ((A) = _darr__resize(A, C, _darr_esize(A), MT))
#undef darr_cap
#define darr_cap(A) (((A) == NULL) ? 0 : _darr_meta(A)->cap)
#undef darr_set_strlen
#define darr_set_strlen(S, L) (_darr_len(S) = (L) + 1, *darr_last(S) = 0, darr_last(S))
#undef darr_ensure_avail_mt
#define darr_ensure_avail_mt(A, S, MT)                                         \
	((__typeof__(A))_darr__ensure_avail((void **)&(A), (S),                \
					    _darr_esize(A), MT))
#undef darr_ensure_cap_mt
#define darr_ensure_cap_mt(A, C, MT)                                           \
	((__typeof__(A))_darr__ensure_cap((void **)&(A), (C), _darr_esize(A),  \
					  MT))
#undef darr_ensure_i_mt
#define darr_ensure_i_mt(A, I, MT)                                             \
	((__typeof__(A))_darr__ensure_i((void **)&(A), (int)(I),               \
					_darr_esize(A), MT))
#undef _darr_insert_n
#define _darr_insert_n(A, I, N, Z, MT)                                         \
	((A) = _darr__insert_n(A, I, N, _darr_esize(A), Z, MT), &(A)[I])
#undef _darr_append_n
#define _darr_append_n(A, N, Z, MT)                                            \
	((__typeof__(A))_darr__append_n((void **)&(A), (N), _darr_esize(A), Z, \
					MT))
#undef darr_pop
#define darr_pop(A) (*(__typeof__(A))_darr__pop((A), _darr_esize(A)))
#undef darr_last
#define darr_last(A) ((darr_len(A) > 0) ? &(A)[darr_len(A) - 1] : NULL)
#undef darr_in_strcat
#define darr_in_strcat(D, S) _darr__in_strcat(&(D), (S))
#undef darr_in_strcat_tail
#define darr_in_strcat_tail(D, S) _darr__in_strcat_tail(&(D), (S))
#undef darr_in_strdup_cap
#define darr_in_strdup_cap(D, S, C) _darr__in_strdup_cap(&(D), (S), (C))
#undef darr_sprintf
#define darr_sprintf(F, ...) _darr__sprintf(F, __VA_ARGS__)
#undef darr_strdup_cap
#define darr_strdup_cap(S, C) _darr__strdup_cap((S), (C))
#undef darr_strlen
#define darr_strlen(S) _darr__strlen(S)
#undef darr_strlen_fixup
#define darr_strlen_fixup(S)                                                   \
	(_darr_len(S) = (uint32_t)strlen(S) + 1, darr_strlen(S))
#undef darr_vsprintf
#define darr_vsprintf(F, A) _darr__vsprintf(F, A)

#endif /* _FRR_WIN_MSVC_DARR_H */
