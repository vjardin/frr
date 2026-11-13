// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <arpa/telnet.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_ARPA_TELNET_H
#define _FRR_WIN_ARPA_TELNET_H
#define IAC 255
#define DONT 254
#define DO 253
#define WONT 252
#define WILL 251
#define SB 250
#define GA 249
#define EL 248
#define EC 247
#define AYT 246
#define AO 245
#define IP 244
#define BREAK 243
#define DM 242
#define NOP 241
#define SE 240
#define EOR 239
#define SYNCH 242
#define TELOPT_BINARY 0
#define TELOPT_ECHO 1
#define TELOPT_SGA 3
#define TELOPT_STATUS 5
#define TELOPT_TM 6
#define TELOPT_TTYPE 24
#define TELOPT_NAWS 31
#define TELOPT_TSPEED 32
#define TELOPT_LFLOW 33
#define TELOPT_LINEMODE 34
#define TELOPT_XDISPLOC 35
#define TELOPT_OLD_ENVIRON 36
#define TELOPT_NEW_ENVIRON 39
#define TELQUAL_IS 0
#define TELQUAL_SEND 1
#endif
