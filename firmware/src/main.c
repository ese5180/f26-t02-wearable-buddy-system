/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/app_version.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/device.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/shell/shell.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/sys/util.h>
#include "radio_link.h"
#include "roster.h"

LOG_MODULE_REGISTER(buddyguard, LOG_LEVEL_INF);

/* 0xffff is a laboratory placeholder, not a company identifier for production.
 * On-air payload: company(2), protocol(1), tag(1), sequence LE(2), flags(1), role(1).
 * Flags: bit0 maintenance, bit1 unconfirmed. No child names or location are sent.
 */
static uint8_t presence[] = {0xff, 0xff, 1, CONFIG_BUDDYGUARD_TAG_ID, 0, 0, 0, 1};
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, SMP_BT_SVC_UUID_VAL),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, presence, sizeof(presence)),
};
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static atomic_t connected;
static atomic_t restart_advertising;
static atomic_t awaiting_recycle;
static atomic_t heartbeat_enabled = ATOMIC_INIT(1);
static atomic_t maintenance_deadline;
static bool advertising;
static bool advertising_connectable;
static uint16_t sequence;
static const struct device *const watchdog = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int watchdog_channel = -1;

static bool in_maintenance(void)
{
	uint32_t now = (uint32_t)(k_uptime_get() / 1000);
	uint32_t until = (uint32_t)atomic_get(&maintenance_deadline);
	return (int32_t)(until - now) > 0;
}

static void on_connected(struct bt_conn *conn, uint8_t error)
{
	ARG_UNUSED(conn);
	if (error) {
		atomic_set(&restart_advertising, 1);
		LOG_WRN("BLE connection failed: %u", error);
		return;
	}
	atomic_set(&connected, 1);
	LOG_INF("BLE client connected; presence advertising pauses during the connection");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	atomic_set(&awaiting_recycle, 1);
	atomic_set(&connected, 0);
	LOG_INF("BLE client disconnected: %u", reason);
}

static void on_recycled(void)
{
	/* Stopping a connectable advertisement also recycles its reserved
	 * connection object. Only a real client disconnection needs a restart.
	 */
	if (atomic_cas(&awaiting_recycle, 1, 0)) {
		atomic_set(&restart_advertising, 1);
	}
}

BT_CONN_CB_DEFINE(connection_callbacks) = {
	.connected = on_connected,
	.disconnected = on_disconnected,
	.recycled = on_recycled,
};

static int update_advertising(void)
{
	bool maintenance = in_maintenance();
	if (atomic_get(&connected)) {
		/* Limit the maintenance admission window; an active upload is allowed to
		 * finish. Production access authorization is a separate requirement.
		 */
		advertising = false;
		return 0;
	}
	if (atomic_cas(&restart_advertising, 1, 0)) {
		if (advertising) {
			(void)bt_le_adv_stop();
		}
		advertising = false;
	}
	if (!atomic_get(&heartbeat_enabled)) {
		if (advertising) {
			(void)bt_le_adv_stop();
			advertising = false;
		}
		return 0;
	}
	if (advertising && advertising_connectable != maintenance) {
		(void)bt_le_adv_stop();
		advertising = false;
	}
	sys_put_le16(sequence, &presence[4]);
	presence[6] = (maintenance ? BIT(0) : 0) |
		(boot_is_img_confirmed() ? 0 : BIT(1));
	if (advertising) {
		return bt_le_adv_update_data(ad, ARRAY_SIZE(ad),
			maintenance ? sd : NULL, maintenance ? ARRAY_SIZE(sd) : 0);
	}
	const struct bt_le_adv_param *params = maintenance ?
		BT_LE_ADV_CONN_FAST_1 : BT_LE_ADV_NCONN_IDENTITY;
	int rc = bt_le_adv_start(params, ad, ARRAY_SIZE(ad),
		maintenance ? sd : NULL, maintenance ? ARRAY_SIZE(sd) : 0);
	if (rc == 0) {
		advertising = true;
		advertising_connectable = maintenance;
		LOG_INF("Advertising started: %s", maintenance ? "maintenance/SMP" : "presence");
	}
	return rc;
}

static int start_watchdog(void)
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

static int check_image_slots(void)
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

static int cmd_bg(const struct shell *shell, size_t argc, char **argv)
{
	if (!strcmp(argv[1], "status") && argc == 2) {
		shell_print(shell, "version=%s role=%s tag=%u sequence=%u connected=%ld maintenance=%u confirmed=%u",
			APP_VERSION_STRING, IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? "COUNSELOR" : "TAG",
			CONFIG_BUDDYGUARD_TAG_ID, sequence,
			(long)atomic_get(&connected), in_maintenance(), boot_is_img_confirmed());
		return 0;
	}
	if (!strcmp(argv[1], "maintenance") && argc == 3) {
		char *end;
		unsigned long seconds = strtoul(argv[2], &end, 10);
		if (!argv[2][0] || *end || seconds > 600) {
			return -EINVAL;
		}
		atomic_set(&maintenance_deadline, (atomic_val_t)(k_uptime_get() / 1000 + seconds));
		shell_print(shell, "Maintenance admission window: %lu seconds", seconds);
		return 0;
	}
	if (!strcmp(argv[1], "heartbeat") && argc == 3) {
		if (strcmp(argv[2], "on") && strcmp(argv[2], "off")) {
			return -EINVAL;
		}
		atomic_set(&heartbeat_enabled, !strcmp(argv[2], "on"));
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

int main(void)
{
	presence[7] = IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? 2 : 1;
	LOG_INF("BuddyGuard version=%s role=%s tag=%u confirmed=%u", APP_VERSION_STRING,
		IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? "COUNSELOR" : "TAG",
		CONFIG_BUDDYGUARD_TAG_ID, boot_is_img_confirmed());
	int rc = start_watchdog();
	if (rc != 0) {
		LOG_ERR("Watchdog unavailable: %d; image will not be confirmed", rc);
		return rc;
	}
	if (check_image_slots() != 0 || bg_radio_init() != 0 || bt_enable(NULL) != 0 ||
		bg_roster_start() != 0) {
		LOG_ERR("Startup health check failed; waiting for watchdog reset");
		for (;;) {
			k_sleep(K_SECONDS(1));
		}
	}
	atomic_set(&maintenance_deadline, (atomic_val_t)(k_uptime_get() / 1000 +
		CONFIG_BUDDYGUARD_MAINTENANCE_SECONDS));
	if (IS_ENABLED(CONFIG_BUDDYGUARD_FAULT_ON_BOOT) && !boot_is_img_confirmed()) {
		LOG_ERR("FAULT INJECTION: unconfirmed image will stop feeding the watchdog");
		for (;;) {
			k_sleep(K_SECONDS(1));
		}
	}
	int64_t healthy_since = k_uptime_get();
	for (;;) {
		bg_roster_poll();
		++sequence;
		rc = update_advertising();
		if (rc != 0) {
			healthy_since = k_uptime_get();
			LOG_ERR("Advertising error: %d; withholding confirmation and watchdog feed", rc);
			k_sleep(K_SECONDS(1));
			continue;
		}
		if (!boot_is_img_confirmed() && atomic_get(&heartbeat_enabled) &&
			k_uptime_get() - healthy_since >=
			CONFIG_BUDDYGUARD_CONFIRM_AFTER_SECONDS * 1000LL) {
			rc = boot_write_img_confirmed();
			LOG_INF("Image health check complete; confirm result=%d", rc);
		}
		LOG_INF("Presence seq=%u advertising=%u connected=%ld maintenance=%u confirmed=%u",
			sequence, advertising, (long)atomic_get(&connected), in_maintenance(),
			boot_is_img_confirmed());
		(void)wdt_feed(watchdog, watchdog_channel);
		k_sleep(K_SECONDS(2));
	}
	return 0;
}
