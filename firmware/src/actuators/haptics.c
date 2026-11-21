/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include "haptics.h"

#if defined(CONFIG_BUDDYGUARD_HAPTICS)
#include <zephyr/device.h>
#include <zephyr/drivers/haptics/drv2605.h>
#include <zephyr/kernel.h>

static const struct device *const motor = DEVICE_DT_GET(DT_NODELABEL(haptics_motor));
K_MUTEX_DEFINE(playback_lock);
static int64_t next_effect_ms;

int bg_haptics_play(void)
{
	if (!device_is_ready(motor)) {
		return -ENODEV;
	}

	k_mutex_lock(&playback_lock, K_FOREVER);
	int rc = -EBUSY;
	if (k_uptime_get() >= next_effect_ms) {
		struct drv2605_rom_data effect = {
			.trigger = DRV2605_MODE_INTERNAL_TRIGGER,
			.library = DRV2605_LIBRARY_TS2200_A,
			.seq_regs = {15, 0},
		};
		const union drv2605_config_data config = {.rom_data = &effect};
		rc = drv2605_haptic_config(motor, DRV2605_HAPTICS_SOURCE_ROM, &config);
		if (rc == 0) {
			rc = haptics_start_output(motor);
			/* Allow the ~750 ms effect to finish before accepting another. */
			next_effect_ms = k_uptime_get() + 1000;
		}
	}
	k_mutex_unlock(&playback_lock);
	return rc;
}
#else
int bg_haptics_play(void)
{
	return -ENOTSUP;
}
#endif
