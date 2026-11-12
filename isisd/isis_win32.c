// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * IS-IS link layer for Windows, on Npcap
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 *
 * Windows has no socket that gives Ethernet frames. Npcap
 * (https://npcap.com) provide them, through the pcap API, behind a handle that
 * poll() cannot wait for. Each circuit therefore gets a thread that owns
 * the pcap handle, and a pair of connected sockets between that thread and
 * the main pthread. circuit->fd is the main pthread's end of the pair: it
 * stands for the packet socket of the other platforms.
 *
 * - main pthread to thread: the frames to send, each one behind its length
 * - thread to main pthread: records made of a type, a length and a body,
 *   which is a received frame or a message for the log.
 *
 * The thread calls nothing of FRR. It ends when the main pthread has closed
 * circuit->fd, which is what takes a circuit down.
 */

#include <zebra.h>

#if ISIS_METHOD == ISIS_METHOD_NPCAP

#include <process.h>
#include <net/ethernet.h>

/* The IP Helper and pcap headers rely on SDK macros that
 * lib/win32/frr_win32.h removes because they collide with FRR identifiers.
 */
#define VOID void
#define CONST const
#define IN
#define OUT
#define OPTIONAL
#define far
#define near
#include <iphlpapi.h>
#include <netioapi.h>
#include <pcap.h>
#undef VOID
#undef CONST
#undef IN
#undef OUT
#undef OPTIONAL
#undef far
#undef near

#include "log.h"
#include "network.h"
#include "stream.h"
#include "if.h"
#include "lib_errors.h"

#include "isisd/isis_constants.h"
#include "isisd/isis_common.h"
#include "isisd/isis_circuit.h"
#include "isisd/isis_flags.h"
#include "isisd/isisd.h"
#include "isisd/isis_network.h"
#include "isisd/isis_pdu.h"

#define NPCAP_FRAME_MAX 16384
#define NPCAP_FRAME_MIN 60

/* main pthread to thread: a length; thread to main pthread: a type, a length */
#define NPCAP_TX_HDR 2
#define NPCAP_RX_HDR 3
#define NPCAP_REC_FRAME 'F'
#define NPCAP_REC_LOG 'L'

/* the thread sends at most one message for the log per interval, in ms */
#define NPCAP_LOG_INTERVAL 5000

/* frames taken from one side before the other one is looked at again */
#define NPCAP_BURST 64

struct isis_npcap {
	pcap_t *pcap;
	HANDLE pcap_event;
	SOCKET sock;
	WSAEVENT sock_event;
	uint8_t snpa[ETH_ALEN];
	ULONGLONG last_log;
	bool more; /* a burst ended with work left: do not wait */
	size_t txlen;
	uint8_t txbuf[NPCAP_TX_HDR + NPCAP_FRAME_MAX];
	uint8_t rxbuf[NPCAP_RX_HDR + NPCAP_FRAME_MAX];
};

/*
 * Table 9 - Architectural constants for use with ISO 8802 subnetworks
 * ISO 10589 - 8.4.8
 */
static const uint8_t ALL_L1_ISS[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x14};
static const uint8_t ALL_L2_ISS[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x15};
static const uint8_t ALL_ISS[6] = {0x09, 0x00, 0x2B, 0x00, 0x00, 0x05};

/* main pthread only */
static uint8_t sock_buff[NPCAP_RX_HDR + NPCAP_FRAME_MAX];

/* the frames with the LLC header of IS-IS: fe fe 03 */
static struct bpf_insn llc_filter[] = {
	BPF_STMT(BPF_LD + BPF_B + BPF_ABS, ETHER_HDR_LEN),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ISO_SAP, 0, 5),
	BPF_STMT(BPF_LD + BPF_B + BPF_ABS, ETHER_HDR_LEN + 1),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ISO_SAP, 0, 3),
	BPF_STMT(BPF_LD + BPF_B + BPF_ABS, ETHER_HDR_LEN + 2),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, 0x03, 0, 1),
	BPF_STMT(BPF_RET + BPF_K, (u_int)-1),
	BPF_STMT(BPF_RET + BPF_K, 0),
};

/* the thread of a circuit */

/*
 * Give a record to the main pthread: all of it, or none of it when the
 * main pthread is not reading, since part of a record would break the
 * stream. false when the socket is gone.
 *
 * send() and recv() are FRR's wrappers for descriptors in this file: the
 * parentheses get the Winsock functions.
 */
static bool npcap_to_main(struct isis_npcap *np, uint8_t type, size_t len)
{
	const char *buf = (const char *)np->rxbuf;
	int todo = (int)(NPCAP_RX_HDR + len);
	int done = 0;

	np->rxbuf[0] = type;
	np->rxbuf[1] = (uint8_t)(len >> 8);
	np->rxbuf[2] = (uint8_t)len;

	while (done < todo) {
		WSAPOLLFD wr = { .fd = np->sock, .events = POLLWRNORM };
		int n = (send)(np->sock, buf + done, todo - done, 0);

		if (n > 0) {
			done += n;
			continue;
		}
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			return false;
		if (!done)
			return true;

		/* the rest of it, once there is room again */
		if (WSAPoll(&wr, 1, -1) == SOCKET_ERROR)
			return false;
	}
	return true;
}

/* a message for the log of the daemon, which the main pthread writes */
static bool npcap_log(struct isis_npcap *np, const char *what, const char *err)
{
	ULONGLONG now = GetTickCount64();
	int len;

	if (np->last_log && now - np->last_log < NPCAP_LOG_INTERVAL)
		return true;
	np->last_log = now;

	len = snprintf((char *)np->rxbuf + NPCAP_RX_HDR, 256, "%s: %s", what, err);
	if (len < 0)
		return true;
	if (len > 255)
		len = 255;
	return npcap_to_main(np, NPCAP_REC_LOG, (size_t)len);
}

/* frames from Npcap to the main pthread; false when the socket is gone */
static bool npcap_capture(struct isis_npcap *np)
{
	struct pcap_pkthdr *hdr;
	const u_char *data;

	for (int i = 0; np->pcap_event; i++) {
		int rc;

		if (i == NPCAP_BURST) {
			np->more = true;
			break;
		}
		rc = pcap_next_ex(np->pcap, &hdr, &data);
		if (rc == 0)
			break;
		if (rc < 0) {
			/* the adapter is gone: nothing more to wait for */
			np->pcap_event = NULL;
			np->last_log = 0;
			return npcap_log(np, "capture stopped", pcap_geterr(np->pcap));
		}
		if (hdr->caplen != hdr->len || hdr->caplen > NPCAP_FRAME_MAX ||
		    hdr->caplen < ETHER_HDR_LEN + LLC_LEN)
			continue;
		/* Npcap also gives the frames this host sends */
		if (!memcmp(data + ETH_ALEN, np->snpa, ETH_ALEN))
			continue;

		memcpy(np->rxbuf + NPCAP_RX_HDR, data, hdr->caplen);
		if (!npcap_to_main(np, NPCAP_REC_FRAME, hdr->caplen))
			return false;
	}
	return true;
}

/* frames from the main pthread to Npcap */
static bool npcap_transmit(struct isis_npcap *np)
{
	for (int i = 0;; i++) {
		size_t len;
		int n;

		if (i == NPCAP_BURST) {
			np->more = true;
			return true;
		}
		n = (recv)(np->sock, (char *)np->txbuf + np->txlen,
			   (int)(sizeof(np->txbuf) - np->txlen), 0);
		if (n == 0)
			return false;
		if (n < 0)
			return WSAGetLastError() == WSAEWOULDBLOCK;
		np->txlen += (size_t)n;

		while (np->txlen >= NPCAP_TX_HDR) {
			len = ((size_t)np->txbuf[0] << 8) | np->txbuf[1];
			if (len > NPCAP_FRAME_MAX)
				return false;
			if (np->txlen < NPCAP_TX_HDR + len)
				break;

			if (pcap_sendpacket(np->pcap, np->txbuf + NPCAP_TX_HDR, (int)len) &&
			    !npcap_log(np, "could not transmit", pcap_geterr(np->pcap)))
				return false;

			np->txlen -= NPCAP_TX_HDR + len;
			memmove(np->txbuf, np->txbuf + NPCAP_TX_HDR + len, np->txlen);
		}
	}
}

static unsigned __stdcall npcap_thread(void *arg)
{
	struct isis_npcap *np = arg;

	for (;;) {
		HANDLE events[2] = { np->sock_event, np->pcap_event };
		WSANETWORKEVENTS fired;

		WaitForMultipleObjects(np->pcap_event ? 2 : 1, events, FALSE,
				       np->more ? 0 : INFINITE);
		/* this is what resets the event of the socket */
		WSAEnumNetworkEvents(np->sock, np->sock_event, &fired);

		np->more = false;
		if (!npcap_transmit(np) || !npcap_capture(np))
			break;
	}

	pcap_close(np->pcap);
	closesocket(np->sock);
	WSACloseEvent(np->sock_event);
	free(np);
	return 0;
}

/* main pthread */

/*
 * wpcap.dll is loaded on first use (/DELAYLOAD): the daemon starts without
 * Npcap. Npcap installs the DLL in a directory of its own, which is not
 * where Windows looks for DLLs, so load it from there beforehand.
 */
static bool npcap_load(void)
{
	static HMODULE wpcap;
	char path[MAX_PATH];
	static const char dll[] = "\\Npcap\\wpcap.dll";
	UINT len;

	if (wpcap)
		return true;

	len = GetSystemDirectoryA(path, sizeof(path));
	if (len && len + sizeof(dll) <= sizeof(path)) {
		memcpy(path + len, dll, sizeof(dll));
		wpcap = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	}
	/* its "WinPcap API-compatible mode" has it where Windows looks */
	if (!wpcap)
		wpcap = LoadLibraryA("wpcap.dll");
	return wpcap != NULL;
}

/* the name Npcap has for an interface: \Device\NPF_{its GUID} */
static bool npcap_device(const struct interface *ifp, char *dev, size_t size)
{
	NET_LUID luid;
	GUID g;

	if (ConvertInterfaceIndexToLuid(ifp->ifindex, &luid) != ERROR_SUCCESS ||
	    ConvertInterfaceLuidToGuid(&luid, &g) != ERROR_SUCCESS)
		return false;
	snprintf(dev, size,
		 "\\Device\\NPF_{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		 g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
		 g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
	return true;
}

static pcap_t *npcap_open(struct isis_circuit *circuit)
{
	struct bpf_program filter = {
		.bf_len = array_size(llc_filter),
		.bf_insns = llc_filter,
	};
	const char *ifname = circuit->interface->name;
	char errbuf[PCAP_ERRBUF_SIZE] = "";
	char dev[128];
	pcap_t *pcap;
	int rc;

	if (!npcap_load()) {
		flog_err(EC_LIB_SYSTEM_CALL,
			 "%s: Npcap is not installed: IS-IS sends and receives Ethernet frames through it (https://npcap.com)",
			 ifname);
		return NULL;
	}
	if (!npcap_device(circuit->interface, dev, sizeof(dev))) {
		zlog_warn("%s: no Npcap device name for interface %s", __func__, ifname);
		return NULL;
	}

	pcap = pcap_create(dev, errbuf);
	if (!pcap) {
		zlog_warn("%s: pcap_create() failed for %s: %s", __func__, ifname, errbuf);
		return NULL;
	}

	/* The frames of IS-IS go to multicast MAC addresses, which only come
	 * in promiscuous mode here. Immediate mode: no wait for a buffer to
	 * fill up.
	 */
	pcap_set_snaplen(pcap, NPCAP_FRAME_MAX);
	pcap_set_promisc(pcap, 1);
	pcap_set_immediate_mode(pcap, 1);
	rc = pcap_activate(pcap);
	if (rc < 0) {
		zlog_warn("%s: pcap_activate() failed for %s: %s (%s)", __func__, ifname,
			  pcap_statustostr(rc), pcap_geterr(pcap));
		pcap_close(pcap);
		return NULL;
	}
	if (rc > 0)
		zlog_warn("%s: %s: %s (%s)", __func__, ifname, pcap_statustostr(rc),
			  pcap_geterr(pcap));

	if (pcap_setfilter(pcap, &filter) < 0 || pcap_setnonblock(pcap, 1, errbuf) < 0) {
		zlog_warn("%s: could not set %s up: %s %s", __func__, ifname, pcap_geterr(pcap),
			  errbuf);
		pcap_close(pcap);
		return NULL;
	}
	return pcap;
}

/* the thread and the sockets of a circuit */
static int npcap_start(struct isis_circuit *circuit, pcap_t *pcap)
{
	struct isis_npcap *np;
	uintptr_t thread;
	int fds[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
		zlog_warn("%s: socketpair() failed: %s", __func__, safe_strerror(errno));
		return ISIS_WARNING;
	}

	/* plain calloc(): the thread, which calls nothing of FRR */
	np = calloc(1, sizeof(*np));
	if (!np) {
		close(fds[0]);
		close(fds[1]);
		return ISIS_WARNING;
	}
	np->pcap = pcap;
	np->pcap_event = pcap_getevent(pcap);
	np->sock = frr_win_fd_detach(fds[1]);
	np->sock_event = WSACreateEvent();
	memcpy(np->snpa, circuit->interface->hw_addr, ETH_ALEN);

	/* this also makes the socket of the thread non-blocking */
	if (np->sock_event == WSA_INVALID_EVENT ||
	    WSAEventSelect(np->sock, np->sock_event, FD_READ | FD_CLOSE) == SOCKET_ERROR)
		thread = 0;
	else
		thread = _beginthreadex(NULL, 0, npcap_thread, np, 0, NULL);
	if (!thread) {
		zlog_warn("%s: could not start the thread of %s", __func__,
			  circuit->interface->name);
		if (np->sock_event != WSA_INVALID_EVENT)
			WSACloseEvent(np->sock_event);
		closesocket(np->sock);
		free(np);
		close(fds[0]);
		return ISIS_WARNING;
	}
	CloseHandle((HANDLE)thread);

	circuit->fd = fds[0];
	return ISIS_OK;
}

int isis_sock_init(struct isis_circuit *circuit)
{
	pcap_t *pcap;

	/* Ethernet framing only */
	if (!if_is_broadcast(circuit->interface) ||
	    circuit->interface->hw_addr_len != ETH_ALEN) {
		zlog_warn("%s: unknown circuit type", __func__);
		return ISIS_WARNING;
	}

	pcap = npcap_open(circuit);
	if (!pcap) {
		zlog_warn("%s: could not initialize the socket", __func__);
		return ISIS_WARNING;
	}
	if (npcap_start(circuit, pcap) != ISIS_OK) {
		pcap_close(pcap);
		return ISIS_WARNING;
	}

	circuit->tx = isis_send_pdu_bcast;
	circuit->rx = isis_recv_pdu_bcast;
	return ISIS_OK;
}

/* all of len, which the thread sent in one go */
static bool npcap_read(int fd, uint8_t *buf, size_t len)
{
	while (len) {
		ssize_t n = read(fd, buf, len);

		if (n <= 0)
			return false;
		buf += n;
		len -= (size_t)n;
	}
	return true;
}

int isis_recv_pdu_bcast(struct isis_circuit *circuit, uint8_t *ssnpa)
{
	uint8_t *frame = sock_buff + NPCAP_RX_HDR;
	size_t len, pdu_len;
	uint16_t type;

	if (!npcap_read(circuit->fd, sock_buff, NPCAP_RX_HDR)) {
		zlog_warn("%s: read() failed on %s", __func__, circuit->interface->name);
		return ISIS_WARNING;
	}
	len = ((size_t)sock_buff[1] << 8) | sock_buff[2];
	if (len > NPCAP_FRAME_MAX || !npcap_read(circuit->fd, frame, len)) {
		zlog_warn("%s: read() failed on %s", __func__, circuit->interface->name);
		return ISIS_WARNING;
	}

	if (sock_buff[0] != NPCAP_REC_FRAME) {
		zlog_warn("IS-IS npcap: %s: %.*s", circuit->interface->name, (int)len,
			  (const char *)frame);
		return ISIS_WARNING;
	}

	if (len < ETHER_HDR_LEN + LLC_LEN || frame[ETHER_HDR_LEN] != ISO_SAP ||
	    frame[ETHER_HDR_LEN + 1] != ISO_SAP || frame[ETHER_HDR_LEN + 2] != 0x03)
		return ISIS_WARNING;

	/* an 802.3 length leaves the padding of a short frame out */
	pdu_len = len - ETHER_HDR_LEN;
	type = ((uint16_t)frame[2 * ETH_ALEN] << 8) | frame[2 * ETH_ALEN + 1];
	if (type <= MAX_LLC_LEN && type >= LLC_LEN && type < pdu_len)
		pdu_len = type;
	pdu_len -= LLC_LEN;

	if (pdu_len > STREAM_WRITEABLE(circuit->rcv_stream))
		return ISIS_WARNING;
	stream_write(circuit->rcv_stream, frame + ETHER_HDR_LEN + LLC_LEN, pdu_len);
	stream_set_getp(circuit->rcv_stream, 0);

	memcpy(ssnpa, frame + ETH_ALEN, ETH_ALEN);

	return ISIS_OK;
}

int isis_send_pdu_bcast(struct isis_circuit *circuit, int level)
{
	struct ether_header *eth = (struct ether_header *)(sock_buff + NPCAP_TX_HDR);
	uint8_t *llc = sock_buff + NPCAP_TX_HDR + ETHER_HDR_LEN;
	size_t pdu_len = stream_get_endp(circuit->snd_stream);
	size_t frame_len = ETHER_HDR_LEN + LLC_LEN + pdu_len;
	size_t done = 0;

	if (frame_len > NPCAP_FRAME_MAX) {
		zlog_warn("%s: output pdu size %zu is more than %d on circuit %s", __func__,
			  frame_len, NPCAP_FRAME_MAX, circuit->interface->name);
		return ISIS_WARNING;
	}

	stream_set_getp(circuit->snd_stream, 0);

	/* RFC5309 section 4.1 recommends ALL_ISS */
	if (circuit->circ_type == CIRCUIT_T_P2P)
		memcpy(eth->ether_dhost, ALL_ISS, ETH_ALEN);
	else if (level == 1)
		memcpy(eth->ether_dhost, ALL_L1_ISS, ETH_ALEN);
	else
		memcpy(eth->ether_dhost, ALL_L2_ISS, ETH_ALEN);
	/* not circuit->u.bc.snpa: the circuit can be a point-to-point one */
	memcpy(eth->ether_shost, circuit->interface->hw_addr, ETH_ALEN);
	eth->ether_type = htons(isis_ethertype(LLC_LEN + pdu_len));

	llc[0] = ISO_SAP;
	llc[1] = ISO_SAP;
	llc[2] = 0x03;
	memcpy(llc + LLC_LEN, circuit->snd_stream->data, pdu_len);

	/* the length above tells the padding of a short frame from the PDU */
	if (frame_len < NPCAP_FRAME_MIN) {
		memset(llc + LLC_LEN + pdu_len, 0, NPCAP_FRAME_MIN - frame_len);
		frame_len = NPCAP_FRAME_MIN;
	}
	sock_buff[0] = (uint8_t)(frame_len >> 8);
	sock_buff[1] = (uint8_t)frame_len;

	while (done < NPCAP_TX_HDR + frame_len) {
		ssize_t n = write(circuit->fd, sock_buff + done, NPCAP_TX_HDR + frame_len - done);

		if (n < 0) {
			zlog_warn("IS-IS npcap: could not transmit packet on %s: %s",
				  circuit->interface->name, safe_strerror(errno));
			/* part of a frame would break the stream */
			if (!done && ERRNO_IO_RETRY(errno))
				return ISIS_WARNING;
			return ISIS_ERROR;
		}
		done += (size_t)n;
	}

	return ISIS_OK;
}

#endif /* ISIS_METHOD == ISIS_METHOD_NPCAP */
