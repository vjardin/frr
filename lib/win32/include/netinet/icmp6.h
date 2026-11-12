// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <netinet/icmp6.h>: the RFC 3542 / RFC 4861 definitions
 * of the ICMPv6 header and of the neighbor discovery messages and options
 * that zebra's router advertisement code uses.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_NETINET_ICMP6_H
#define _FRR_WIN_NETINET_ICMP6_H

#include <netinet/in.h>

struct icmp6_hdr {
	uint8_t icmp6_type;
	uint8_t icmp6_code;
	uint16_t icmp6_cksum;
	union {
		uint32_t icmp6_un_data32[1];
		uint16_t icmp6_un_data16[2];
		uint8_t icmp6_un_data8[4];
	} icmp6_dataun;
};
#define icmp6_data32 icmp6_dataun.icmp6_un_data32
#define icmp6_data16 icmp6_dataun.icmp6_un_data16
#define icmp6_data8 icmp6_dataun.icmp6_un_data8

#define ND_ROUTER_SOLICIT 133
#define ND_ROUTER_ADVERT 134
#define ND_NEIGHBOR_SOLICIT 135
#define ND_NEIGHBOR_ADVERT 136
#define ND_REDIRECT 137

struct nd_router_solicit {
	struct icmp6_hdr nd_rs_hdr;
};
#define nd_rs_type nd_rs_hdr.icmp6_type
#define nd_rs_code nd_rs_hdr.icmp6_code
#define nd_rs_cksum nd_rs_hdr.icmp6_cksum
#define nd_rs_reserved nd_rs_hdr.icmp6_data32[0]

struct nd_router_advert {
	struct icmp6_hdr nd_ra_hdr;
	uint32_t nd_ra_reachable;
	uint32_t nd_ra_retransmit;
};
#define nd_ra_type nd_ra_hdr.icmp6_type
#define nd_ra_code nd_ra_hdr.icmp6_code
#define nd_ra_cksum nd_ra_hdr.icmp6_cksum
#define nd_ra_curhoplimit nd_ra_hdr.icmp6_data8[0]
#define nd_ra_flags_reserved nd_ra_hdr.icmp6_data8[1]
#define nd_ra_router_lifetime nd_ra_hdr.icmp6_data16[1]

#define ND_RA_FLAG_MANAGED 0x80
#define ND_RA_FLAG_OTHER 0x40
#define ND_RA_FLAG_HOME_AGENT 0x20

struct nd_opt_hdr {
	uint8_t nd_opt_type;
	uint8_t nd_opt_len;
};

#define ND_OPT_SOURCE_LINKADDR 1
#define ND_OPT_TARGET_LINKADDR 2
#define ND_OPT_PREFIX_INFORMATION 3
#define ND_OPT_REDIRECTED_HEADER 4
#define ND_OPT_MTU 5
#define ND_OPT_RTR_ADV_INTERVAL 7
#define ND_OPT_HOME_AGENT_INFO 8

struct nd_opt_prefix_info {
	uint8_t nd_opt_pi_type;
	uint8_t nd_opt_pi_len;
	uint8_t nd_opt_pi_prefix_len;
	uint8_t nd_opt_pi_flags_reserved;
	uint32_t nd_opt_pi_valid_time;
	uint32_t nd_opt_pi_preferred_time;
	uint32_t nd_opt_pi_reserved2;
	struct in6_addr nd_opt_pi_prefix;
};

#define ND_OPT_PI_FLAG_ONLINK 0x80
#define ND_OPT_PI_FLAG_AUTO 0x40
#define ND_OPT_PI_FLAG_RADDR 0x20

struct nd_opt_mtu {
	uint8_t nd_opt_mtu_type;
	uint8_t nd_opt_mtu_len;
	uint16_t nd_opt_mtu_reserved;
	uint32_t nd_opt_mtu_mtu;
};

/*
 * ICMPv6 type filter. Windows has no ICMP6_FILTER socket option: the
 * compatibility layer keeps the filter of a socket and applies it in
 * poll(), see lib/win32/compat.c.
 */
struct icmp6_filter {
	uint32_t icmp6_filt[8];
};
#define ICMP6_FILTER 1

#define ICMP6_FILTER_SETPASSALL(filterp) memset((filterp), 0, sizeof(struct icmp6_filter))
#define ICMP6_FILTER_SETBLOCKALL(filterp) memset((filterp), 0xff, sizeof(struct icmp6_filter))
#define ICMP6_FILTER_SETPASS(type, filterp)                                    \
	((((filterp)->icmp6_filt[(type) >> 5]) &= ~(1U << ((type) & 31))))
#define ICMP6_FILTER_SETBLOCK(type, filterp)                                   \
	((((filterp)->icmp6_filt[(type) >> 5]) |= (1U << ((type) & 31))))

#endif
