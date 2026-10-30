/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include "radio_link.h"
#include "radio_driver.h"
#include "wire.h"

LOG_MODULE_REGISTER(bg_lora, LOG_LEVEL_INF);

enum { STATUS = 1, ACK = 2, SOS = 3, QUERY = 4, REPLY = 5 };
enum { FRAME_SIZE = 12 };
struct request { uint8_t type; uint8_t tag; };
K_MSGQ_DEFINE(requests, sizeof(struct request), 8, 1);
K_SEM_DEFINE(radio_started, 0, 1);
static uint16_t next_sequence;
static uint16_t last_sequence[4];
static uint8_t last_type[4];

static void make_frame(uint8_t *frame, uint8_t type, uint8_t tag, uint16_t seq)
{
	bg_radio_frame_encode(frame, type, tag, seq);
}

static bool valid(const uint8_t *frame, int size)
{
	return size >= 0 && bg_radio_frame_valid(frame, (size_t)size);
}

static void send_with_ack(uint8_t *frame)
{
	uint16_t seq = sys_get_le16(frame + 6);
	for (int attempt = 1; attempt <= 3; ++attempt) {
		LOG_INF("TX tag=T%02u type=%u seq=%u attempt=%d",
			frame[4], frame[3], seq, attempt);
		int rc = bg_radio_driver_send(frame);
		if (rc) {
			LOG_ERR("LoRa TX failed rc=%d", rc);
			return;
		}
		uint8_t reply[FRAME_SIZE];
		int16_t rssi;
		int8_t snr;
		int size = bg_radio_driver_receive(reply, K_MSEC(600), &rssi, &snr);
		if (valid(reply, size) && reply[3] == ACK && reply[4] == frame[4] &&
			sys_get_le16(reply + 6) == seq) {
			LOG_INF("ACK tag=T%02u type=%u seq=%u attempt=%d rssi=%d snr=%d",
				frame[4], frame[3], seq, attempt, rssi, snr);
			return;
		}
		k_sleep(K_MSEC(100 + (seq % 7) * 20));
	}
	LOG_WRN("Delivery unconfirmed tag=T%02u type=%u seq=%u after 3 attempts",
		frame[4], frame[3], seq);
}

static void worker(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	k_sem_take(&radio_started, K_FOREVER);
	int64_t next_status = k_uptime_get() + 3000;
	for (;;) {
		struct request req;
		uint8_t frame[FRAME_SIZE];
		if (k_msgq_get(&requests, &req, K_NO_WAIT) == 0) {
			make_frame(frame, req.type, req.tag, ++next_sequence);
			if (req.type == QUERY) {
				LOG_INF("QUERY tag=T%02u seq=%u", req.tag, next_sequence);
				(void)bg_radio_driver_send(frame);
			} else {
				send_with_ack(frame);
			}
		}
		if (!IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) && k_uptime_get() >= next_status) {
			make_frame(frame, STATUS, CONFIG_BUDDYGUARD_TAG_ID, ++next_sequence);
			send_with_ack(frame);
			next_status = k_uptime_get() + 10000;
		}
		int16_t rssi;
		int8_t snr;
		int size = bg_radio_driver_receive(frame, K_MSEC(200), &rssi, &snr);
		if (!valid(frame, size)) {
			continue;
		}
		uint8_t type = frame[3];
		uint8_t tag = frame[4];
		uint16_t seq = sys_get_le16(frame + 6);
		if (IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) &&
			(type == STATUS || type == SOS || type == REPLY)) {
			bool duplicate = last_sequence[tag] == seq && last_type[tag] == type;
			if (!duplicate) {
				last_sequence[tag] = seq;
				last_type[tag] = type;
				LOG_INF("RX tag=T%02u type=%u seq=%u rssi=%d snr=%d motion=UNKNOWN battery=unavailable",
					tag, type, seq, rssi, snr);
				if (type == SOS) {
					LOG_WRN("ACTION: counselor check tag T%02u, SOS received", tag);
				}
			}
			frame[3] = ACK;
			(void)bg_radio_driver_send(frame); /* Duplicate events are ACKed again. */
		} else if (!IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) && type == QUERY &&
			tag == CONFIG_BUDDYGUARD_TAG_ID) {
			LOG_INF("Received counselor QUERY seq=%u", seq);
			make_frame(frame, REPLY, tag, seq);
			send_with_ack(frame);
		}
	}
}

K_THREAD_DEFINE(bg_lora_thread, 3072, worker, NULL, NULL, NULL, 5, 0, 0);

int bg_radio_init(void)
{
	int rc = bg_radio_driver_init();
	if (rc == 0) {
		k_sem_give(&radio_started);
	}
	return rc;
}

int bg_radio_sos(void)
{
	if (IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR)) {
		return -EINVAL;
	}
	struct request req = {.type = SOS, .tag = CONFIG_BUDDYGUARD_TAG_ID};
	return k_msgq_put(&requests, &req, K_NO_WAIT);
}

int bg_radio_query(uint8_t tag)
{
	if (!IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) || tag == 0 || tag > 3) {
		return -EINVAL;
	}
	struct request req = {.type = QUERY, .tag = tag};
	return k_msgq_put(&requests, &req, K_NO_WAIT);
}
