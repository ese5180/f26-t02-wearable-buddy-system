/* Golden on-air vectors, including peer interoperability and malformed frames. */
#include <assert.h>
#include <string.h>
#include "wire.h"
int main(void)
{
    uint8_t p[BG_PRESENCE_SIZE];
    const uint8_t expected[] = {0xff, 0xff, 1, 3, 0x34, 0x12, 3, 1};
    bg_presence_encode(p, 3, 0x1234, 3, 1);
    assert(memcmp(p, expected, sizeof(p)) == 0);
    struct bg_presence_frame out;
    assert(bg_presence_decode(expected, sizeof(expected), &out));
    assert(out.tag == 3 && out.sequence == 0x1234 && out.flags == 3);
    for (size_t n = 0; n < sizeof(p); ++n) assert(!bg_presence_decode(p, n, &out));
    p[3] = 0; assert(!bg_presence_decode(p, sizeof(p), &out));
    p[3] = 4; assert(!bg_presence_decode(p, sizeof(p), &out));
    p[3] = 1; p[6] = 4; assert(!bg_presence_decode(p, sizeof(p), &out));
    p[6] = 0; p[7] = 2; assert(!bg_presence_decode(p, sizeof(p), &out));
    assert(!bg_presence_decode(NULL, 8, &out));
    uint8_t radio[BG_RADIO_FRAME_SIZE];
    const uint8_t sos[] = {'B','G',1,3,1,0,0xcd,0xab,0,0,0xff,0xff};
    bg_radio_frame_encode(radio, 3, 1, 0xabcd);
    assert(memcmp(radio, sos, sizeof(radio)) == 0);
    assert(bg_radio_frame_valid(sos, sizeof(sos)));
    for (unsigned type = 1; type <= 5; ++type) {
        bg_radio_frame_encode(radio, type, 3, 0xffff);
        assert(bg_radio_frame_valid(radio, sizeof(radio)));
    }
    radio[3] = 6; assert(!bg_radio_frame_valid(radio, sizeof(radio)));
    radio[3] = 0; assert(!bg_radio_frame_valid(radio, sizeof(radio)));
    radio[3] = 1; radio[4] = 4; assert(!bg_radio_frame_valid(radio, sizeof(radio)));
    radio[4] = 0; assert(!bg_radio_frame_valid(radio, sizeof(radio)));
    radio[4] = 1; radio[2] = 2; assert(!bg_radio_frame_valid(radio, sizeof(radio)));
    radio[2] = 1; radio[0] = 'X'; assert(!bg_radio_frame_valid(radio, sizeof(radio)));
    assert(!bg_radio_frame_valid(sos, 11));
    assert(!bg_radio_frame_valid(sos, 13));
    assert(!bg_radio_frame_valid(NULL, 12));
    return 0;
}
