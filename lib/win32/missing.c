// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Stand-ins for the libfrr files that are left out of the Windows build,
 * for the few functions the rest of libfrr calls:
 *
 *  - zlog_5424.c, zlog_5424_cli.c
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#include "zlog_5424.h"

/* only reached for a stdout that is a systemd journal stream */
void zlog_5424_init(struct zlog_cfg_5424 *zcf)
{
	memset(zcf, 0, sizeof(*zcf));
	zcf->fd = -1;
}

bool zlog_5424_apply_dst(struct zlog_cfg_5424 *zcf)
{
	return false;
}

bool zlog_5424_apply_meta(struct zlog_cfg_5424 *zcf)
{
	return false;
}

/* no "log extended" commands */
void log_5424_cmd_init(void)
{
}
