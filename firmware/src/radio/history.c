/* SPDX-License-Identifier: Apache-2.0 */
/* Bounded recent-event cache; suppresses repeated reports, not malicious replay.
 * 有限近期事件缓存；避免重复报告，不提供安全防重放。
 */
#include "radio_history.h"

bool bg_radio_history_seen(struct bg_radio_history *h,
	const uint8_t frame[BG_RADIO_FRAME_SIZE])
{
	uint16_t seq = (uint16_t)(frame[6] | ((uint16_t)frame[7] << 8));
	for (unsigned i = 0; i < BG_RADIO_HISTORY_SIZE; ++i) {
		if (h->entries[i].valid && h->entries[i].tag == frame[4] &&
			h->entries[i].type == frame[3] && h->entries[i].sequence == seq) {
			return true;
		}
	}
	h->entries[h->next].tag = frame[4];
	h->entries[h->next].type = frame[3];
	h->entries[h->next].sequence = seq;
	h->entries[h->next].valid = true;
	h->next = (h->next + 1) % BG_RADIO_HISTORY_SIZE;
	return false;
}
