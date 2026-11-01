/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_RADIO_EXCHANGE_H
#define BUDDYGUARD_RADIO_EXCHANGE_H
#include "wire.h"

enum bg_radio_exchange_action {
	BG_EXCHANGE_WAIT, BG_EXCHANGE_SEND, BG_EXCHANGE_EXHAUSTED,
};
/* Pure retry state: no hardware, logging, sleeping or Zephyr dependencies.
 * One in-flight transaction; callers continue dispatching RX while it waits.
 * All durations must be below INT32_MAX ms for wrap-safe comparisons.
 * 调用者持有状态并执行收发；时间间隔须小于 INT32_MAX 毫秒以支持计时回绕。 */
struct bg_radio_exchange {
	uint8_t frame[BG_RADIO_FRAME_SIZE];
	uint32_t deadline_ms, timeout_ms, backoff_ms;
	uint8_t attempts;
	bool active, awaiting;
};
void bg_radio_exchange_start(struct bg_radio_exchange *exchange,
	const uint8_t frame[BG_RADIO_FRAME_SIZE], uint32_t now_ms,
	uint32_t timeout_ms, uint32_t backoff_ms);
enum bg_radio_exchange_action bg_radio_exchange_next(
	struct bg_radio_exchange *exchange, uint32_t now_ms);
/* Call after each send attempt, including a driver failure; retries are bounded. */
void bg_radio_exchange_sent(struct bg_radio_exchange *exchange, uint32_t now_ms);
bool bg_radio_exchange_accept(struct bg_radio_exchange *exchange,
	const uint8_t *frame, size_t size);
#endif
