/* Retry policy is driven by fake time; no sleeps or fake sensor measurements. */
#include <assert.h>
#include <string.h>
#include "radio_exchange.h"
#include "radio_history.h"

int main(void)
{
    struct bg_radio_exchange tx = {0};
    uint8_t query[BG_RADIO_FRAME_SIZE], reply[BG_RADIO_FRAME_SIZE];
    bg_radio_frame_encode(query, BG_RADIO_QUERY, 1, 42);
    bg_radio_exchange_start(&tx, query, 100, 1500, 100);
    assert(bg_radio_exchange_next(&tx, 100) == BG_EXCHANGE_SEND);
    bg_radio_exchange_sent(&tx, 100);

    /* STATUS/SOS arriving during a query wait must remain available to the
     * dispatcher, and must not complete or cancel the pending query. */
    bg_radio_frame_encode(reply, BG_RADIO_STATUS, 1, 42);
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    assert(tx.active && tx.attempts == 1);
    bg_radio_frame_encode(reply, BG_RADIO_ACK, 1, 42);
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    bg_radio_frame_encode(reply, BG_RADIO_REPLY, 2, 42);
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    bg_radio_frame_encode(reply, BG_RADIO_REPLY, 1, 41);
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    assert(bg_radio_exchange_next(&tx, 1599) == BG_EXCHANGE_WAIT);
    assert(bg_radio_exchange_next(&tx, 1600) == BG_EXCHANGE_WAIT);
    assert(bg_radio_exchange_next(&tx, 1699) == BG_EXCHANGE_WAIT);
    assert(bg_radio_exchange_next(&tx, 1700) == BG_EXCHANGE_SEND);
    bg_radio_exchange_sent(&tx, 1700);
    bg_radio_frame_encode(reply, BG_RADIO_REPLY, 1, 42);
    assert(bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    assert(!tx.active);
    assert(bg_radio_exchange_next(&tx, 9999) == BG_EXCHANGE_WAIT);

    /* No response: three sends with the SAME sequence, then one failure. */
    bg_radio_exchange_start(&tx, query, 0, 600, 100);
    for (unsigned n = 0; n < 3; ++n) {
        uint32_t now = n * 700;
        assert(bg_radio_exchange_next(&tx, now) == BG_EXCHANGE_SEND);
        assert(memcmp(tx.frame, query, sizeof(query)) == 0);
        bg_radio_exchange_sent(&tx, now);
        assert(tx.attempts == n + 1);
        enum bg_radio_exchange_action result = bg_radio_exchange_next(&tx, now + 600);
        assert(result == (n == 2 ? BG_EXCHANGE_EXHAUSTED : BG_EXCHANGE_WAIT));
    }
    assert(!tx.active);
    assert(bg_radio_exchange_next(&tx, 10000) == BG_EXCHANGE_WAIT);

    /* Millisecond wrap and a late ACK during backoff. Typed ACKs distinguish
     * a REPLY from a STATUS with the same sequence. Legacy ACK(0) still works. */
    uint8_t status[BG_RADIO_FRAME_SIZE];
    bg_radio_frame_encode(status, BG_RADIO_STATUS, 1, 9);
    bg_radio_exchange_start(&tx, status, UINT32_MAX - 50, 100, 20);
    bg_radio_exchange_sent(&tx, UINT32_MAX - 50);
    assert(bg_radio_exchange_next(&tx, 48) == BG_EXCHANGE_WAIT);
    assert(bg_radio_exchange_next(&tx, 49) == BG_EXCHANGE_WAIT);
    bg_radio_frame_encode(reply, BG_RADIO_QUERY, 1, 9);
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    bg_radio_frame_encode(reply, BG_RADIO_ACK, 1, 9);
    reply[5] = BG_RADIO_REPLY;
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    reply[5] = BG_RADIO_STATUS;
    assert(bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    bg_radio_exchange_start(&tx, status, 0, 600, 100);
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    bg_radio_exchange_sent(&tx, 0);
    reply[5] = 0;
    assert(bg_radio_exchange_accept(&tx, reply, sizeof(reply)));
    assert(!bg_radio_exchange_accept(&tx, reply, sizeof(reply)));

    /* Interleaved retransmissions do not repeat a business event, but the
     * dispatcher may ACK both copies. Cache is bounded, not replay security. */
    struct bg_radio_history history = {0};
    assert(!bg_radio_history_seen(&history, status));
    bg_radio_frame_encode(reply, BG_RADIO_SOS, 1, 10);
    assert(!bg_radio_history_seen(&history, reply));
    assert(bg_radio_history_seen(&history, status));
    assert(bg_radio_history_seen(&history, reply));
    bg_radio_frame_encode(reply, BG_RADIO_STATUS, 2, 9);
    assert(!bg_radio_history_seen(&history, reply));
    bg_radio_frame_encode(reply, BG_RADIO_REPLY, 1, 9);
    assert(!bg_radio_history_seen(&history, reply));
    for (unsigned i = 0; i < BG_RADIO_HISTORY_SIZE; ++i) {
        bg_radio_frame_encode(reply, BG_RADIO_STATUS, 1, (uint16_t)(100 + i));
        assert(!bg_radio_history_seen(&history, reply));
    }
    assert(!bg_radio_history_seen(&history, status));
    return 0;
}
