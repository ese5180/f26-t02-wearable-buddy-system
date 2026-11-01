/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_WIRE_H
#define BUDDYGUARD_WIRE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { BG_PRESENCE_SIZE = 8, BG_RADIO_FRAME_SIZE = 12, BG_MAX_TAGS = 3 };
enum bg_radio_type {
	BG_RADIO_STATUS = 1, BG_RADIO_ACK, BG_RADIO_SOS, BG_RADIO_QUERY, BG_RADIO_REPLY,
};
struct bg_presence_frame { uint8_t tag, flags; uint16_t sequence; };
void bg_presence_encode(uint8_t out[BG_PRESENCE_SIZE], uint8_t tag,
						uint16_t sequence, uint8_t flags, uint8_t role);
/* Roster accepts only laboratory tag IDs 1..3, role TAG, and defined flags. */
bool bg_presence_decode(const uint8_t *data, size_t size, struct bg_presence_frame *out);
void bg_radio_frame_encode(uint8_t out[BG_RADIO_FRAME_SIZE], uint8_t type,
						  uint8_t tag, uint16_t sequence);
bool bg_radio_frame_valid(const uint8_t *data, size_t size);
#endif
