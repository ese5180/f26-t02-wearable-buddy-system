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
/* Caller owns filter and feeds samples in time order; no bus access or allocation.
 * 调用者持有 filter、按时间顺序输入样本；本接口不访问总线、不分配内存。 */
enum bg_motion bg_motion_update(struct bg_motion_filter *filter, uint32_t now_ms,
			       const int32_t accel_mg[3], const int32_t gyro_mrad_s[3]);
const char *bg_motion_name(uint8_t motion);

/* Impact (impulse) detector: fires when any single axis changes by at least
 * threshold_mg between two consecutive samples, then ignores impacts for
 * lockout_ms. Pure logic like the motion filter: no bus access, caller owns state.
 * 冲击检测：任一轴相邻两次采样的变化 >= 阈值即触发，之后锁定 lockout_ms。 */
struct bg_impact_detector {
	bool initialized, fired;
	uint32_t last_sample_ms, last_impact_ms;
	int32_t previous_accel_mg[3];
};
struct bg_impact {
	bool detected;
	bool hardware;     /* true: the IMU's own wake-up detector saw it */
	uint8_t axis;      /* 0 = x, 1 = y, 2 = z */
	int32_t delta_mg;  /* largest absolute single-axis change between polled samples */
};
/* hw_axes: axes flagged by the IMU's latched wake-up detector since the last poll
 * (bit0 = x, bit1 = y, bit2 = z; 0 = none). It runs on every 417 Hz sample, so it
 * catches ~1 ms knocks that 25 Hz polling misses. Either source fires the impact;
 * both share the lockout. */
void bg_impact_reset(struct bg_impact_detector *detector);
struct bg_impact bg_impact_update(struct bg_impact_detector *detector, uint32_t now_ms,
				  const int32_t accel_mg[3], int32_t threshold_mg,
				  uint32_t lockout_ms, uint8_t hw_axes);
#endif
