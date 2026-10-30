/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/app_version.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include "presence.h"
#include "radio_link.h"
static int cmd_bg(const struct shell *shell, size_t argc, char **argv)
{
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
	shell_error(shell, "bg status | maintenance <0..600> | heartbeat <on|off> | sos | query <1..3> | reboot");
	return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(bg, NULL, "BuddyGuard laboratory controls", cmd_bg, 2, 1);
