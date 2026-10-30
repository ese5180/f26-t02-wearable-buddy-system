/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <sx126x/sx126x.h>
#include "radio_driver.h"
#include "wire.h"
LOG_MODULE_REGISTER(bg_wio, LOG_LEVEL_INF);
#define FRAME_SIZE BG_RADIO_FRAME_SIZE
static const struct device *const radio = DEVICE_DT_GET(DT_ALIAS(lora0));
static bool tx_mode;
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

int bg_radio_driver_send(uint8_t *frame)
{
	int rc = set_mode(true);
	if (rc == 0) {
		rc = lora_send(radio, frame, FRAME_SIZE);
	}
	int rx_rc = set_mode(false);
	return rc ? rc : rx_rc;
}

int bg_radio_driver_receive(uint8_t *frame, k_timeout_t timeout, int16_t *rssi, int8_t *snr)
{
	int rc = set_mode(false);
	if (rc) {
		return rc;
	}
	return lora_recv(radio, frame, FRAME_SIZE, timeout, rssi, snr);
}

int bg_radio_driver_init(void)
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
	}
	return rc;
}
