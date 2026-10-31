/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_MOTION_H
#define BUDDYGUARD_MOTION_H
#include <stdbool.h>
#include <stdint.h>

enum bg_motion { BG_MOTION_UNKNOWN = 0, BG_MOTION_STILL = 1, BG_MOTION_MOVING = 2 };
struct bg_motion_filter {
	bool initialized, activity_seen;
	uint32_t started_ms, last_sample_ms, last_activity_ms;
	int32_t previous_accel_mg[3];
};
void bg_motion_reset(struct bg_motion_filter *filter);
enum bg_motion bg_motion_update(struct bg_motion_filter *filter, uint32_t now_ms,
			       const int32_t accel_mg[3], const int32_t gyro_mrad_s[3]);
const char *bg_motion_name(uint8_t motion);
#endif
