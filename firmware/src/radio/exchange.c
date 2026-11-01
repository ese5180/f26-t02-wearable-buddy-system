/* SPDX-License-Identifier: Apache-2.0 */
/* One pure pending transaction: deadline, bounded retry and response matching.
 * 只管理一个待完成事务的期限、有限重试及应答匹配，不执行收发。
 */
#include <string.h>
#include "radio_exchange.h"

void bg_radio_exchange_start(struct bg_radio_exchange *e,
	const uint8_t frame[BG_RADIO_FRAME_SIZE], uint32_t now,
	uint32_t timeout, uint32_t backoff)
{
	*e = (struct bg_radio_exchange) {
		.active = true, .deadline_ms = now,
		.timeout_ms = timeout, .backoff_ms = backoff,
	};
	memcpy(e->frame, frame, sizeof(e->frame));
}

enum bg_radio_exchange_action bg_radio_exchange_next(struct bg_radio_exchange *e,
	uint32_t now)
{
	/* Subtract before casting so a uint32_t clock wrap preserves short deadlines.
	 * 先做无符号差再转有符号数，避免毫秒计数回绕破坏短期限比较。 */
	if (!e->active || (int32_t)(now - e->deadline_ms) < 0) {
		return BG_EXCHANGE_WAIT;
	}
	if (e->awaiting) {
		if (e->attempts >= 3) {
			e->active = false;
			return BG_EXCHANGE_EXHAUSTED;
		}
		e->awaiting = false;
		e->deadline_ms = now + e->backoff_ms;
		return BG_EXCHANGE_WAIT;
	}
	return BG_EXCHANGE_SEND;
}

void bg_radio_exchange_sent(struct bg_radio_exchange *e, uint32_t now)
{
	++e->attempts;
	e->awaiting = true;
	e->deadline_ms = now + e->timeout_ms;
}

bool bg_radio_exchange_accept(struct bg_radio_exchange *e,
	const uint8_t *frame, size_t size)
{
	if (!e->active || !e->attempts || !bg_radio_frame_valid(frame, size) ||
		frame[4] != e->frame[4] || frame[6] != e->frame[6] ||
		frame[7] != e->frame[7]) {
		return false;
	}
	uint8_t expected = e->frame[3] == BG_RADIO_QUERY ? BG_RADIO_REPLY : BG_RADIO_ACK;
	if (frame[3] != expected) {
		return false;
	}
	/* Byte5 was reserved. New ACKs identify the acknowledged type; legacy
	 * zero ACKs remain accepted for rolling upgrades (weaker ambiguity check). */
	if (expected == BG_RADIO_ACK && frame[5] && frame[5] != e->frame[3]) {
		return false;
	}
	e->active = false;
	return true;
}
