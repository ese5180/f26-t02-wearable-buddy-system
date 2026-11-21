/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include "l76k.h"

static const struct device *const uart = DEVICE_DT_GET(DT_ALIAS(buddyguard_gnss));
K_MSGQ_DEFINE(sentences, sizeof(struct bg_l76k_sentence), 8, 1);
static struct bg_l76k_sentence pending;
static size_t length;
static atomic_t bytes_received;
static atomic_t invalid_sentences;
static atomic_t dropped_sentences;

static bool checksum_matches(void)
{
	if (length < 10 || pending.text[length - 3] != '*') {
		return false;
	}

	uint8_t high, low, checksum = 0;
	if (char2hex(pending.text[length - 2], &high) ||
	    char2hex(pending.text[length - 1], &low)) {
		return false;
	}
	for (size_t index = 1; index < length - 3; ++index) {
		checksum ^= (uint8_t)pending.text[index];
	}
	return checksum == ((high << 4) | low);
}

static void receive_byte(uint8_t byte)
{
	/* A new start marker also resynchronizes after a truncated sentence. */
	if (byte == '$') {
		pending.text[0] = '$';
		length = 1;
		return;
	}
	if (length == 0 || byte == '\r') {
		return;
	}
	if (byte == '\n') {
		pending.text[length] = '\0';
		if (!checksum_matches()) {
			atomic_inc(&invalid_sentences);
		} else if (k_msgq_put(&sentences, &pending, K_NO_WAIT) != 0) {
			atomic_inc(&dropped_sentences);
		}
		length = 0;
		return;
	}
	if (byte < ' ' || byte > '~' || length == sizeof(pending.text) - 1) {
		atomic_inc(&invalid_sentences);
		length = 0;
		return;
	}
	pending.text[length++] = (char)byte;
}

static void uart_interrupt(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uart_irq_update(dev);
	if (!uart_irq_rx_ready(dev)) {
		return;
	}
	if (uart_err_check(dev) > 0) {
		length = 0;
	}

	uint8_t buffer[32];
	int received;
	do {
		received = uart_fifo_read(dev, buffer, sizeof(buffer));
		if (received > 0) {
			atomic_add(&bytes_received, received);
			for (int index = 0; index < received; ++index) {
				receive_byte(buffer[index]);
			}
		}
	} while (received == sizeof(buffer));
}

int bg_l76k_start(void)
{
	if (!device_is_ready(uart)) {
		return -ENODEV;
	}
	int rc = uart_irq_callback_user_data_set(uart, uart_interrupt, NULL);
	if (rc == 0) {
		uart_irq_rx_enable(uart);
	}
	return rc;
}

int bg_l76k_receive(struct bg_l76k_sentence *sentence, k_timeout_t timeout)
{
	return k_msgq_get(&sentences, sentence, timeout);
}

struct bg_l76k_stats bg_l76k_stats(void)
{
	return (struct bg_l76k_stats){
		.bytes = (uint32_t)atomic_get(&bytes_received),
		.invalid_sentences = (uint32_t)atomic_get(&invalid_sentences),
		.dropped_sentences = (uint32_t)atomic_get(&dropped_sentences),
	};
}
