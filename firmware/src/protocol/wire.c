/* SPDX-License-Identifier: Apache-2.0 */
/* Pure byte encoding/validation; no radio hardware, scheduling or application state.
 * 只做字节编码与校验，不操作无线硬件、不调度、不持有业务状态。
 */
#include <string.h>
#include <zephyr/sys/byteorder.h>
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
		data[1] == 'G' && data[2] == 1 &&
		((data[3] >= BG_RADIO_STATUS && data[3] <= BG_RADIO_GATHER) ||
		 data[3] == BG_RADIO_LOCATE) &&
		data[4] > 0 && data[4] <= BG_MAX_TAGS;
}

void bg_position_encode(uint8_t out[BG_POSITION_FRAME_SIZE], const struct bg_position_frame *p)
{
	const uint8_t header[] = {'B', 'G', 1, BG_RADIO_POSITION, p->tag, p->age_s};
	memcpy(out, header, sizeof(header));
	sys_put_le16(p->sequence, out + 6);
	sys_put_le32((uint32_t)p->latitude_e7, out + 8);
	sys_put_le32((uint32_t)p->longitude_e7, out + 12);
}

bool bg_position_decode(const uint8_t *data, size_t size, struct bg_position_frame *p)
{
	if (!data || !p || size != BG_POSITION_FRAME_SIZE || data[0] != 'B' || data[1] != 'G' ||
	    data[2] != 1 || data[3] != BG_RADIO_POSITION || data[4] == 0 ||
	    data[4] > BG_MAX_TAGS) {
		return false;
	}
	int32_t lat = (int32_t)sys_get_le32(data + 8);
	int32_t lon = (int32_t)sys_get_le32(data + 12);
	if (lat < -900000000 || lat > 900000000 || lon < -1800000000 || lon > 1800000000) {
		return false;
	}
	*p = (struct bg_position_frame){
		.latitude_e7 = lat, .longitude_e7 = lon, .sequence = sys_get_le16(data + 6),
		.tag = data[4], .age_s = data[5],
	};
	return true;
}
