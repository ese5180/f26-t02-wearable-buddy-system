/* SPDX-License-Identifier: Apache-2.0 */
/* USB shell adapter; delegates to service APIs rather than operating hardware.
 * USB 命令适配层；调用服务接口，不直接操作硬件。
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/app_version.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include "presence.h"
#include "radio_link.h"
#include "imu.h"
#if defined(CONFIG_BUDDYGUARD_FLEET)
#include "memfault/core/reboot_tracking.h"
#endif
static int cmd_bg(const struct shell *shell, size_t argc, char **argv)
{
	if (!strcmp(argv[1], "radio") && argc == 2) {
		struct bg_radio_stats s = bg_radio_get_stats();
		shell_print(shell, "radio tx_attempts=%u completed=%u exhausted=%u rx=%u interleaved=%u duplicates=%u queries=%u replies=%u queue_drops=%u",
			s.tx_attempts, s.tx_completed, s.tx_exhausted, s.rx_valid,
			s.rx_interleaved, s.rx_duplicates, s.queries_received,
			s.replies_received, s.queue_drops);
		return 0;
	}
	if (!strcmp(argv[1], "imu") && argc == 2) {
		struct bg_imu_snapshot s = bg_imu_get();
		shell_print(shell, "imu enabled=%u available=%u address=0x%02x whoami=0x%02x motion=%s samples=%u age_ms=%lld error=%d",
			s.enabled, s.available, s.address, s.whoami, bg_motion_name(s.motion),
			s.samples, s.samples ? (long long)(k_uptime_get() - s.sampled_ms) : -1LL,
			s.last_error);
		shell_print(shell, "accel_mg=(%d,%d,%d) gyro_mrad_s=(%d,%d,%d)",
			s.accel_mg[0], s.accel_mg[1], s.accel_mg[2], s.gyro_mrad_s[0],
			s.gyro_mrad_s[1], s.gyro_mrad_s[2]);
		shell_print(shell, "impacts=%u last_impact_mg=%d", s.impacts, s.last_impact_mg);
		return 0;
	}
	if (!strcmp(argv[1], "status") && argc == 2) {
		struct bg_presence_status s = bg_presence_status();
		shell_print(shell, "version=%s role=%s tag=%u sequence=%u connected=%ld maintenance=%u confirmed=%u",
			APP_VERSION_STRING, IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? "COUNSELOR" : "TAG",
			CONFIG_BUDDYGUARD_TAG_ID, s.sequence,
			(long)s.connected, s.maintenance, boot_is_img_confirmed());
		return 0;
	}
	if (!strcmp(argv[1], "maintenance") && argc == 3) {
		char *end;
		unsigned long seconds = strtoul(argv[2], &end, 10);
		if (!argv[2][0] || *end || seconds > 600) {
			return -EINVAL;
		}
		bg_presence_maintenance(seconds);
		shell_print(shell, "Maintenance admission window: %lu seconds", seconds);
		return 0;
	}
	if (!strcmp(argv[1], "heartbeat") && argc == 3) {
		if (strcmp(argv[2], "on") && strcmp(argv[2], "off")) {
			return -EINVAL;
		}
		bg_presence_heartbeat(!strcmp(argv[2], "on"));
		shell_print(shell, "Heartbeat %s", argv[2]);
		return 0;
	}
	if (!strcmp(argv[1], "reboot") && argc == 2) {
#if defined(CONFIG_BUDDYGUARD_FLEET)
		/* Preserve user intent: the hardware alone only reports a software reset. */
		MEMFAULT_REBOOT_MARK_RESET_IMMINENT(kMfltRebootReason_UserReset);
#endif
		sys_reboot(SYS_REBOOT_COLD);
		return 0;
	}
	if (!strcmp(argv[1], "sos") && argc == 2) {
		int rc = bg_radio_sos();
		shell_print(shell, "SOS queue result=%d", rc);
		return rc;
	}
	if (!strcmp(argv[1], "query") && argc == 3) {
		char *end;
		unsigned long tag = strtoul(argv[2], &end, 10);
		if (!argv[2][0] || *end || tag == 0 || tag > 3) {
			return -EINVAL;
		}
		return bg_radio_query(tag);
	}
	shell_error(shell, "bg status | radio | imu | maintenance <0..600> | heartbeat <on|off> | sos | query <1..3> | reboot");
	return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(bg, NULL, "BuddyGuard laboratory controls", cmd_bg, 2, 1);
