// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <netinet/ip.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_NETINET_IP_H
#define _FRR_WIN_NETINET_IP_H

#include <netinet/in.h>
#define IPVERSION 4
#define IP_MAXPACKET 65535
#define MAXTTL 255
#define IPDEFTTL 64
#define IPTOS_LOWDELAY 0x10
#define IPTOS_THROUGHPUT 0x08
#define IPTOS_RELIABILITY 0x04
#define IPTOS_PREC_INTERNETCONTROL 0xc0
#define IPTOS_PREC_NETCONTROL 0xe0
#define IP_RF 0x8000
#define IP_DF 0x4000
#define IP_MF 0x2000
#define IP_OFFMASK 0x1fff
struct ip {
	unsigned char ip_hl: 4, ip_v: 4;
	uint8_t ip_tos;
	uint16_t ip_len;
	uint16_t ip_id;
	uint16_t ip_off;
	uint8_t ip_ttl;
	uint8_t ip_p;
	uint16_t ip_sum;
	struct in_addr ip_src, ip_dst;
} __attribute__((packed));

#endif
