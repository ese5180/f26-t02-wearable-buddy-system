/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_PRESENCE_H
#define BUDDYGUARD_PRESENCE_H
#include <stdbool.h>
#include <stdint.h>
struct bg_presence_status {
	uint16_t sequence;
	bool advertising, connected, maintenance, heartbeat;
};
int bg_presence_init(void);
int bg_presence_tick(void);
void bg_presence_maintenance(unsigned seconds);
void bg_presence_heartbeat(bool enabled);
struct bg_presence_status bg_presence_status(void);
#endif
