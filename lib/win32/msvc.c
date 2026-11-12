// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * helpers behind the MSVC variants of libfrr macros
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#include <zebra.h>

#include "checksum.h"
#include "darr.h"
#include "if.h"
#include "lib_errors.h"
#include "log.h"
#include "memory.h"
#include "mgmt_msg_native.h"
#include "mlag.h"
#include "prefix.h"
#include "vrf.h"
#include "xref.h"

/* MSVC ignores __attribute__((packed)): check that the packed structs */
static_assert(sizeof(struct ethaddr) == ETH_ALEN, "struct ethaddr has padding");
static_assert(sizeof(struct ipv4_ph) == 12, "struct ipv4_ph has padding");
static_assert(sizeof(struct ipv6_ph) == 40, "struct ipv6_ph has padding");
static_assert(offsetof(struct mlag_msg, data) == 8, "struct mlag_msg has padding");

#define PFX_SAME_OFFSET(type, field)                                           \
	static_assert(offsetof(struct type, field) == offsetof(struct prefix, u), \
		      "struct " #type " layout does not match struct prefix")
PFX_SAME_OFFSET(prefix_ipv4, prefix);
PFX_SAME_OFFSET(prefix_ipv6, prefix);
PFX_SAME_OFFSET(prefix_rd, val);
PFX_SAME_OFFSET(prefix_eth, eth_addr);
PFX_SAME_OFFSET(prefix_evpn, prefix);
PFX_SAME_OFFSET(prefix_fs, prefix);

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

/*
 * lib/xref.h: section start/end markers
 */
__declspec(allocate("xref_array$a")) const struct xref * const
	__start_xref_array[1] = { NULL };
__declspec(allocate("xref_array$z")) const struct xref * const
	__stop_xref_array[1] = { NULL };

/* lib/win32/msvc/xref.h: XREF_SETUP(), from constructors, so one thread */
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

/*
 * lib/module.c: "#pragma weak _frrmod_this_module = frrmod_default".  A
 * program without FRR_DAEMON_INFO() (vtysh) is part of the libfrr module.
 */
#pragma comment(linker, "/alternatename:_frrmod_this_module=frrmod_default")

/*
 * lib/darr.h
 */
void *_darr__ensure_avail(void **ap, ssize_t avail, size_t esize,
			  struct memtype *mt)
{
	void *a = *ap;
	ssize_t need = avail - (ssize_t)(darr_cap(a) - darr_len(a));

	if (need > 0)
		a = _darr__resize(a, darr_cap(a) + need, esize, mt);
	*ap = a;
	return a;
}

void *_darr__ensure_cap(void **ap, size_t cap, size_t esize, struct memtype *mt)
{
	void *a = *ap;

	if (!cap)
		cap = 1;
	if ((size_t)darr_cap(a) < cap)
		a = _darr__resize(a, (uint)cap, esize, mt);
	*ap = a;
	return a;
}

void *_darr__ensure_i(void **ap, int i, size_t esize, struct memtype *mt)
{
	void *a = *ap;

	assert(i >= 0);
	if (i > (int)darr_cap(a) - 1)
		a = _darr__resize(a, i + 1, esize, mt);
	assert(a != NULL);
	if ((uint)i + 1 > _darr_len(a)) {
		memset((char *)a + darr_len(a) * esize, 0,
		       (i + 1 - darr_len(a)) * esize);
		_darr_len(a) = i + 1;
	}
	*ap = a;
	return (char *)a + (size_t)i * esize;
}

void *_darr__append_n(void **ap, uint count, size_t esize, bool zero,
		      struct memtype *mt)
{
	uint len = darr_len(*ap);
	void *a = _darr__ensure_cap(ap, len + count, esize, mt);

	_darr_len(a) = len + count;
	if (zero)
		memset((char *)a + len * esize, 0, count * esize);
	return (char *)a + len * esize;
}

void *_darr__pop(void *a, size_t esize)
{
	uint len = _darr_len(a);

	assert(len);
	_darr_len(a) = len - 1;
	/* the element is still there, we don't shrink */
	return (char *)a + (len - 1) * esize;
}

uint _darr__strlen(const char *s)
{
	uint size = darr_len(s);

	if (size)
		size -= 1;
	assert(!s || s[size] == 0);
	return size;
}

char *_darr__in_strcat(char **dp, const char *s)
{
	char *d = *dp;
	uint dlen = darr_strlen(d);
	uint slen = (uint)strlen(s);

	_darr__ensure_cap((void **)&d, dlen + slen + 1, 1, MTYPE_DARR_STR);
	if (darr_len(d) == 0)
		*(char *)_darr__append_n((void **)&d, 1, 1, false, MTYPE_DARR) =
			0;
	memcpy(&d[darr_strlen(d)], s, slen + 1);
	_darr_len(d) += slen;
	*dp = d;
	return d;
}

char *_darr__in_strcat_tail(char **dp, const char *s)
{
	char *d = *dp;
	int dsize, ssize, extra;

	if (darr_len(d) == 0)
		*(char *)_darr__append_n((void **)&d, 1, 1, false, MTYPE_DARR) =
			0;
	dsize = (int)darr_ilen(d);
	ssize = (int)darr_ilen(s);
	extra = ssize - dsize;
	if (extra > 0) {
		_darr__ensure_cap((void **)&d, (uint)ssize, 1, MTYPE_DARR_STR);
		memcpy(darr_last(d), s + dsize - 1, extra + 1);
		_darr_len(d) += extra;
	}
	*dp = d;
	return d;
}

char *_darr__in_strdup_cap(char **dp, const char *s, size_t cap)
{
	char *d = *dp;
	size_t size = strlen(s) + 1;

	darr_reset(d);
	_darr__ensure_cap((void **)&d, cap > size ? cap : size, 1,
			  MTYPE_DARR_STR);
	strlcpy(d, s, darr_cap(d));
	darr_setlen(d, size);
	*dp = d;
	return d;
}

char *_darr__strdup_cap(const char *s, size_t cap)
{
	size_t size = strlen(s) + 1;
	char *d = NULL;

	_darr__ensure_cap((void **)&d,
			  ((ssize_t)cap > (ssize_t)size) ? cap : size, 1,
			  MTYPE_DARR_STR);
	strlcpy(d, s, darr_cap(d));
	darr_setlen(d, size);
	return d;
}

char *_darr__vsprintf(const char *fmt, va_list ap)
{
	char *d = NULL;

	_darr__in_vsprintf(&d, false, fmt, ap);
	return d;
}

char *_darr__sprintf(const char *fmt, ...)
{
	va_list ap;
	char *d;

	va_start(ap, fmt);
	d = _darr__vsprintf(fmt, ap);
	va_end(ap);
	return d;
}

/*
 * lib/mgmt_msg_native.h
 */
void *_mgmt_msg_native_alloc(size_t size, struct memtype *mt)
{
	uint8_t *buf = NULL;

	darr_append_nz_mt(buf, (uint)size, mt);
	return buf;
}

uint8_t *_mgmt_msg_native_append(uint8_t **darrp, const void *data,
				 size_t len)
{
	uint8_t *p = darr_append_n(*darrp, (uint)len);

	memcpy(p, data, len);
	return p;
}

const char *_mgmt_msg_native_xpath_data_decode(const char *data, size_t len,
					       size_t vsplit,
					       const char **datap)
{
	*datap = NULL;
	if (vsplit && vsplit <= len && data[vsplit - 1] == 0) {
		if (vsplit < len)
			*datap = data + vsplit;
		return data;
	}
	return NULL;
}

const char *_mgmt_msg_native_xpath_decode(const char *data, size_t len,
					  size_t vsplit)
{
	if (!vsplit || vsplit > len || data[vsplit - 1] != 0)
		return NULL;
	return data;
}

const char *_mgmt_msg_native_data_decode(const char *data, size_t len,
					 size_t vsplit)
{
	if (!vsplit || vsplit >= len || data[vsplit - 1] != 0)
		return NULL;
	return data + vsplit;
}

/*
 * lib/if.h
 */
struct interface *if_rb_name_insert(struct vrf *vrf, struct interface *ifp,
				    const char *caller)
{
	struct interface *iz = RB_INSERT(if_name_head, &vrf->ifaces_by_name, ifp);

	if (iz)
		flog_err(EC_LIB_INTERFACE,
			 "%s(%s): corruption detected -- interface with this name exists already in VRF %s!",
			 caller, ifp->name, ifp->vrf->name);
	return iz;
}

struct interface *if_rb_name_remove(struct vrf *vrf, struct interface *ifp,
				    const char *caller)
{
	struct interface *iz = RB_REMOVE(if_name_head, &vrf->ifaces_by_name, ifp);

	if (iz == NULL)
		flog_err(EC_LIB_INTERFACE,
			 "%s(%s): corruption detected -- interface with this name doesn't exist in VRF %s!",
			 caller, ifp->name, ifp->vrf->name);
	return iz;
}

struct interface *if_rb_index_insert(struct vrf *vrf, struct interface *ifp,
				     const char *caller)
{
	struct interface *iz = RB_INSERT(if_index_head, &vrf->ifaces_by_index, ifp);

	if (iz)
		flog_err(EC_LIB_INTERFACE,
			 "%s(%u): corruption detected -- interface with this ifindex exists already in VRF %s!",
			 caller, ifp->ifindex, ifp->vrf->name);
	return iz;
}

struct interface *if_rb_index_remove(struct vrf *vrf, struct interface *ifp,
				     const char *caller)
{
	struct interface *iz = RB_REMOVE(if_index_head, &vrf->ifaces_by_index, ifp);

	if (iz == NULL)
		flog_err(EC_LIB_INTERFACE,
			 "%s(%u): corruption detected -- interface with this ifindex doesn't exist in VRF %s!",
			 caller, ifp->ifindex, ifp->vrf->name);
	return iz;
}
