// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Logging - Filtered file log target
 * Copyright (C) 2019 Cumulus Networks, Inc.
 *                    Stephen Worley
 */

#include <zebra.h>

#include "frr_pthread.h"
#include "log.h"

static pthread_mutex_t logfilterlock = PTHREAD_MUTEX_INITIALIZER;
static char zlog_filters[ZLOG_FILTERS_MAX][ZLOG_FILTER_LENGTH_MAX + 1];
static uint8_t zlog_filter_count;

/*
 * look for a match on the filter in the current filters,
 * logfilterlock must be held
 */
static int zlog_filter_lookup(const char *lookup)
{
	for (int i = 0; i < zlog_filter_count; i++) {
		if (strncmp(lookup, zlog_filters[i], sizeof(zlog_filters[0]))
		    == 0)
			return i;
	}
	return -1;
}

void zlog_filter_clear(void)
{
	frr_with_mutex (&logfilterlock) {
		zlog_filter_count = 0;
	}
}

int zlog_filter_add(const char *filter)
{
	int ret = 0;

	frr_with_mutex (&logfilterlock) {
		if (zlog_filter_count >= ZLOG_FILTERS_MAX) {
			ret = 1;
			break;
		}

		if (zlog_filter_lookup(filter) != -1) {
			/* Filter already present */
			ret = -1;
			break;
		}

		strlcpy(zlog_filters[zlog_filter_count], filter,
			sizeof(zlog_filters[0]));

		if (zlog_filters[zlog_filter_count][0] == '\0') {
			/* Filter was either empty or didn't get copied
			 * correctly
			 */
			ret = -1;
			break;
		}

		zlog_filter_count++;
	}
	return ret;
}

int zlog_filter_del(const char *filter)
{
	int ret = 0;

	frr_with_mutex (&logfilterlock) {
		int found_idx = zlog_filter_lookup(filter);
		int last_idx = zlog_filter_count - 1;

		if (found_idx == -1) {
			/* Didn't find the filter to delete */
			ret = -1;
			break;
		}

		/* Adjust the filter array */
		memmove(zlog_filters[found_idx], zlog_filters[found_idx + 1],
			(last_idx - found_idx) * sizeof(zlog_filters[0]));

		zlog_filter_count--;
	}
	return ret;
}

/* Dump all filters to buffer, delimited by new line */
int zlog_filter_dump(char *buf, size_t max_size)
{
	int len = 0;
	bool overflow = false;

	frr_with_mutex (&logfilterlock) {
		for (int i = 0; i < zlog_filter_count; i++) {
			int ret;

			ret = snprintf(buf + len, max_size - len, " %s\n",
				       zlog_filters[i]);
			len += ret;
			if ((ret < 0) || ((size_t)len >= max_size)) {
				overflow = true;
				break;
			}
		}
	}

	return overflow ? -1 : len;
}

static int search_buf(const char *buf, size_t len)
{
	char *found = NULL;
	int ret = -1;

	frr_with_mutex (&logfilterlock) {
		for (int i = 0; i < zlog_filter_count; i++) {
			found = memmem(buf, len, zlog_filters[i],
				       strlen(zlog_filters[i]));
			if (found != NULL) {
				ret = 0;
				break;
			}
		}
	}

	return ret;
}

static void zlog_filterfile_fd(struct zlog_target *zt, struct zlog_msg *msgs[],
			       size_t nmsgs)
{
	FRR_VLA(struct zlog_msg *, msgfilt, nmsgs);
	size_t i, o = 0;
	const char *text;
	size_t text_len;

	for (i = 0; i < nmsgs; i++) {
		if (zlog_msg_prio(msgs[i]) >= LOG_DEBUG) {
			text = zlog_msg_text(msgs[i], &text_len);
			if (search_buf(text, text_len) < 0)
				continue;
		}
		msgfilt[o++] = msgs[i];
	}

	if (o)
		zlog_fd(zt, msgfilt, o);
}

void zlog_filterfile_init(struct zlog_cfg_filterfile *zcf)
{
	zlog_file_init(&zcf->parent);
	zcf->parent.zlog_wrap = zlog_filterfile_fd;
}

void zlog_filterfile_fini(struct zlog_cfg_filterfile *zcf)
{
	zlog_file_fini(&zcf->parent);
}
