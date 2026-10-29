#ifndef BUDDYGUARD_RADIO_LINK_H
#define BUDDYGUARD_RADIO_LINK_H
#include <stdint.h>
int bg_radio_init(void);
int bg_radio_sos(void);
int bg_radio_query(uint8_t tag);
#endif
