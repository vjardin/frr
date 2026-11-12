// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of the lib/frrevent.h scheduling macros
 * Included at the end of lib/frrevent.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_FRREVENT_H
#define _FRR_WIN_MSVC_FRREVENT_H

#undef _xref_t_a
#define _xref_t_a(addfn, type, m, f, a, v, t)                                  \
	do {                                                                   \
		static const struct xref_eventsched _xref = {                  \
			.xref = XREF_INIT(XREFT_EVENTSCHED, NULL, __func__),   \
			.funcname = #f,                                        \
			.dest = #t,                                            \
			.event_type = EVENT_##type,                            \
		};                                                             \
		XREF_LINK(_xref.xref);                                         \
		_event_add_##addfn(&_xref, m, f, a, v, t);                     \
	} while (0) /* end */

#undef event_execute
#define event_execute(m, f, a, v, p)                                           \
	do {                                                                   \
		static const struct xref_eventsched _xref = {                  \
			.xref = XREF_INIT(XREFT_EVENTSCHED, NULL, __func__),   \
			.funcname = #f,                                        \
			.dest = NULL,                                          \
			.event_type = EVENT_EXECUTE,                           \
		};                                                             \
		XREF_LINK(_xref.xref);                                         \
		_event_execute(&_xref, m, f, a, v, p);                         \
	} while (0) /* end */

#endif /* _FRR_WIN_MSVC_FRREVENT_H */
