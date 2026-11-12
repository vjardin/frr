// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows kernel interface for zebra, on top of the IP Helper API
 * (iphlpapi): declarations shared by the *_win32.c files.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _ZEBRA_KERNEL_WIN32_H
#define _ZEBRA_KERNEL_WIN32_H

#ifdef _WIN32

#define VOID void
#define CONST const
#define IN
#define OUT
#define OPTIONAL
#define far
#define near
#include <iphlpapi.h>
#include <netioapi.h>
#undef VOID
#undef CONST
#undef IN
#undef OUT
#undef OPTIONAL
#undef far
#undef near

#ifdef __cplusplus
extern "C" {
#endif

extern void win32_interface_sync(void);
extern void win32_route_sync(bool startup);

extern void win32_route_fini(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* _ZEBRA_KERNEL_WIN32_H */
