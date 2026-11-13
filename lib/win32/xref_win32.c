// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * xref_array on Windows (COFF)
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#include "xref.h"

/*
 * lib/xref.c: the last directory and the file name of a path that has '\\'
 * or '/', as "dir/file.c".
 */
const char *frr_win_xref_filename(const char *file, char *buf, size_t len)
{
	const char *last = NULL, *prev = NULL, *p;
	size_t i;

	for (p = file; *p; p++) {
		if (*p == '/' || *p == '\\') {
			prev = last;
			last = p;
		}
	}
	if (prev)
		file = prev + 1;

	for (i = 0; file[i] && i + 1 < len; i++)
		buf[i] = file[i] == '\\' ? '/' : file[i];
	buf[i] = '\0';
	return buf;
}

/* lib/xref.h: section start/end markers */
const struct xref * const __start_xref_array[1]
	__attribute__((used, section("xref_array$a"))) = { NULL };
const struct xref * const __stop_xref_array[1]
	__attribute__((used, section("xref_array$z"))) = { NULL };

/* XREF_SETUP(), from constructors, so one thread */
void frr_win_xref_setup(void)
{
	static struct xref_block block = {
		.start = __start_xref_array,
		.stop = __stop_xref_array,
	};
	static bool added;

	if (added)
		return;
	added = true;
	xref_block_add(&block);
}
