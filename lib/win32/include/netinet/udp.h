// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <netinet/udp.h>: the UDP header
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_NETINET_UDP_H
#define _FRR_WIN_NETINET_UDP_H

struct udphdr {
	uint16_t uh_sport;
	uint16_t uh_dport;
	uint16_t uh_ulen;
	uint16_t uh_sum;
};

#endif
