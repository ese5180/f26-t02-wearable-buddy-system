/* SPDX-License-Identifier: Apache-2.0 */
/* Counselor-only presence state; fresh BLE sequences drive loss/recovery and queued queries.
 * 辅导员名单状态；以新 BLE 序号判断失联／恢复，再排队 LoRa 查询。
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "radio_link.h"
#include "roster.h"
#include "wire.h"

LOG_MODULE_REGISTER(bg_roster, LOG_LEVEL_INF);

#if defined(CONFIG_BUDDYGUARD_COUNSELOR)
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/atomic.h>

struct member {
	atomic_t last_seen;
	atomic_t sequence;
	atomic_t maintenance;
	bool announced;
	bool missing;
};
static struct member roster[BG_MAX_TAGS + 1];
/* Count online -> missing transitions once; RAM resets this count on reboot. */
static atomic_t missing_events;

uint32_t bg_roster_missing_count(void)
{
	return (uint32_t)atomic_get(&missing_events);
}

static bool parse_presence(struct bt_data *data, void *user_data)
{
	ARG_UNUSED(user_data);
	struct bg_presence_frame frame;
	if (data->type != BT_DATA_MANUFACTURER_DATA ||
		!bg_presence_decode(data->data, data->data_len, &frame)) {
		return true;
	}
	struct member *m = &roster[frame.tag];
	uint16_t seq = frame.sequence;
	if (!atomic_get(&m->last_seen) || atomic_get(&m->sequence) != seq) {
		atomic_set(&m->sequence, seq);
		atomic_set(&m->maintenance, frame.flags & 1);
		atomic_set(&m->last_seen, (atomic_val_t)(uint32_t)k_uptime_get());
	}
	return false;
}

static void received(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
		     struct net_buf_simple *data)
{
	ARG_UNUSED(addr); ARG_UNUSED(rssi); ARG_UNUSED(type);
	bt_data_parse(data, parse_presence, NULL);
}

int bg_roster_start(void)
{
	/* Presence requires every fresh sequence, not discovery of each address
	 * once. The convenience macro enables controller duplicate filtering,
	 * which can suppress changing manufacturer payloads on this controller.
	 * Keep radio scan timing unchanged; deduplicate by sequence in our parser.
	 */
	const struct bt_le_scan_param parameters = {
		.type = BT_LE_SCAN_TYPE_PASSIVE,
		.options = BT_LE_SCAN_OPT_NONE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window = BT_GAP_SCAN_FAST_WINDOW,
	};
	int rc = bt_le_scan_start(&parameters, received);
	if (rc == 0) {
		LOG_INF("Counselor BLE roster started: controller duplicate filtering OFF; demo timeout=%u ms; maintenance grace=%u s",
			CONFIG_BUDDYGUARD_PRESENCE_TIMEOUT_MS,
			CONFIG_BUDDYGUARD_MAINTENANCE_GRACE_SECONDS);
	}
	return rc;
}

void bg_roster_poll(void)
{
	uint32_t now = (uint32_t)k_uptime_get();
	for (unsigned tag = 1; tag <= BG_MAX_TAGS; ++tag) {
		struct member *m = &roster[tag];
		uint32_t last = (uint32_t)atomic_get(&m->last_seen);
		if (!last) {
			continue;
		}
		uint32_t age = now - last;
		uint32_t timeout = atomic_get(&m->maintenance) ?
			CONFIG_BUDDYGUARD_MAINTENANCE_GRACE_SECONDS * 1000U :
			CONFIG_BUDDYGUARD_PRESENCE_TIMEOUT_MS;
		bool missing = age > timeout;
		if (!m->announced || (m->missing && !missing)) {
			LOG_INF("%s tag=T%02u seq=%ld maintenance=%ld", m->announced ? "RECOVERED" : "ONLINE",
				tag, (long)atomic_get(&m->sequence), (long)atomic_get(&m->maintenance));
			m->announced = true;
		}
		if (missing && !m->missing) {
			atomic_inc(&missing_events);
			LOG_WRN("BLE MISSING tag=T%02u age_ms=%u; requesting LoRa status", tag, age);
			int rc = bg_radio_query(tag);
			if (rc) {
				LOG_WRN("Unable to queue status query for T%02u: %d", tag, rc);
			}
		}
		m->missing = missing;
	}
}
#else
int bg_roster_start(void) { return 0; }
void bg_roster_poll(void) {}
uint32_t bg_roster_missing_count(void) { return 0; }
#endif
