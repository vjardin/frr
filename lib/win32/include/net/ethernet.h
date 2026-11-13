// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Windows shim for <net/ethernet.h>, most of it lives in lib/win32/frr_win32.h
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_NET_ETHERNET_H
#define _FRR_WIN_NET_ETHERNET_H
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
#define ETHER_ADDR_LEN ETH_ALEN
#define ETH_HLEN 14
#define ETHER_HDR_LEN ETH_HLEN
#define ETHER_TYPE_LEN 2
#define ETH_DATA_LEN 1500
#define ETH_FRAME_LEN 1514
#define ETH_P_IP 0x0800
#define ETH_P_ARP 0x0806
#define ETH_P_IPV6 0x86DD
#define ETH_P_8021Q 0x8100
#define ETH_P_ALL 0x0003
#define ETHERTYPE_IP ETH_P_IP
#define ETHERTYPE_ARP ETH_P_ARP
#define ETHERTYPE_IPV6 ETH_P_IPV6
#define ETHERTYPE_VLAN ETH_P_8021Q
struct ether_addr { uint8_t ether_addr_octet[ETH_ALEN]; } __attribute__((packed));
struct ether_header { uint8_t ether_dhost[ETH_ALEN]; uint8_t ether_shost[ETH_ALEN]; uint16_t ether_type; } __attribute__((packed));
#endif
