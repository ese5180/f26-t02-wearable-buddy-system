/* SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include "radio_link.h"
#include "roster.h"
#include "wire.h"

LOG_MODULE_REGISTER(bg_user_button, LOG_LEVEL_INF);

static void user_longpress(struct input_event *event, void *user_data)
{
	ARG_UNUSED(user_data);
	if (event->type != INPUT_EV_KEY || event->code != INPUT_BTN_1 || !event->value) {
		return;
	}
	if (!IS_ENABLED(CONFIG_BUDDYGUARD_COUNSELOR)) {
		const int rc = bg_radio_sos();
		LOG_INF("USER SOS queue result=%d", rc);
		return;
	}

	/* Send a unicast command to each tag seen this session, including tags
	 * whose recent BLE heartbeats are missing but may still hear LoRa. */
	const uint32_t registered = bg_roster_known_mask();
	if (registered == 0) {
		LOG_INF("USER GATHER skipped: no tags registered this session");
		return;
	}
	for (uint8_t tag = 1; tag <= BG_MAX_TAGS; ++tag) {
		if (registered & BIT(tag)) {
			const int rc = bg_radio_gather(tag);
			LOG_INF("USER GATHER queue T%02u result=%d", tag, rc);
		}
	}
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(wio_longpress)), user_longpress, NULL);
