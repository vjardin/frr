// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * MSVC variant of the lib/json.h macro
 * Included at the end of lib/json.h, do not include directly.
 *
 * Copyright (C) 2026 Vincent Jardin <vjardin@free.fr>
 */

#ifndef _FRR_WIN_MSVC_JSON_H
#define _FRR_WIN_MSVC_JSON_H

static inline struct json_object *
_json_object_new_array_u32(struct json_object *(*json_func)(int32_t), const uint32_t *fields,
			   int n)
{
	struct json_object *json_array = json_object_new_array();

	for (int i = 0; i < n; i++)
		json_object_array_add(json_array, json_func(fields[i]));
	return json_array;
}

#undef JSON_OBJECT_NEW_ARRAY
#define JSON_OBJECT_NEW_ARRAY(json_func, fields, n)                            \
	_Generic(&(fields)[0],                                                 \
		uint32_t *: _json_object_new_array_u32,                        \
		const uint32_t *: _json_object_new_array_u32)((json_func), (fields), (n))

#endif /* _FRR_WIN_MSVC_JSON_H */
