/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <string.h>
#include "gnss.h"

#if defined(CONFIG_BUDDYGUARD_GNSS)
#include <zephyr/kernel.h>
#include "l76k.h"

#define FIX_STALE_MS 3000

K_MUTEX_DEFINE(status_lock);
K_THREAD_STACK_DEFINE(gnss_stack, 1536);
static struct k_thread gnss_thread;
static int startup_error = -EAGAIN;
static uint32_t valid_sentences;
static int64_t last_sentence_ms = -1;
static int64_t last_fix_ms;
static enum bg_gnss_fix latest_fix;
static int32_t latitude_e7, longitude_e7;
static int64_t position_ms = -1;

/* NMEA ddmm.mmmm / dddmm.mmmm to signed degrees × 10^7. */
static bool coordinate(const char *field, unsigned digits, const char *direction,
		       int32_t *out)
{
	const char *p = field;
	int32_t degrees = 0;
	for (unsigned i = 0; i < digits; ++i, ++p) {
		if (*p < '0' || *p > '9') {
			return false;
		}
		degrees = degrees * 10 + *p - '0';
	}
	if (p[0] < '0' || p[0] > '5' || p[1] < '0' || p[1] > '9' || p[2] != '.') {
		return false;
	}
	int32_t minutes = ((p[0] - '0') * 10 + p[1] - '0') * 10000000;
	int32_t place = 1000000;
	for (p += 3; *p >= '0' && *p <= '9'; ++p) {
		minutes += (*p - '0') * place;
		place /= 10;
	}
	bool negative = *direction == (digits == 2 ? 'S' : 'W');
	bool positive = *direction == (digits == 2 ? 'N' : 'E');
	if (*p != ',' || direction[1] != ',' || (!negative && !positive)) {
		return false;
	}
	int32_t limit = digits == 2 ? 90 : 180;
	if (degrees > limit || (degrees == limit && minutes)) {
		return false;
	}
	int32_t value = degrees * 10000000 + (minutes + 30) / 60;
	*out = negative ? -value : value;
	return true;
}

static bool sentence_position(const char *sentence, int32_t *lat, int32_t *lon)
{
	const char *field[6] = {sentence + 7};
	for (unsigned i = 1; i < ARRAY_SIZE(field); ++i) {
		const char *comma = strchr(field[i - 1], ',');
		if (!comma) {
			return false;
		}
		field[i] = comma + 1;
	}
	unsigned base = strncmp(sentence + 3, "GGA,", 4) == 0 ? 1 : 2;
	return coordinate(field[base], 2, field[base + 1], lat) &&
	       coordinate(field[base + 2], 3, field[base + 3], lon);
}

/* GGA and RMC each report fix status, even when there is no UTC or position. */
static enum bg_gnss_fix sentence_fix(const char *sentence)
{
	const bool gga = strncmp(sentence + 3, "GGA,", 4) == 0;
	if (!gga && strncmp(sentence + 3, "RMC,", 4) != 0) {
		return BG_GNSS_FIX_UNKNOWN;
	}

	const char *field = sentence + 7;
	for (unsigned int index = 1; index < (gga ? 6U : 2U); ++index) {
		field = strchr(field, ',');
		if (field == NULL) {
			return BG_GNSS_FIX_UNKNOWN;
		}
		++field;
	}
	if (field[0] == '\0' || (field[1] != ',' && field[1] != '*')) {
		return BG_GNSS_FIX_UNKNOWN;
	}
	if (field[0] == (gga ? '0' : 'V')) {
		return BG_GNSS_NO_FIX;
	}
	if (gga ? (field[0] >= '1' && field[0] <= '5') : field[0] == 'A') {
		return BG_GNSS_FIX_REPORTED;
	}
	return BG_GNSS_FIX_UNKNOWN;
}

static void gnss_worker(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (;;) {
		struct bg_l76k_sentence sentence;
		if (bg_l76k_receive(&sentence, K_FOREVER) != 0) {
			continue;
		}
		const enum bg_gnss_fix fix = sentence_fix(sentence.text);
		int32_t lat, lon;
		bool has_position = fix == BG_GNSS_FIX_REPORTED &&
				    sentence_position(sentence.text, &lat, &lon);
		const int64_t now = k_uptime_get();
		k_mutex_lock(&status_lock, K_FOREVER);
		++valid_sentences;
		last_sentence_ms = now;
		if (fix != BG_GNSS_FIX_UNKNOWN) {
			latest_fix = fix;
			last_fix_ms = now;
		}
		if (has_position) {
			latitude_e7 = lat;
			longitude_e7 = lon;
			position_ms = now;
		}
		k_mutex_unlock(&status_lock);
	}
}

void bg_gnss_start(void)
{
	const int rc = bg_l76k_start();
	k_mutex_lock(&status_lock, K_FOREVER);
	startup_error = rc;
	k_mutex_unlock(&status_lock);
	if (rc == 0) {
		k_thread_create(&gnss_thread, gnss_stack, K_THREAD_STACK_SIZEOF(gnss_stack),
				gnss_worker, NULL, NULL, NULL, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);
	}
}

struct bg_gnss_status bg_gnss_status(void)
{
	k_mutex_lock(&status_lock, K_FOREVER);
	const struct bg_l76k_stats stats = bg_l76k_stats();
	const int64_t now = k_uptime_get();
	struct bg_gnss_status status = {
		.enabled = true,
		.fix = now - last_fix_ms < FIX_STALE_MS ? latest_fix : BG_GNSS_FIX_UNKNOWN,
		.bytes = stats.bytes,
		.valid_sentences = valid_sentences,
		.invalid_sentences = stats.invalid_sentences,
		.dropped_sentences = stats.dropped_sentences,
		.sentence_age_ms = last_sentence_ms < 0 ? -1 : now - last_sentence_ms,
		.error = startup_error,
	};
	k_mutex_unlock(&status_lock);
	return status;
}

bool bg_gnss_position(int32_t *lat_e7, int32_t *lon_e7, uint8_t *age_s)
{
	k_mutex_lock(&status_lock, K_FOREVER);
	int64_t age_ms = k_uptime_get() - position_ms;
	bool fresh = position_ms >= 0 && age_ms < FIX_STALE_MS &&
		     latest_fix == BG_GNSS_FIX_REPORTED;
	if (fresh) {
		*lat_e7 = latitude_e7;
		*lon_e7 = longitude_e7;
		*age_s = age_ms / 1000;
	}
	k_mutex_unlock(&status_lock);
	return fresh;
}
#else
void bg_gnss_start(void) {}

struct bg_gnss_status bg_gnss_status(void)
{
	return (struct bg_gnss_status){.sentence_age_ms = -1};
}
bool bg_gnss_position(int32_t *lat_e7, int32_t *lon_e7, uint8_t *age_s)
{
	ARG_UNUSED(lat_e7);
	ARG_UNUSED(lon_e7);
	ARG_UNUSED(age_s);
	return false;
}
#endif

const char *bg_gnss_fix_name(enum bg_gnss_fix fix)
{
	switch (fix) {
	case BG_GNSS_NO_FIX:
		return "NO_FIX";
	case BG_GNSS_FIX_REPORTED:
		return "FIX_REPORTED";
	default:
		return "UNKNOWN";
	}
}
