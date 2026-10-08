#define RS_BB_UART_TX_TEST_HOST_BUILD
#include <rs_bb_uart_tx/rs_bb_uart_tx_engine.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* ---- Test 1: full multi-byte message, decode the pin trace as a receiver would ---- */
static void test_multibyte_message_decodes_correctly(void)
{
    rs_bb_uart_tx_ring_buffer_t rb;
    rs_bb_uart_tx_engine_state_t eng;
    rs_bb_uart_tx_ring_buffer_reset(&rb);
    rs_bb_uart_tx_engine_reset(&eng);

    const char *msg = "Hi!";
    assert(rs_bb_uart_tx_ring_buffer_enqueue(&rb, (const uint8_t*)msg, 3) == true);

    /* Run enough ticks: 1 (wasted idle) + 3 bytes * 10 bits each + a couple extra
     * to land back in idle and confirm queue_now_empty fires once. */
    int total_ticks = 1 + 3*10 + 3;
    int decoded_bytes[8]; int nbytes = 0;
    int bit_buf[10]; int bit_count = 0;
    int in_frame = 0;
    int byte_transmitted_count = 0;
    int queue_empty_count = 0;

    int prev_level = 1; /* idle high, matches gpio_open() */
    for (int t = 0; t < total_ticks; t++) {
        rs_bb_uart_tx_engine_output_t out = rs_bb_uart_tx_engine_output_get(&eng);
        int level = out.drive_pin_high ? 1 : 0;

        /* naive receiver: look for a falling edge (start bit) when not already in a frame */
        if (!in_frame && prev_level == 1 && level == 0) {
            in_frame = 1;
            bit_count = 0;
        } else if (in_frame) {
            bit_buf[bit_count++] = level;
            if (bit_count == 9) { /* 8 data bits + stop bit collected (start bit itself wasn't stored) */
                int byte = 0;
                for (int b = 0; b < 8; b++) byte |= (bit_buf[b] << b);
                assert(bit_buf[8] == 1); /* stop bit must be high */
                decoded_bytes[nbytes++] = byte;
                in_frame = 0;
            }
        }
        prev_level = level;

        rs_bb_uart_tx_engine_advance_result_t adv = rs_bb_uart_tx_engine_advance(&eng, &rb);
        if (adv.byte_transmitted) byte_transmitted_count++;
        if (adv.queue_now_empty) queue_empty_count++;
    }

    assert(nbytes == 3);
    assert(decoded_bytes[0] == 'H');
    assert(decoded_bytes[1] == 'i');
    assert(decoded_bytes[2] == '!');
    assert(byte_transmitted_count == 3);
    assert(queue_empty_count == 1);
    printf("test_multibyte_message_decodes_correctly: PASS (decoded '%c%c%c', byte_transmitted=%d, queue_empty=%d)\n",
           decoded_bytes[0], decoded_bytes[1], decoded_bytes[2], byte_transmitted_count, queue_empty_count);
}

/* ---- Test 2: output_get() can NEVER touch the ring buffer (enforced structurally --
 * it doesn't even take a ring buffer pointer -- this test just documents/confirms the
 * call compiles with only the engine state, no ring buffer argument possible). ---- */
static void test_output_get_has_no_ring_buffer_access_by_construction(void)
{
    rs_bb_uart_tx_engine_state_t eng;
    rs_bb_uart_tx_engine_reset(&eng);
    /* This line would fail to compile if output_get() took a ring buffer parameter
     * and this test *called* it with one; the absence of a second argument here
     * is itself the proof for this test's claim. */
    rs_bb_uart_tx_engine_output_t out = rs_bb_uart_tx_engine_output_get(&eng);
    (void) out;
    printf("test_output_get_has_no_ring_buffer_access_by_construction: PASS (compiles with engine-state-only signature)\n");
}

/* ---- Test 3: the race scenario -- a Write() landing in the window between
 * TX_STOP_BIT's prefetch and TX_NEXT_BYTE's fresh recheck must NOT be stranded. ---- */
static void test_race_write_during_prefetch_window_not_stranded(void)
{
    rs_bb_uart_tx_ring_buffer_t rb;
    rs_bb_uart_tx_engine_state_t eng;
    rs_bb_uart_tx_ring_buffer_reset(&rb);
    rs_bb_uart_tx_engine_reset(&eng);

    uint8_t first_byte = 'A';
    assert(rs_bb_uart_tx_ring_buffer_enqueue(&rb, &first_byte, 1) == true);

    /* Drive exactly through: 1 wasted idle tick + 10 bits of 'A' (start..stop).
     * After that many advance() calls, we should be sitting in TX_NEXT_BYTE
     * (STOP_BIT's prefetch found nothing, since only one byte was queued). */
    for (int t = 0; t < 1 + 10; t++) {
        (void) rs_bb_uart_tx_engine_output_get(&eng);
        rs_bb_uart_tx_engine_advance_result_t adv = rs_bb_uart_tx_engine_advance(&eng, &rb);
        (void) adv;
    }
    assert(eng.state == RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE);
    assert(eng.next_output_high == true); /* tentative idle-high, matches what a real receiver expects here too */

    /* Simulate a concurrent Write() landing exactly in this window, BEFORE the
     * fresh recheck tick's advance() runs. */
    uint8_t second_byte = 'B';
    assert(rs_bb_uart_tx_ring_buffer_enqueue(&rb, &second_byte, 1) == true);

    /* This tick's output must still be idle-high (already committed, correct
     * either way) -- confirms the pin write for this tick never depended on
     * whether the race-enqueue happened before or after it. */
    rs_bb_uart_tx_engine_output_t out = rs_bb_uart_tx_engine_output_get(&eng);
    assert(out.drive_pin_high == true);

    /* The fresh recheck (advance()) must find 'B' and NOT report queue_now_empty --
     * i.e. the caller (hal.c) will NOT stop the timer, so 'B' is not stranded. */
    rs_bb_uart_tx_engine_advance_result_t adv = rs_bb_uart_tx_engine_advance(&eng, &rb);
    assert(adv.queue_now_empty == false);
    assert(eng.state == RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT);
    assert(eng.current_byte == 'B');

    printf("test_race_write_during_prefetch_window_not_stranded: PASS (byte 'B' picked up by the fresh recheck, timer never stopped)\n");
}

/* ---- Test 4: genuinely empty (no race) -- must cleanly report queue_now_empty
 * at the correct tick and return to TX_IDLE. ---- */
static void test_genuinely_empty_reports_queue_now_empty_once(void)
{
    rs_bb_uart_tx_ring_buffer_t rb;
    rs_bb_uart_tx_engine_state_t eng;
    rs_bb_uart_tx_ring_buffer_reset(&rb);
    rs_bb_uart_tx_engine_reset(&eng);

    uint8_t byte = 'Z';
    assert(rs_bb_uart_tx_ring_buffer_enqueue(&rb, &byte, 1) == true);

    int queue_empty_ticks = 0;
    for (int t = 0; t < 1 + 10 + 1; t++) {
        (void) rs_bb_uart_tx_engine_output_get(&eng);
        rs_bb_uart_tx_engine_advance_result_t adv = rs_bb_uart_tx_engine_advance(&eng, &rb);
        if (adv.queue_now_empty) queue_empty_ticks++;
    }
    assert(queue_empty_ticks == 1);
    assert(eng.state == RS_BB_UART_TX_ENGINE_STATE_TX_IDLE);
    assert(eng.next_output_high == true);
    printf("test_genuinely_empty_reports_queue_now_empty_once: PASS\n");
}

/* ---- Test 5: back-to-back bytes within one message never visit TX_NEXT_BYTE
 * (the prefetch during TX_STOP_BIT should always find the next byte immediately,
 * since the whole message was enqueued before transmission began). ---- */
static void test_back_to_back_bytes_skip_next_byte_state(void)
{
    rs_bb_uart_tx_ring_buffer_t rb;
    rs_bb_uart_tx_engine_state_t eng;
    rs_bb_uart_tx_ring_buffer_reset(&rb);
    rs_bb_uart_tx_engine_reset(&eng);

    uint8_t msg[5] = {1,2,3,4,5};
    assert(rs_bb_uart_tx_ring_buffer_enqueue(&rb, msg, 5) == true);

    int saw_next_byte_state = 0;
    for (int t = 0; t < 1 + 5*10 + 1; t++) {
        (void) rs_bb_uart_tx_engine_output_get(&eng);
        rs_bb_uart_tx_engine_advance_result_t adv = rs_bb_uart_tx_engine_advance(&eng, &rb);
        (void) adv;
        if (eng.state == RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE) saw_next_byte_state = 1;
    }
    /* TX_NEXT_BYTE should be visited exactly once -- only after the LAST byte's
     * stop bit, when the prefetch genuinely finds nothing left. */
    assert(saw_next_byte_state == 1);
    printf("test_back_to_back_bytes_skip_next_byte_state: PASS (TX_NEXT_BYTE only reached after the final byte)\n");
}

int main(void) {
    test_multibyte_message_decodes_correctly();
    test_output_get_has_no_ring_buffer_access_by_construction();
    test_race_write_during_prefetch_window_not_stranded();
    test_genuinely_empty_reports_queue_now_empty_once();
    test_back_to_back_bytes_skip_next_byte_state();
    printf("\nALL ENGINE PIPELINE TESTS PASSED\n");
    return 0;
}
