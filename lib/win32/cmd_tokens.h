// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Force-included (after frr_win32.h) when compiling the generated CLI lexer
 * and parser only: their WORD token would clash with the WORD typedef of the
 * Windows headers.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#define WORD CMD_TOKEN_WORD
