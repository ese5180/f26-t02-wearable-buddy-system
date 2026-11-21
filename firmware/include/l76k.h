/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_L76K_H
#define BUDDYGUARD_L76K_H

#include <stdint.h>
#include <zephyr/kernel.h>

struct bg_l76k_sentence {
	char text[96];
};

struct bg_l76k_stats {
	uint32_t bytes;
	uint32_t invalid_sentences;
	uint32_t dropped_sentences;
};

/* The driver owns UART0 and delivers checksum-valid NMEA to one consumer. */
int bg_l76k_start(void);
int bg_l76k_receive(struct bg_l76k_sentence *sentence, k_timeout_t timeout);
struct bg_l76k_stats bg_l76k_stats(void);

#endif
