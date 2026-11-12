// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of the lib/mgmt_msg_native.h macros
 * Included at the end of lib/mgmt_msg_native.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_MGMT_MSG_NATIVE_H
#define _FRR_WIN_MSVC_MGMT_MSG_NATIVE_H

extern void *_mgmt_msg_native_alloc(size_t size, struct memtype *mt);
extern uint8_t *_mgmt_msg_native_append(uint8_t **darrp, const void *data,
					size_t len);
extern const char *_mgmt_msg_native_xpath_data_decode(const char *data,
						      size_t len, size_t vsplit,
						      const char **datap);
extern const char *_mgmt_msg_native_xpath_decode(const char *data, size_t len,
						 size_t vsplit);
extern const char *_mgmt_msg_native_data_decode(const char *data, size_t len,
						size_t vsplit);

#undef mgmt_msg_native_alloc_msg
#define mgmt_msg_native_alloc_msg(msg_type, var_len, mem_type)                 \
	((msg_type *)_mgmt_msg_native_alloc(sizeof(msg_type) + (var_len),      \
					    (mem_type)))

#undef mgmt_msg_native_append
#define mgmt_msg_native_append(msg, data, len)                                 \
	_mgmt_msg_native_append(mgmt_msg_native_get_darrp(msg), (data), (len))

#undef mgmt_msg_native_xpath_data_decode
#define mgmt_msg_native_xpath_data_decode(msg, msglen, _data)                  \
	_mgmt_msg_native_xpath_data_decode((const char *)(msg)->data,          \
					   (msglen) - sizeof(*(msg)),          \
					   (msg)->vsplit,                      \
					   (const char **)&(_data))

#undef mgmt_msg_native_xpath_decode
#define mgmt_msg_native_xpath_decode(msg, msglen)                              \
	_mgmt_msg_native_xpath_decode((const char *)(msg)->data,               \
				      (msglen) - sizeof(*(msg)), (msg)->vsplit)

#undef mgmt_msg_native_data_decode
#define mgmt_msg_native_data_decode(msg, msglen)                               \
	_mgmt_msg_native_data_decode((const char *)(msg)->data,                \
				     (msglen) - sizeof(*(msg)), (msg)->vsplit)

#endif /* _FRR_WIN_MSVC_MGMT_MSG_NATIVE_H */
