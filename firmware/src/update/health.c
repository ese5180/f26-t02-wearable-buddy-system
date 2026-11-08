/* SPDX-License-Identifier: Apache-2.0 */
/* Main-loop watchdog and trial-image confirmation; not a full business-function test.
 * 管理主循环看门狗与试运行确认，不代表全部业务功能已验证。
 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include "health.h"
LOG_MODULE_REGISTER(bg_health, LOG_LEVEL_INF);
static int64_t healthy_since;
static const struct device *const watchdog = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int watchdog_channel = -1;

int bg_health_watchdog_start(void)
{
	if (!device_is_ready(watchdog)) {
		return -ENODEV;
	}
	struct wdt_timeout_cfg cfg = {
		.window = {.min = 0, .max = 8000},
		.flags = WDT_FLAG_RESET_SOC,
	};
	watchdog_channel = wdt_install_timeout(watchdog, &cfg);
	if (watchdog_channel < 0) {
		return watchdog_channel;
	}
	return wdt_setup(watchdog, 0);
}

int bg_health_check_slots(void)
{
	const struct flash_area *area;
	const uint8_t ids[] = {FIXED_PARTITION_ID(slot0_partition),
		FIXED_PARTITION_ID(slot1_partition)};
	for (size_t i = 0; i < ARRAY_SIZE(ids); ++i) {
		int rc = flash_area_open(ids[i], &area);
		if (rc != 0) {
			return rc;
		}
		bool ready = device_is_ready(flash_area_get_device(area));
		LOG_INF("Image slot %u offset=0x%lx size=%lu ready=%u", (unsigned)i,
			(unsigned long)area->fa_off, (unsigned long)area->fa_size, ready);
		flash_area_close(area);
		if (!ready) {
			return -ENODEV;
		}
	}
	return 0;
}

void bg_health_begin(void)
{
	healthy_since = k_uptime_get();
}

void bg_health_cycle(bool healthy, bool allow_confirmation)
{
	/* Failure restarts the trial window and stops feeding WDT. Confirmation is
	 * irreversible for this trial: do not let the host confirm on upload alone.
	 * 失败重置健康窗口且停止喂狗；确认结束本轮回滚保护，不能仅凭上传成功确认。 */
	if (!healthy) {
		healthy_since = k_uptime_get();
		return;
	}
	if (!boot_is_img_confirmed() && allow_confirmation &&
		k_uptime_get() - healthy_since >=
		CONFIG_BUDDYGUARD_CONFIRM_AFTER_SECONDS * 1000LL) {
		int rc = boot_write_img_confirmed();
		LOG_INF("Image health check complete; confirm result=%d", rc);
	}
	(void)wdt_feed(watchdog, watchdog_channel);
}
