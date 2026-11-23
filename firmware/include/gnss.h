/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_GNSS_H
#define BUDDYGUARD_GNSS_H

#include <stdbool.h>
#include <stdint.h>

enum bg_gnss_fix {
	BG_GNSS_FIX_UNKNOWN,
	BG_GNSS_NO_FIX,
	BG_GNSS_FIX_REPORTED,
};

struct bg_gnss_status {
	bool enabled;
	enum bg_gnss_fix fix;
	uint32_t bytes;
	uint32_t valid_sentences;
	uint32_t invalid_sentences;
	uint32_t dropped_sentences;
	int64_t sentence_age_ms;
	int error;
};

void bg_gnss_start(void);
struct bg_gnss_status bg_gnss_status(void);
bool bg_gnss_position(int32_t *lat_e7, int32_t *lon_e7, uint8_t *age_s);
const char *bg_gnss_fix_name(enum bg_gnss_fix fix);

#endif
