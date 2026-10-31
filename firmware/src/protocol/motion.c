/* SPDX-License-Identifier: Apache-2.0 */
#include <string.h>
#include "motion.h"

void bg_motion_reset(struct bg_motion_filter *filter)
{
	memset(filter, 0, sizeof(*filter));
}

static int64_t magnitude(int64_t value)
{
	return value < 0 ? -value : value;
}

enum bg_motion bg_motion_update(struct bg_motion_filter *filter, uint32_t now_ms,
			       const int32_t accel_mg[3], const int32_t gyro_mrad_s[3])
{
	/* A sampling gap requires a new warm-up, not a fabricated STILL result. */
	if (filter->initialized && now_ms - filter->last_sample_ms > 1000U) {
		bg_motion_reset(filter);
	}
	if (!filter->initialized) {
		filter->initialized = true;
		filter->started_ms = now_ms;
		filter->last_sample_ms = now_ms;
		memcpy(filter->previous_accel_mg, accel_mg, sizeof(filter->previous_accel_mg));
		return BG_MOTION_UNKNOWN;
	}
	int64_t change = 0;
	bool rotation = false;
	for (unsigned axis = 0; axis < 3; ++axis) {
		change += magnitude((int64_t)accel_mg[axis] - filter->previous_accel_mg[axis]);
		rotation |= magnitude(gyro_mrad_s[axis]) > 300;
	}
	memcpy(filter->previous_accel_mg, accel_mg, sizeof(filter->previous_accel_mg));
	filter->last_sample_ms = now_ms;
	if (change > 150 || rotation) {
		filter->last_activity_ms = now_ms;
		filter->activity_seen = true;
	}
	if (now_ms - filter->started_ms < 1000U) {
		return BG_MOTION_UNKNOWN;
	}
	return filter->activity_seen && now_ms - filter->last_activity_ms < 2000U ?
		BG_MOTION_MOVING : BG_MOTION_STILL;
}

const char *bg_motion_name(uint8_t motion)
{
	switch (motion) {
	case BG_MOTION_STILL: return "STILL";
	case BG_MOTION_MOVING: return "MOVING";
	default: return "UNKNOWN";
	}
}
