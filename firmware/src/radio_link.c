/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <sx126x/sx126x.h>
#include "radio_link.h"

LOG_MODULE_REGISTER(bg_lora, LOG_LEVEL_INF);

enum { STATUS = 1, ACK = 2, SOS = 3, QUERY = 4, REPLY = 5 };
enum { FRAME_SIZE = 12 };
struct request { uint8_t type; uint8_t tag; };
K_MSGQ_DEFINE(requests, sizeof(struct request), 8, 1);
K_SEM_DEFINE(radio_started, 0, 1);
static const struct device *const radio = DEVICE_DT_GET(DT_ALIAS(lora0));
static uint16_t next_sequence;
static bool tx_mode;
static uint16_t last_sequence[4];
static uint8_t last_type[4];

static int configure(bool tx)
{
	struct lora_modem_config cfg = {
		.frequency = 915000000, .bandwidth = BW_125_KHZ, .datarate = SF_7,
		.preamble_len = 8, .coding_rate = CR_4_5, .iq_inverted = false,
		.public_network = false, .tx_power = 4, .tx = tx,
	};
	int rc = lora_config(radio, &cfg);
	if (rc == 0) {
		/* Wio-SX1262 Module Datasheet V1.1 specifies the DC-DC design.
		 * Keep this explicit: some board BSP examples select LDO instead.
		 */
		SX126xSetRegulatorMode(USE_DCDC);
		tx_mode = tx;
	}
	return rc;
}

static int set_mode(bool tx)
{
	return tx_mode == tx ? 0 : configure(tx);
}

static void make_frame(uint8_t *frame, uint8_t type, uint8_t tag, uint16_t seq)
{
	memset(frame, 0, FRAME_SIZE);
	frame[0] = 'B'; frame[1] = 'G'; frame[2] = 1;
	frame[3] = type; frame[4] = tag;
	sys_put_le16(seq, frame + 6);
	/* Motion and clasp UNKNOWN; battery is unavailable, not a fabricated value. */
	sys_put_le16(UINT16_MAX, frame + 10);
}

static bool valid(const uint8_t *frame, int size)
{
	return size == FRAME_SIZE && frame[0] == 'B' && frame[1] == 'G' &&
		frame[2] == 1 && frame[3] >= STATUS && frame[3] <= REPLY &&
		frame[4] > 0 && frame[4] <= 3;
}

static int transmit(uint8_t *frame)
{
	int rc = set_mode(true);
	if (rc == 0) {
		rc = lora_send(radio, frame, FRAME_SIZE);
	}
	int rx_rc = set_mode(false);
	return rc ? rc : rx_rc;
}

static int receive(uint8_t *frame, k_timeout_t timeout, int16_t *rssi, int8_t *snr)
{
	int rc = set_mode(false);
	if (rc) {
		return rc;
	}
	return lora_recv(radio, frame, FRAME_SIZE, timeout, rssi, snr);
}

static void send_with_ack(uint8_t *frame)
{
	uint16_t seq = sys_get_le16(frame + 6);
	for (int attempt = 1; attempt <= 3; ++attempt) {
		LOG_INF("TX tag=T%02u type=%u seq=%u attempt=%d",
			frame[4], frame[3], seq, attempt);
		int rc = transmit(frame);
		if (rc) {
			LOG_ERR("LoRa TX failed rc=%d", rc);
			return;
		}
		uint8_t reply[FRAME_SIZE];
		int16_t rssi;
		int8_t snr;
		int size = receive(reply, K_MSEC(600), &rssi, &snr);
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
				(void)transmit(frame);
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
		int size = receive(frame, K_MSEC(200), &rssi, &snr);
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
			(void)transmit(frame); /* Duplicate events are ACKed again. */
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
	/* Bring up the radio only after main has armed the watchdog. A driver
	 * waiting on BUSY must not strand an unconfirmed image before main.
	 */
	int init_rc = device_init(radio);
	if (init_rc != 0 && init_rc != -EALREADY) {
		LOG_ERR("Wio-SX1262 initialization failed: %d", init_rc);
		return init_rc;
	}
	if (!device_is_ready(radio)) {
		LOG_ERR("Wio-SX1262 device not ready");
		return -ENODEV;
	}
	int rc = configure(false);
	if (rc == 0) {
		uint8_t sync_bytes[2];
		SX126xReadRegisters(0x0740, sync_bytes, sizeof(sync_bytes));
		uint16_t sync_word = sys_get_be16(sync_bytes);
		LOG_INF("SX1262 SPI readback: private sync word=0x%04x (expected 0x1424)",
			sync_word);
		if (sync_word != 0x1424) {
			LOG_ERR("Radio readback failed; inspect B2B connection and SPI wiring");
			return -EIO;
		}
		LOG_INF("Wio-SX1262 ready: 915 MHz SF7 BW125 CR4/5 power=4 dBm role=%s",
			IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR) ? "COUNSELOR" : "TAG");
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
