/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#if defined(CONFIG_BUDDYGUARD_WIFI)
void bg_wifi_start(void);
#else
static inline void bg_wifi_start(void) {}
#endif
