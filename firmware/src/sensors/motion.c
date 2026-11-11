/* SPDX-License-Identifier: Apache-2.0 */
/* Pure motion filter; caller supplies timestamps, acceleration (mg) and gyro (mrad/s).
 * 纯运动判断；调用者提供毫秒时间、mg 加速度与 mrad/s 角速度。
 */
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

void bg_impact_reset(struct bg_impact_detector *detector)
{
	memset(detector, 0, sizeof(*detector));
}

struct bg_impact bg_impact_update(struct bg_impact_detector *detector, uint32_t now_ms,
				  const int32_t accel_mg[3], int32_t threshold_mg,
				  uint32_t lockout_ms, uint8_t hw_axes)
{
	struct bg_impact result = {0};
	/* After a sampling gap the previous sample is stale: compare against nothing. */
	if (detector->initialized && now_ms - detector->last_sample_ms > 1000U) {
		bool fired = detector->fired;
		uint32_t last_impact_ms = detector->last_impact_ms;
		bg_impact_reset(detector);
		detector->fired = fired;          /* keep the lockout across a gap */
		detector->last_impact_ms = last_impact_ms;
	}
	bool have_previous = detector->initialized;
	if (have_previous) {
		for (unsigned axis = 0; axis < 3; ++axis) {
			int32_t delta = (int32_t)magnitude((int64_t)accel_mg[axis] -
							   detector->previous_accel_mg[axis]);
			if (delta > result.delta_mg) {
				result.delta_mg = delta;
				result.axis = (uint8_t)axis;
			}
		}
	}
	detector->initialized = true;
	detector->last_sample_ms = now_ms;
	memcpy(detector->previous_accel_mg, accel_mg, sizeof(detector->previous_accel_mg));

	bool software = have_previous && result.delta_mg >= threshold_mg;
	hw_axes &= 0x7;
	if (hw_axes) {
		result.hardware = true;
		result.axis = (hw_axes & 1) ? 0 : (hw_axes & 2) ? 1 : 2;
	}
	bool locked = detector->fired && now_ms - detector->last_impact_ms < lockout_ms;
	if ((software || hw_axes) && !locked) {
		result.detected = true;
		detector->fired = true;
		detector->last_impact_ms = now_ms;
	}
	return result;
}
