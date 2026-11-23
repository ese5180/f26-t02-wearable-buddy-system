/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_APP_BLE_H
#define BUDDYGUARD_APP_BLE_H

#include <stdint.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/util.h>

#define BG_APP_SERVICE_UUID BT_UUID_128_ENCODE(0x39c2ae25, 0xc509, 0x4902, 0xbad6, 0xfa7d52cbbb86)

enum bg_app_tag_state { BG_APP_NOT_SEEN, BG_APP_ONLINE, BG_APP_MISSING, BG_APP_RECOVERED };
enum bg_app_gather_result { BG_APP_QUEUED, BG_APP_STARTED, BG_APP_NO_ACK, BG_APP_REJECTED };

#if defined(CONFIG_BUDDYGUARD_APP_BLE)
void bg_app_init(void);
void bg_app_poll(void);
void bg_app_tag_event(uint8_t tag, enum bg_app_tag_state state, int64_t heartbeat_ms);
void bg_app_sos(uint8_t tag);
void bg_app_gather_done(uint8_t tag, uint32_t request, enum bg_app_gather_result result);
void bg_app_position(uint8_t tag, int32_t lat_e7, int32_t lon_e7, uint8_t age_s);
#else
static inline void bg_app_init(void) {}
static inline void bg_app_poll(void) {}
static inline void bg_app_tag_event(uint8_t tag, enum bg_app_tag_state state, int64_t heartbeat_ms)
{
	ARG_UNUSED(tag);
	ARG_UNUSED(state);
	ARG_UNUSED(heartbeat_ms);
}
static inline void bg_app_sos(uint8_t tag) { ARG_UNUSED(tag); }
static inline void bg_app_gather_done(uint8_t tag, uint32_t request, enum bg_app_gather_result result)
{
	ARG_UNUSED(tag);
	ARG_UNUSED(request);
	ARG_UNUSED(result);
}
static inline void bg_app_position(uint8_t tag, int32_t lat_e7, int32_t lon_e7, uint8_t age_s)
{
	ARG_UNUSED(tag);
	ARG_UNUSED(lat_e7);
	ARG_UNUSED(lon_e7);
	ARG_UNUSED(age_s);
}
#endif
#endif
