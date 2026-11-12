// SPDX-License-Identifier: GPL-2.0-or-later
/* Windows shim for <termios.h>: consoles are not ttys, every call fails
 * with ENOTTY (vty's stdio terminal mode only restores settings it got)
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_TERMIOS_H
#define _FRR_WIN_TERMIOS_H

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

#define NCCS 32
struct termios {
	tcflag_t c_iflag;
	tcflag_t c_oflag;
	tcflag_t c_cflag;
	tcflag_t c_lflag;
	cc_t c_cc[NCCS];
};

#define VMIN 6
#define VTIME 5
#define ICANON 0000002
#define ECHO 0000010
#define ECHONL 0000100
#define ISIG 0000001
#define IEXTEN 0100000
#define ICRNL 0000400
#define IXON 0002000
#define OPOST 0000001
#define IGNBRK 0000001
#define BRKINT 0000002
#define IGNPAR 0000004
#define PARMRK 0000010
#define INPCK 0000020
#define ISTRIP 0000040
#define INLCR 0000100
#define IGNCR 0000200
#define CSIZE 0000060
#define CS8 0000060
#define PARENB 0000400
#define TCSANOW 0
#define TCSADRAIN 1
#define TCSAFLUSH 2

static __forceinline int tcgetattr(int fd, struct termios *t)
{
	(void)fd;
	(void)t;
	errno = ENOTTY;
	return -1;
}
static __forceinline int tcsetattr(int fd, int act, const struct termios *t)
{
	(void)fd;
	(void)act;
	(void)t;
	errno = ENOTTY;
	return -1;
}

#endif
