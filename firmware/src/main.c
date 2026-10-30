/* SPDX-License-Identifier: Apache-2.0 */
/* Boot orchestration only; each service owns its state and hardware. */
#include <zephyr/app_version.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "health.h"
#include "presence.h"
#include "radio_link.h"
#include "roster.h"
LOG_MODULE_REGISTER(buddyguard, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("BuddyGuard version=%s role=%s tag=%u confirmed=%u", APP_VERSION_STRING,
		IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? "COUNSELOR" : "TAG",
		CONFIG_BUDDYGUARD_TAG_ID, boot_is_img_confirmed());
	int rc = bg_health_watchdog_start();
	if (rc != 0) {
		LOG_ERR("Watchdog unavailable: %d; image will not be confirmed", rc);
		return rc;
	}
	if (bg_health_check_slots() != 0 || bg_radio_init() != 0 || bg_presence_init() != 0 ||
		bg_roster_start() != 0) {
		LOG_ERR("Startup health check failed; waiting for watchdog reset");
		for (;;) {
			k_sleep(K_SECONDS(1));
		}
	}
	if (IS_ENABLED(CONFIG_BUDDYGUARD_FAULT_ON_BOOT) && !boot_is_img_confirmed()) {
		LOG_ERR("FAULT INJECTION: unconfirmed image will stop feeding the watchdog");
		for (;;) {
			k_sleep(K_SECONDS(1));
		}
	}
	bg_health_begin();
	for (;;) {
		bg_roster_poll();
		rc = bg_presence_tick();
		if (rc != 0) {
			bg_health_cycle(false, false);
			LOG_ERR("Advertising error: %d; withholding confirmation and watchdog feed", rc);
			k_sleep(K_SECONDS(1));
			continue;
		}
		struct bg_presence_status s = bg_presence_status();
		bg_health_cycle(true, s.heartbeat);
		LOG_INF("Presence seq=%u advertising=%u connected=%ld maintenance=%u confirmed=%u",
			s.sequence, s.advertising, (long)s.connected, s.maintenance,
			boot_is_img_confirmed());
		k_sleep(K_SECONDS(2));
	}
	return 0;
}
