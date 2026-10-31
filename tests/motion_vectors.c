/* Native checks for gravity, warm-up, activity hold, stale data and clock wrap. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "motion.h"

int main(void)
{
	struct bg_motion_filter f;
	int32_t accel[] = {0, 0, 1000}, gyro[] = {0, 0, 0};
	bg_motion_reset(&f);
	assert(bg_motion_update(&f, 0, accel, gyro) == BG_MOTION_UNKNOWN);
	for (uint32_t t = 40; t < 1000; t += 40) {
		assert(bg_motion_update(&f, t, accel, gyro) == BG_MOTION_UNKNOWN);
	}
	assert(bg_motion_update(&f, 1000, accel, gyro) == BG_MOTION_STILL);
	accel[0] = 200;
	assert(bg_motion_update(&f, 1040, accel, gyro) == BG_MOTION_MOVING);
	for (uint32_t t = 1080; t < 3040; t += 40) {
		assert(bg_motion_update(&f, t, accel, gyro) == BG_MOTION_MOVING);
	}
	assert(bg_motion_update(&f, 3040, accel, gyro) == BG_MOTION_STILL);
	gyro[1] = -400;
	assert(bg_motion_update(&f, 3080, accel, gyro) == BG_MOTION_MOVING);
	assert(bg_motion_update(&f, 4081, accel, gyro) == BG_MOTION_UNKNOWN);
	/* Constant gravity in another orientation is still, even over uint32 wrap. */
	bg_motion_reset(&f);
	accel[0] = 1000; accel[2] = 0; gyro[1] = 0;
	uint32_t start = UINT32_MAX - 400;
	for (uint32_t elapsed = 0; elapsed < 1000; elapsed += 40) {
		assert(bg_motion_update(&f, start + elapsed, accel, gyro) == BG_MOTION_UNKNOWN);
	}
	assert(bg_motion_update(&f, start + 1000, accel, gyro) == BG_MOTION_STILL);
	assert(strcmp(bg_motion_name(0), "UNKNOWN") == 0);
	assert(strcmp(bg_motion_name(1), "STILL") == 0);
	assert(strcmp(bg_motion_name(2), "MOVING") == 0);
	assert(strcmp(bg_motion_name(255), "UNKNOWN") == 0);
	return 0;
}
