// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of lib/typesafe.h macros
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_TYPESAFE_H
#define _FRR_WIN_MSVC_TYPESAFE_H

#undef _HASH_SIZE
#define _HASH_SIZE(tabshift) ((1U << (tabshift)) >> 1)
#undef _HASH_KEY
#define _HASH_KEY(tabshift, val) ((val) >> (33 - (tabshift)))

#endif /* _FRR_WIN_MSVC_TYPESAFE_H */
