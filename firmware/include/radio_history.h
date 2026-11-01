/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_RADIO_HISTORY_H
#define BUDDYGUARD_RADIO_HISTORY_H
#include "wire.h"
enum { BG_RADIO_HISTORY_SIZE = 16 };
struct bg_radio_history {
	struct { uint16_t sequence; uint8_t type, tag; bool valid; } entries[BG_RADIO_HISTORY_SIZE];
	uint8_t next;
};
/* Validated business frames only. True means already reported; ACK it again.
 * Bounded recent history is duplicate suppression, NOT replay protection.
 * 只输入已校验的业务帧；重复消息仍需 ACK，此缓存不是安全防重放。 */
bool bg_radio_history_seen(struct bg_radio_history *history,
	const uint8_t frame[BG_RADIO_FRAME_SIZE]);
#endif
