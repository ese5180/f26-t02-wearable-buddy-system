/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "radio_link.h"
#include "roster.h"

LOG_MODULE_REGISTER(bg_roster, LOG_LEVEL_INF);

#if defined(CONFIG_BUDDYGUARD_COUNSELOR)
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>

struct member {
	atomic_t last_seen;
	atomic_t sequence;
	atomic_t maintenance;
	bool announced;
	bool missing;
};
static struct member roster[4];

static bool parse_presence(struct bt_data *data, void *user_data)
{
	ARG_UNUSED(user_data);
	const uint8_t *p = data->data;
	if (data->type != BT_DATA_MANUFACTURER_DATA || data->data_len != 8 ||
		p[0] != 0xff || p[1] != 0xff || p[2] != 1 || p[7] != 1 ||
		p[3] == 0 || p[3] > 3 || (p[6] & ~3)) {
		return true;
	}
	struct member *m = &roster[p[3]];
	uint16_t seq = sys_get_le16(p + 4);
	if (!atomic_get(&m->last_seen) || atomic_get(&m->sequence) != seq) {
		atomic_set(&m->sequence, seq);
		atomic_set(&m->maintenance, p[6] & 1);
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
	int rc = bt_le_scan_start(BT_LE_SCAN_PASSIVE, received);
	if (rc == 0) {
		LOG_INF("Counselor BLE roster started: demo timeout=%u ms; maintenance grace=120 s",
			CONFIG_BUDDYGUARD_PRESENCE_TIMEOUT_MS);
	}
	return rc;
}

void bg_roster_poll(void)
{
	uint32_t now = (uint32_t)k_uptime_get();
	for (unsigned tag = 1; tag <= 3; ++tag) {
		struct member *m = &roster[tag];
		uint32_t last = (uint32_t)atomic_get(&m->last_seen);
		if (!last) {
			continue;
		}
		uint32_t age = now - last;
		uint32_t timeout = atomic_get(&m->maintenance) ?
			120000U : CONFIG_BUDDYGUARD_PRESENCE_TIMEOUT_MS;
		bool missing = age > timeout;
		if (!m->announced || (m->missing && !missing)) {
			LOG_INF("%s tag=T%02u seq=%ld maintenance=%ld", m->announced ? "RECOVERED" : "ONLINE",
				tag, (long)atomic_get(&m->sequence), (long)atomic_get(&m->maintenance));
			m->announced = true;
		}
		if (missing && !m->missing) {
			LOG_WRN("BLE MISSING tag=T%02u age_ms=%u; requesting LoRa status", tag, age);
			(void)bg_radio_query(tag);
		}
		m->missing = missing;
	}
}
#else
int bg_roster_start(void) { return 0; }
void bg_roster_poll(void) {}
#endif
