// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of the lib/frr_pthread.h mutex helpers
 * Included at the end of lib/frr_pthread.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_FRR_PTHREAD_H
#define _FRR_WIN_MSVC_FRR_PTHREAD_H

#define _frr_with_mutex_msvc(_mtx, _once, mutex)                               \
	for (pthread_mutex_t *_mtx = _frr_mtx_lock(mutex); _mtx;               \
	     _frr_mtx_unlock(&_mtx))                                           \
		for (int _once = 0; !_once; _once = 1)                         \
	/* end */
#undef frr_with_mutex
#define frr_with_mutex(mutex)                                                  \
	_frr_with_mutex_msvc(NAMECTR(_mtx_), NAMECTR(_once_), mutex)

#undef frr_mutex_lock_autounlock
#define frr_mutex_lock_autounlock(mutex)                                       \
	_Static_assert(0, "frr_mutex_lock_autounlock() not available with MSVC")

#endif /* _FRR_WIN_MSVC_FRR_PTHREAD_H */
