/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <string.h>
#include <zephyr/bluetooth/att.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/byteorder.h>
#include "app_ble.h"
#include "radio_link.h"
#include "roster.h"
#include "wire.h"

LOG_MODULE_REGISTER(bg_app, LOG_LEVEL_INF);

enum { VERSION = 1, CONTROL_SIZE = 16, EVENT_SIZE = 20 };
enum { SYNC = 1, GATHER = 2, ROLL_CALL = 3 };
enum { TAG_STATE = 1, SOS, GATHER_RESULT, SYNC_DONE, TAG_POSITION };

struct event {
	int64_t time_ms;
	uint32_t id;
	uint32_t request;
	uint32_t data;
	uint8_t kind, tag, value;
};

static struct event history[32], outbox[64], gather_cache[8];
static struct event positions[BG_MAX_TAGS + 1];
static uint8_t tag_state[BG_MAX_TAGS + 1];
static uint32_t boot_id, newest_id, pending_request;
static uint32_t roll_calls[8];
static size_t queued, cache_next;
static size_t roll_call_next;
static int64_t next_snapshots;
static struct bt_conn *client;
static bool in_flight;
static uint8_t payload[EVENT_SIZE];
K_MUTEX_DEFINE(app_lock);

static ssize_t control_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

#define CONTROL_UUID BT_UUID_128_ENCODE(0x7f3f8d94, 0xcb46, 0x46a7, 0xac11, 0x91d75bacf707)
#define EVENT_UUID BT_UUID_128_ENCODE(0x8ed898b1, 0x50a1, 0x4dad, 0x8779, 0x2bfa73ed78b9)

BT_GATT_SERVICE_DEFINE(app_service,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(BG_APP_SERVICE_UUID)),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(CONTROL_UUID), BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE_ENCRYPT, NULL, control_write, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(EVENT_UUID), BT_GATT_CHRC_INDICATE,
			       BT_GATT_PERM_READ_ENCRYPT, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT)
);
#define EVENT_ATTR (&app_service.attrs[4])

static void publish_work(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(publisher, publish_work);

static void queue_event(struct event e)
{
	if (client && queued < ARRAY_SIZE(outbox)) {
		outbox[queued++] = e;
		k_work_schedule(&publisher, K_NO_WAIT);
	}
}

static void record_event(struct event *e)
{
	e->id = ++newest_id;
	history[(e->id - 1) % ARRAY_SIZE(history)] = *e;
	queue_event(*e);
}

static uint32_t elapsed_ms(int64_t then, int64_t now)
{
	return then < 0 ? UINT32_MAX : (uint32_t)CLAMP(now - then, 0, (int64_t)UINT32_MAX - 1);
}

static void encode_event(const struct event *e)
{
	int64_t now = k_uptime_get();
	uint32_t data = e->data;
	uint8_t value = e->value;

	if (e->kind == TAG_POSITION) {
		value = MIN(e->value + elapsed_ms(e->time_ms, now) / 1000, 255);
	} else if (e->kind != SYNC_DONE) {
		data = elapsed_ms(e->time_ms, now);
	}
	payload[0] = VERSION;
	payload[1] = e->kind;
	payload[2] = e->tag;
	payload[3] = value;
	sys_put_le32(e->id, payload + 4);
	sys_put_le32(e->request, payload + 8);
	sys_put_le32(data, payload + 12);
	sys_put_le32(boot_id, payload + 16);
}

static void indication_done(struct bt_gatt_indicate_params *params)
{
	ARG_UNUSED(params);
	k_mutex_lock(&app_lock, K_FOREVER);
	in_flight = false;
	if (queued) {
		k_work_schedule(&publisher, K_NO_WAIT);
	}
	k_mutex_unlock(&app_lock);
}

static struct bt_gatt_indicate_params indication = {
	.attr = EVENT_ATTR, .destroy = indication_done, .data = payload, .len = sizeof(payload),
};

static void publish_work(struct k_work *work)
{
	ARG_UNUSED(work);
	k_mutex_lock(&app_lock, K_FOREVER);
	if (!client || !queued || in_flight) {
		k_mutex_unlock(&app_lock);
		return;
	}

	struct bt_conn *conn = bt_conn_ref(client);
	encode_event(&outbox[0]);
	/* Parameter and payload storage belong to Zephyr until destroy(). */
	in_flight = true;
	int rc = bt_gatt_indicate(conn, &indication);
	if (rc == 0) {
		--queued;
		memmove(outbox, outbox + 1, queued * sizeof(outbox[0]));
	} else {
		in_flight = false;
		if (rc == -ENOMEM || rc == -EBUSY) {
			k_work_schedule(&publisher, K_MSEC(30));
		} else {
			queued = 0;
			LOG_DBG("App indication stopped: %d", rc);
		}
	}
	k_mutex_unlock(&app_lock);
	bt_conn_unref(conn);
}

static void queue_snapshots(uint32_t request, bool include_positions)
{
	for (uint8_t tag = 1; tag <= BG_MAX_TAGS; ++tag) {
		queue_event((struct event){
			.time_ms = bg_roster_last_seen(tag),
			.request = request, .kind = TAG_STATE, .tag = tag, .value = tag_state[tag],
		});
	}
	if (include_positions) {
		for (uint8_t tag = 1; tag <= BG_MAX_TAGS; ++tag) {
			if (positions[tag].kind) {
				queue_event(positions[tag]);
			}
		}
	}
}

static void sync_client(uint32_t request, uint32_t last_boot, uint32_t last_id)
{
	uint32_t oldest = newest_id > ARRAY_SIZE(history) ? newest_id - ARRAY_SIZE(history) + 1 : 1;
	uint32_t start = oldest;
	uint8_t result = 0;

	queued = 0;
	if (last_boot) {
		if (last_boot != boot_id) {
			result = 2;
		} else if (last_id >= newest_id) {
			start = newest_id + 1;
		} else if (last_id + 1 < oldest) {
			result = 1;
		} else {
			start = last_id + 1;
		}
	}
	for (uint32_t id = start; id <= newest_id; ++id) {
		queue_event(history[(id - 1) % ARRAY_SIZE(history)]);
	}
	queue_snapshots(request, true);
	queue_event((struct event){
		.kind = SYNC_DONE, .request = request, .value = result, .data = newest_id,
	});
	next_snapshots = k_uptime_get() + 5000;
	LOG_INF("App SYNC queued through event %u", newest_id);
}

static void gather_final(uint8_t tag, uint32_t request, enum bg_app_gather_result result)
{
	struct event done = {
		.time_ms = k_uptime_get(), .request = request,
		.kind = GATHER_RESULT, .tag = tag, .value = result,
	};
	record_event(&done);
	done.id = 0;
	gather_cache[cache_next] = done;
	cache_next = (cache_next + 1) % ARRAY_SIZE(gather_cache);
}

static void gather_request(uint8_t tag, uint32_t request)
{
	for (size_t i = 0; i < ARRAY_SIZE(gather_cache); ++i) {
		if (gather_cache[i].request == request) {
			queue_event(gather_cache[i]);
			return;
		}
	}
	if (request != pending_request &&
	    (pending_request || tag == 0 || tag > BG_MAX_TAGS ||
	     !(bg_roster_known_mask() & BIT(tag)) || bg_radio_gather_for_app(tag, request))) {
		gather_final(tag, request, BG_APP_REJECTED);
		return;
	}

	pending_request = request;
	queue_event((struct event){
		.time_ms = k_uptime_get(), .request = request,
		.kind = GATHER_RESULT, .tag = tag, .value = BG_APP_QUEUED,
	});
}

static ssize_t control_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);
	if (offset) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}
	if (len != CONTROL_SIZE) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	const uint8_t *data = buf;
	uint32_t request = sys_get_le32(data + 4);
	if (data[0] != VERSION || data[1] < SYNC || data[1] > ROLL_CALL ||
	    !request || data[3] != 0 || (data[1] != GATHER && data[2] != 0)) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	if (!bt_gatt_is_subscribed(conn, EVENT_ATTR, BT_GATT_CCC_INDICATE)) {
		return BT_GATT_ERR(BT_ATT_ERR_CCC_IMPROPER_CONF);
	}

	k_mutex_lock(&app_lock, K_FOREVER);
	if (data[1] == SYNC) {
		if (!client) {
			client = bt_conn_ref(conn);
		}
		sync_client(request, sys_get_le32(data + 8), sys_get_le32(data + 12));
	} else if (client != conn) {
		k_mutex_unlock(&app_lock);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	} else if (data[1] == GATHER) {
		gather_request(data[2], request);
	} else {
		for (size_t i = 0; i < ARRAY_SIZE(roll_calls); ++i) {
			if (roll_calls[i] == request) {
				k_mutex_unlock(&app_lock);
				return len;
			}
		}
		if (bg_radio_locate_group()) {
			k_mutex_unlock(&app_lock);
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		roll_calls[roll_call_next] = request;
		roll_call_next = (roll_call_next + 1) % ARRAY_SIZE(roll_calls);
	}
	k_mutex_unlock(&app_lock);
	return len;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(reason);
	k_mutex_lock(&app_lock, K_FOREVER);
	bool was_client = client == conn;
	if (was_client) {
		client = NULL;
		queued = 0;
	}
	k_mutex_unlock(&app_lock);
	if (was_client) {
		bt_conn_unref(conn);
	}
}
BT_CONN_CB_DEFINE(app_connection) = { .disconnected = disconnected };

void bg_app_init(void)
{
	boot_id = sys_rand32_get();
	if (!boot_id) {
		boot_id = 1;
	}
}

void bg_app_poll(void)
{
	k_mutex_lock(&app_lock, K_FOREVER);
	int64_t now = k_uptime_get();
	if (client && now >= next_snapshots) {
		queue_snapshots(0, false);
		next_snapshots = now + 5000;
	}
	k_mutex_unlock(&app_lock);
}

void bg_app_tag_event(uint8_t tag, enum bg_app_tag_state state, int64_t heartbeat_ms)
{
	struct event e = { .time_ms = heartbeat_ms, .kind = TAG_STATE, .tag = tag, .value = state };
	k_mutex_lock(&app_lock, K_FOREVER);
	tag_state[tag] = state == BG_APP_RECOVERED ? BG_APP_ONLINE : state;
	record_event(&e);
	k_mutex_unlock(&app_lock);
}

void bg_app_sos(uint8_t tag)
{
	struct event e = { .time_ms = k_uptime_get(), .kind = SOS, .tag = tag };
	k_mutex_lock(&app_lock, K_FOREVER);
	record_event(&e);
	k_mutex_unlock(&app_lock);
}

void bg_app_gather_done(uint8_t tag, uint32_t request, enum bg_app_gather_result result)
{
	k_mutex_lock(&app_lock, K_FOREVER);
	if (request && pending_request == request) {
		pending_request = 0;
		gather_final(tag, request, result);
	}
	k_mutex_unlock(&app_lock);
}

void bg_app_position(uint8_t tag, int32_t lat_e7, int32_t lon_e7, uint8_t age_s)
{
	struct event e = {
		.time_ms = k_uptime_get(), .value = age_s,
		.request = (uint32_t)lat_e7, .data = (uint32_t)lon_e7,
		.kind = TAG_POSITION, .tag = tag,
	};
	k_mutex_lock(&app_lock, K_FOREVER);
	if (!positions[tag].kind ||
	    e.time_ms - e.value * 1000 >= positions[tag].time_ms - positions[tag].value * 1000) {
		positions[tag] = e;
		queue_event(e);
	}
	k_mutex_unlock(&app_lock);
}
