/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BUDDYGUARD_RADIO_DRIVER_H
#define BUDDYGUARD_RADIO_DRIVER_H
#include <stdint.h>
#include <zephyr/kernel.h>
/* Init only after arming WDT. Send/receive are serialized by the radio worker. */
int bg_radio_driver_init(void);
int bg_radio_driver_send(uint8_t *frame);
int bg_radio_driver_receive(uint8_t *frame, k_timeout_t timeout,
							int16_t *rssi, int8_t *snr);
#endif
