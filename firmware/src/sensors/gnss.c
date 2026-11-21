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
		const int64_t now = k_uptime_get();
		k_mutex_lock(&status_lock, K_FOREVER);
		++valid_sentences;
		last_sentence_ms = now;
		if (fix != BG_GNSS_FIX_UNKNOWN) {
			latest_fix = fix;
			last_fix_ms = now;
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
#else
void bg_gnss_start(void) {}

struct bg_gnss_status bg_gnss_status(void)
{
	return (struct bg_gnss_status){.sentence_age_ms = -1};
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
