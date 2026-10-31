/* SPDX-License-Identifier: Apache-2.0 */
#include <string.h>
#include "wire.h"
void bg_presence_encode(uint8_t out[BG_PRESENCE_SIZE], uint8_t tag,
						uint16_t sequence, uint8_t flags, uint8_t role)
{
	const uint8_t frame[] = {0xff, 0xff, 1, tag, (uint8_t)sequence,
		(uint8_t)(sequence >> 8), flags, role};
	memcpy(out, frame, sizeof(frame));
}

bool bg_presence_decode(const uint8_t *data, size_t size, struct bg_presence_frame *out)
{
	if (!data || !out || size != BG_PRESENCE_SIZE || data[0] != 0xff ||
		data[1] != 0xff || data[2] != 1 || data[7] != 1 ||
		data[3] == 0 || data[3] > BG_MAX_TAGS || (data[6] & ~3)) {
		return false;
	}
	*out = (struct bg_presence_frame) {.tag = data[3], .flags = data[6],
		.sequence = (uint16_t)(data[4] | ((uint16_t)data[5] << 8))};
	return true;
}

void bg_radio_frame_encode(uint8_t out[BG_RADIO_FRAME_SIZE], uint8_t type,
						  uint8_t tag, uint16_t sequence)
{
	/* Default motion/clasp UNKNOWN; link fills fresh optional motion. Battery unavailable. */
	const uint8_t frame[] = {'B', 'G', 1, type, tag, 0, (uint8_t)sequence,
		(uint8_t)(sequence >> 8), 0, 0, 0xff, 0xff};
	memcpy(out, frame, sizeof(frame));
}

bool bg_radio_frame_valid(const uint8_t *data, size_t size)
{
	return data && size == BG_RADIO_FRAME_SIZE && data[0] == 'B' &&
		data[1] == 'G' && data[2] == 1 && data[3] >= 1 && data[3] <= 5 &&
		data[4] > 0 && data[4] <= BG_MAX_TAGS;
}
