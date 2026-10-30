/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_HEALTH_H
#define BUDDYGUARD_HEALTH_H
#include <stdbool.h>
int bg_health_watchdog_start(void);
int bg_health_check_slots(void);
void bg_health_begin(void);
/* Failed cycles reset the healthy window and deliberately do not feed WDT. */
void bg_health_cycle(bool healthy, bool allow_confirmation);
#endif
