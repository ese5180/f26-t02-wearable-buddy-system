/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_IMU_H
#define BUDDYGUARD_IMU_H
#include <stdbool.h>
#include <stdint.h>
#include "motion.h"

struct bg_imu_snapshot {
	bool enabled, available;
	uint8_t address, whoami;
	enum bg_motion motion;
	/* x/y/z, mg and mrad/s. / 三轴，分别为 mg 与 mrad/s。 */
	int32_t accel_mg[3], gyro_mrad_s[3];
	uint32_t samples;
	uint32_t impacts;        /* impacts detected since boot / 启动以来冲击次数 */
	int32_t last_impact_mg;  /* largest axis change of the last impact */
	int64_t sampled_ms;
	int last_error;
};
/* Optional service: a missing IMU never blocks main or image confirmation. */
void bg_imu_start(void);
struct bg_imu_snapshot bg_imu_get(void);
/* Missing/stale data returns UNKNOWN. / 缺失或过期时返回 UNKNOWN。 */
enum bg_motion bg_imu_motion(void);
#endif
