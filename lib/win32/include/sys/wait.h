// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <sys/wait.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_SYS_WAIT_H
#define _FRR_WIN_SYS_WAIT_H

#define WNOHANG 1
#define WUNTRACED 2
#define WIFEXITED(s) (((s) & 0x7f) == 0)
#define WEXITSTATUS(s) (((s) >> 8) & 0xff)
#define WIFSIGNALED(s) (((s) & 0x7f) != 0 && ((s) & 0x7f) != 0x7f)
#define WTERMSIG(s) ((s) & 0x7f)
#define WIFSTOPPED(s) (((s) & 0xff) == 0x7f)
#define WSTOPSIG(s) WEXITSTATUS(s)
#define WCOREDUMP(s) 0

extern pid_t waitpid(pid_t pid, int *status, int options);
static inline pid_t wait(int *status)
{
	return waitpid(-1, status, 0);
}
static inline pid_t wait4(pid_t pid, int *status, int options, void *usage)
{
	(void)usage;
	return waitpid(pid, status, options);
}

#endif
