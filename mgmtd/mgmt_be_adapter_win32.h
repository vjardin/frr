// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variants of the mgmtd/mgmt_be_adapter.h
 * Included at the end of mgmtd/mgmt_be_adapter.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_MGMTD_BE_ADAPTER_WIN32_H_
#define _FRR_MGMTD_BE_ADAPTER_WIN32_H_

static inline mgmt_be_client_id_t _mgmt_be_next_set(mgmt_be_client_id_t id, uint64_t bits)
{
	for (; id < darr_len(mgmt_be_adapters_by_id); id++)
		if (IS_IDBIT_SET(bits, id))
			break;
	return id;
}
#undef _GET_NEXT_SET
#define _GET_NEXT_SET(id, bits) _mgmt_be_next_set((id), (bits))

static inline mgmt_be_client_id_t
_mgmt_be_next_set_adapter(mgmt_be_client_id_t id, struct mgmt_be_client_adapter **adapter,
			  uint64_t bits)
{
	for (; id < darr_len(mgmt_be_adapters_by_id); id++) {
		if (IS_IDBIT_SET(bits, id)) {
			*adapter = mgmt_be_get_adapter_by_id(id);
			if (*adapter)
				break;
		}
	}
	return id;
}
#undef _GET_NEXT_SET_ADAPTER
#define _GET_NEXT_SET_ADAPTER(id, adapter, bits)                                                  \
	_mgmt_be_next_set_adapter((id), &(adapter), (bits))

#endif /* _FRR_MGMTD_BE_ADAPTER_WIN32_H_ */
