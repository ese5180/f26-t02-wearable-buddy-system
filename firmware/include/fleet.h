/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#if defined(CONFIG_BUDDYGUARD_FLEET)
void bg_fleet_poll(void);
#else
static inline void bg_fleet_poll(void) {}
#endif
