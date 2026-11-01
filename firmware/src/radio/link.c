/* SPDX-License-Identifier: Apache-2.0 */
/* Single runtime radio owner: queue, scheduler and packet dispatch.
 * 唯一运行期无线硬件使用者；重试和去重策略分别交给 exchange/history。 */
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include "radio_link.h"
#include "radio_driver.h"
#include "radio_exchange.h"
#include "radio_history.h"
#include "imu.h"

LOG_MODULE_REGISTER(bg_lora, LOG_LEVEL_INF);

struct request { uint8_t type; uint8_t tag; };
K_MSGQ_DEFINE(requests, sizeof(struct request), 8, 1);
K_SEM_DEFINE(radio_started, 0, 1);
static uint16_t next_sequence;
static struct bg_radio_exchange exchange;
static struct bg_radio_history history;
static bool reply_pending;
static uint16_t reply_sequence;

enum counter {
	TX_ATTEMPTS, TX_COMPLETED, TX_EXHAUSTED, RX_VALID, RX_INTERLEAVED,
	RX_DUPLICATES, QUERIES_RECEIVED, REPLIES_RECEIVED, QUEUE_DROPS, COUNTER_COUNT,
};
static atomic_t counters[COUNTER_COUNT];

static void make_frame(uint8_t *frame, uint8_t type, uint8_t tag, uint16_t seq)
{
	bg_radio_frame_encode(frame, type, tag, seq);
	frame[8] = bg_imu_motion();
}

static void begin(uint8_t *frame, uint32_t now)
{
	uint16_t seq = sys_get_le16(frame + 6);
	/* QUERY expects a REPLY, not an ACK. Deterministic tag/sequence jitter
	 * separates retries without blocking reception or fabricating randomness. */
	bg_radio_exchange_start(&exchange, frame, now,
		frame[3] == BG_RADIO_QUERY ? 1500 : 600,
		80 + frame[4] * 17 + (seq % 7) * 25);
}

static void transmit_due(uint32_t now)
{
	enum bg_radio_exchange_action action = bg_radio_exchange_next(&exchange, now);
	if (action == BG_EXCHANGE_WAIT) {
		return;
	}
	uint8_t *frame = exchange.frame;
	uint16_t seq = sys_get_le16(frame + 6);
	if (action == BG_EXCHANGE_EXHAUSTED) {
		atomic_inc(&counters[TX_EXHAUSTED]);
		LOG_WRN("Delivery unconfirmed tag=T%02u type=%u seq=%u after %u attempts",
			frame[4], frame[3], seq, exchange.attempts);
		return;
	}
	LOG_INF("TX tag=T%02u type=%u seq=%u attempt=%u",
		frame[4], frame[3], seq, exchange.attempts + 1);
	atomic_inc(&counters[TX_ATTEMPTS]);
	int rc = bg_radio_driver_send(frame);
	bg_radio_exchange_sent(&exchange, (uint32_t)k_uptime_get());
	if (rc) {
		LOG_WRN("LoRa TX error rc=%d; bounded retry remains scheduled", rc);
	}
}

static void acknowledge(uint8_t *frame)
{
	/* Protocol1's reserved byte5 now optionally identifies the original type.
	 * Old peers ignore it; new peers still accept legacy zero-type ACKs. */
	frame[5] = frame[3];
	frame[3] = BG_RADIO_ACK;
	int rc = bg_radio_driver_send(frame);
	if (rc) {
		LOG_WRN("ACK send failed rc=%d; peer may retransmit", rc);
	}
}

static void dispatch(uint8_t *frame, int16_t rssi, int8_t snr)
{
	uint8_t type = frame[3];
	uint8_t tag = frame[4];
	uint16_t seq = sys_get_le16(frame + 6);
	atomic_inc(&counters[RX_VALID]);
	bool was_active = exchange.active;
	if (bg_radio_exchange_accept(&exchange, frame, BG_RADIO_FRAME_SIZE)) {
		atomic_inc(&counters[TX_COMPLETED]);
		LOG_INF("%s tag=T%02u type=%u seq=%u attempt=%u rssi=%d snr=%d",
			exchange.frame[3] == BG_RADIO_QUERY ? "QUERY complete" : "ACK",
			tag, exchange.frame[3], seq, exchange.attempts, rssi, snr);
	} else if (was_active && type != BG_RADIO_ACK) {
		atomic_inc(&counters[RX_INTERLEAVED]);
		LOG_INF("Dispatch while waiting tag=T%02u type=%u seq=%u", tag, type, seq);
	}

	if (IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) &&
		(type == BG_RADIO_STATUS || type == BG_RADIO_SOS || type == BG_RADIO_REPLY)) {
		if (type == BG_RADIO_REPLY) {
			atomic_inc(&counters[REPLIES_RECEIVED]);
		}
		bool duplicate = bg_radio_history_seen(&history, frame);
		if (!duplicate) {
			LOG_INF("RX tag=T%02u type=%u seq=%u rssi=%d snr=%d motion=%s battery=unavailable",
				tag, type, seq, rssi, snr, bg_motion_name(frame[8]));
			if (type == BG_RADIO_SOS) {
				LOG_WRN("ACTION: counselor check tag T%02u, SOS received", tag);
			}
		} else {
			atomic_inc(&counters[RX_DUPLICATES]);
		}
		acknowledge(frame); /* Also ACK duplicates, even while another query waits. */
	} else if (!IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) &&
		type == BG_RADIO_QUERY && tag == CONFIG_BUDDYGUARD_TAG_ID) {
		atomic_inc(&counters[QUERIES_RECEIVED]);
		LOG_INF("Received counselor QUERY seq=%u", seq);
		/* Keep one deferred response while an existing STATUS/SOS finishes.
		 * Duplicate QUERY retries reuse the sequence and coalesce with REPLY. */
		if (!(exchange.active && exchange.frame[3] == BG_RADIO_REPLY &&
			sys_get_le16(exchange.frame + 6) == seq)) {
			if (reply_pending && reply_sequence != seq) {
				LOG_WRN("Replacing deferred QUERY reply seq=%u with seq=%u",
					reply_sequence, seq);
			}
			reply_pending = true;
			reply_sequence = seq;
		}
	}
}

static void worker(void *a, void *b, void *c)
{
	ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
	k_sem_take(&radio_started, K_FOREVER);
	uint32_t next_status = (uint32_t)k_uptime_get() + 3000;
	for (;;) {
		uint32_t now = (uint32_t)k_uptime_get();
		uint8_t frame[BG_RADIO_FRAME_SIZE];
		struct request req;
		if (!exchange.active) {
			if (k_msgq_get(&requests, &req, K_NO_WAIT) == 0) {
				make_frame(frame, req.type, req.tag, ++next_sequence);
				begin(frame, now);
			} else if (reply_pending) {
				make_frame(frame, BG_RADIO_REPLY, CONFIG_BUDDYGUARD_TAG_ID, reply_sequence);
				reply_pending = false;
				begin(frame, now);
			} else if (!IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) &&
				(int32_t)(now - next_status) >= 0) {
				make_frame(frame, BG_RADIO_STATUS, CONFIG_BUDDYGUARD_TAG_ID, ++next_sequence);
				begin(frame, now);
				next_status = now + 10000;
			}
		}
		transmit_due(now);
		/* No nested ACK receive loop: EVERY valid frame reaches the dispatcher. */
		int16_t rssi;
		int8_t snr;
		/* Keep the established 200 ms RX window: this synchronous driver
		 * sleeps the radio on timeout, so very short windows can cut a packet. */
		int size = bg_radio_driver_receive(frame, K_MSEC(200), &rssi, &snr);
		if (size >= 0 && bg_radio_frame_valid(frame, (size_t)size)) {
			dispatch(frame, rssi, snr);
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

static int enqueue(uint8_t type, uint8_t tag)
{
	struct request req = {.type = type, .tag = tag};
	int rc = k_msgq_put(&requests, &req, K_NO_WAIT);
	if (rc) {
		atomic_inc(&counters[QUEUE_DROPS]);
		LOG_WRN("Radio command queue full: type=%u tag=T%02u", type, tag);
	}
	return rc;
}

int bg_radio_sos(void)
{
	return IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? -EINVAL :
		enqueue(BG_RADIO_SOS, CONFIG_BUDDYGUARD_TAG_ID);
}

int bg_radio_query(uint8_t tag)
{
	if (!IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) || tag == 0 || tag > BG_MAX_TAGS) {
		return -EINVAL;
	}
	return enqueue(BG_RADIO_QUERY, tag);
}

struct bg_radio_stats bg_radio_get_stats(void)
{
	return (struct bg_radio_stats) {
		.tx_attempts = atomic_get(&counters[TX_ATTEMPTS]),
		.tx_completed = atomic_get(&counters[TX_COMPLETED]),
		.tx_exhausted = atomic_get(&counters[TX_EXHAUSTED]),
		.rx_valid = atomic_get(&counters[RX_VALID]),
		.rx_interleaved = atomic_get(&counters[RX_INTERLEAVED]),
		.rx_duplicates = atomic_get(&counters[RX_DUPLICATES]),
		.queries_received = atomic_get(&counters[QUERIES_RECEIVED]),
		.replies_received = atomic_get(&counters[REPLIES_RECEIVED]),
		.queue_drops = atomic_get(&counters[QUEUE_DROPS]),
	};
}
