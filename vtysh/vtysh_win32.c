// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * vtysh on Windows
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#ifdef _WIN32

#include <readline/readline.h>

/* ^Z leaves the configuration: on POSIX systems the terminal turns it into
 * SIGTSTP, which vtysh handles. The Windows console has it as a key, so it
 * is bound to raising the signal for that same handler.
 */
static int vtysh_win32_ctrl_z(int count, int key)
{
	kill(getpid(), SIGTSTP);
	return 0;
}

static void __attribute__((constructor)) vtysh_win32_init(void)
{
	rl_bind_key('Z' - '@', vtysh_win32_ctrl_z);

	/* readline would put its own handlers in front of the C runtime's
	 * default ones, which end the process: ^C is for vtysh's SIGINT
	 * handler alone, which starts a new line
	 */
	rl_catch_signals = 0;
}

#endif /* _WIN32 */
