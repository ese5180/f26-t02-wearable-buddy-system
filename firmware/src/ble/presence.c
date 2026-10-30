/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/transport/smp_bt.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include "presence.h"
#include "wire.h"

LOG_MODULE_REGISTER(bg_presence, LOG_LEVEL_INF);

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
	bg_presence_encode(presence, CONFIG_BUDDYGUARD_TAG_ID, sequence,
		(maintenance ? 1 : 0) | (boot_is_img_confirmed() ? 0 : 2),
		IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? 2 : 1);
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

int bg_presence_init(void)
{
	int rc = bt_enable(NULL);
	if (rc == 0) {
		bg_presence_maintenance(CONFIG_BUDDYGUARD_MAINTENANCE_SECONDS);
	}
	return rc;
}

int bg_presence_tick(void)
{
	++sequence;
	return update_advertising();
}

void bg_presence_maintenance(unsigned seconds)
{
	atomic_set(&maintenance_deadline,
		(atomic_val_t)(k_uptime_get() / 1000 + seconds));
}

void bg_presence_heartbeat(bool enabled)
{
	atomic_set(&heartbeat_enabled, enabled);
}

struct bg_presence_status bg_presence_status(void)
{
	return (struct bg_presence_status) {
		.sequence = sequence, .advertising = advertising,
		.connected = atomic_get(&connected), .maintenance = in_maintenance(),
		.heartbeat = atomic_get(&heartbeat_enabled),
	};
}
