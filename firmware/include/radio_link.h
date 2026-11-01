#ifndef BUDDYGUARD_RADIO_LINK_H
#define BUDDYGUARD_RADIO_LINK_H
#include <stdint.h>
int bg_radio_init(void);
int bg_radio_sos(void);
int bg_radio_query(uint8_t tag);
/* Approximate monotonic counters since boot, copied atomically per field. */
struct bg_radio_stats {
	uint32_t tx_attempts, tx_completed, tx_exhausted;
	uint32_t rx_valid, rx_interleaved, rx_duplicates;
	uint32_t queries_received, replies_received, queue_drops;
};
struct bg_radio_stats bg_radio_get_stats(void);
#endif
