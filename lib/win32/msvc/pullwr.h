// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variant of the lib/pullwr.h
 * Included at the end of lib/pullwr.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_PULLWR_H
#define _FRR_WIN_MSVC_PULLWR_H

#undef pullwr_new
#define pullwr_new(tm, fd, arg, fill, err)                                     \
	_pullwr_new(tm, fd, arg, (void (*)(void *, struct pullwr *))(fill),    \
		    (void (*)(void *, struct pullwr *, bool))(err))

#endif /* _FRR_WIN_MSVC_PULLWR_H */
