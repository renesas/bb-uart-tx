/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file test_rs_bb_uart_tx_engine.c
 * @brief Unit tests for the Bit-Banged UART TX bit-level state machine.
 *
 * Tests rs_bb_uart_tx_engine_{reset,output_get,advance}() (declared in
 * rs_bb_uart_tx_engine.h).  The engine is hardware-agnostic: no FSP, no
 * timer, no pin.  The pipelined two-call design means the level returned by
 * output_get() on tick N was decided by advance() on tick N-1; these tests
 * pin that contract down state by state, and then check the resulting
 * waveform frame by frame.
 *
 * helper_engine_run() (tests/rs_bb_uart_tx_test_helpers.h) drives the engine
 * exactly like rs_bb_uart_tx_hal_gpt_callback(): one output_get() then one
 * advance() per tick.  One tick == one bit period, so the recorded levels are
 * the TX line sampled once per bit period, and edge timing is exact by
 * construction (this is the deterministic half of the jitter specification;
 * the analogue half needs a logic analyser — see tests/Bit_Banged_UART_TX_
 * Jitter_Specification.txt).
 *
 * Test groups:
 *   BbUartEngine      — per-state transitions, pipeline register, race window.
 *   BbUartEngineFrame — 8N1 frame structure, start-to-start spacing, stop bit,
 *                       back-to-back bytes, restart after idle.
 **********************************************************************************************************************/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity_fixture.h"

#include "rs_bb_uart_tx/rs_bb_uart_tx_engine.h"
#include "rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h"
#include "rs_bb_uart_tx_test_helpers.h"

/* ── Shared fixtures ─────────────────────────────────────────────────────── */

static rs_bb_uart_tx_engine_state_t g_eng;
static rs_bb_uart_tx_ring_buffer_t  g_rb;

static void engine_fixture_reset(void)
{
    memset(&g_eng, 0xEE, sizeof(g_eng));   /* dirty on purpose */
    memset(&g_rb,  0,    sizeof(g_rb));
    rs_bb_uart_tx_engine_reset(&g_eng);
    rs_bb_uart_tx_ring_buffer_reset(&g_rb);
}

static void enqueue_byte(uint8_t b)
{
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, &b, 1U));
}

/** Enqueue @p b and run the first tick (idle-high), leaving the engine in START_BIT. */
static void engine_to_start_bit(uint8_t b)
{
    enqueue_byte(b);
    (void) rs_bb_uart_tx_engine_output_get(&g_eng);
    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT, g_eng.state);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: BbUartEngine
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(BbUartEngine);

TEST_SETUP(BbUartEngine)
{
    engine_fixture_reset();
}

TEST_TEAR_DOWN(BbUartEngine)
{
}

/* Reset: IDLE, idle-high pending, dirty RAM fully overwritten */
TEST(BbUartEngine, ResetStateIsIdleWithIdleHighPending)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_IDLE, g_eng.state);
    TEST_ASSERT_TRUE(g_eng.next_output_high);
    TEST_ASSERT_EQUAL_UINT8(0U, g_eng.current_byte);
    TEST_ASSERT_EQUAL_UINT8(0U, g_eng.bit_index);
}

/* output_get() is a pure read of the pipeline register */
TEST(BbUartEngine, OutputGetReturnsPipelineRegister)
{
    g_eng.next_output_high = true;
    TEST_ASSERT_TRUE(rs_bb_uart_tx_engine_output_get(&g_eng).drive_pin_high);
    g_eng.next_output_high = false;
    TEST_ASSERT_FALSE(rs_bb_uart_tx_engine_output_get(&g_eng).drive_pin_high);
}

/* output_get() has no side effects on the engine */
TEST(BbUartEngine, OutputGetDoesNotModifyEngine)
{
    rs_bb_uart_tx_engine_state_t before;

    g_eng.state = RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS;
    g_eng.current_byte = 0x3CU;
    g_eng.bit_index = 4U;
    g_eng.next_output_high = false;
    memcpy(&before, &g_eng, sizeof(before));

    (void) rs_bb_uart_tx_engine_output_get(&g_eng);
    TEST_ASSERT_EQUAL_MEMORY(&before, &g_eng, sizeof(before));
}

/* IDLE with nothing queued: stay idle-high, report nothing */
TEST(BbUartEngine, IdleWithEmptyQueueStaysIdleHigh)
{
    rs_bb_uart_tx_engine_advance_result_t r = rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_IDLE, g_eng.state);
    TEST_ASSERT_TRUE(g_eng.next_output_high);
    TEST_ASSERT_FALSE(r.byte_transmitted);
    TEST_ASSERT_FALSE(r.queue_now_empty);
}

/* IDLE with a byte queued: dequeue it, arm the start bit (low) for the NEXT tick */
TEST(BbUartEngine, IdleWithQueuedByteLoadsStartBit)
{
    enqueue_byte(0x55U);
    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT, g_eng.state);
    TEST_ASSERT_FALSE(g_eng.next_output_high);
    TEST_ASSERT_EQUAL_HEX8(0x55U, g_eng.current_byte);
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_count_get(&g_rb));
}

/* Start bit displayed -> next output is D0 (the LSB): 1 for odd bytes ... */
TEST(BbUartEngine, StartBitAdvanceArmsLsbHighForOddByte)
{
    engine_to_start_bit(0x01U);
    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS, g_eng.state);
    TEST_ASSERT_TRUE(g_eng.next_output_high);
    TEST_ASSERT_EQUAL_UINT8(0U, g_eng.bit_index);
}

/* ... and 0 for even bytes (so the MSB-first mistake would show up here) */
TEST(BbUartEngine, StartBitAdvanceArmsLsbLowForEvenByte)
{
    engine_to_start_bit(0x80U);
    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    TEST_ASSERT_FALSE(g_eng.next_output_high);
}

/* Data bits are shifted out LSB first: walk 0xA5 = 1010_0101 bit by bit */
TEST(BbUartEngine, DataBitsShiftLsbFirst)
{
    static uint8_t const expect[8] = { 1U, 0U, 1U, 0U, 0U, 1U, 0U, 1U };   /* D0..D7 of 0xA5 */

    engine_to_start_bit(0xA5U);
    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);   /* start bit shown -> D0 armed */

    for (uint32_t i = 0U; i < 8U; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(expect[i], rs_bb_uart_tx_engine_output_get(&g_eng).drive_pin_high ? 1U : 0U);
        TEST_ASSERT_EQUAL_UINT8((uint8_t) i, g_eng.bit_index);
        (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    }
}

/* After D7 is displayed the stop bit (high) is armed */
TEST(BbUartEngine, LastDataBitTransitionsToStopBitHigh)
{
    engine_to_start_bit(0x00U);                             /* all data bits low */
    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);     /* D0 armed */
    for (uint32_t i = 0U; i < 7U; i++)                      /* display D0..D6 */
    {
        (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    }
    TEST_ASSERT_EQUAL_UINT8(7U, g_eng.bit_index);
    TEST_ASSERT_FALSE(g_eng.next_output_high);              /* D7 = 0 is what will be shown */

    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);     /* D7 displayed */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT, g_eng.state);
    TEST_ASSERT_TRUE(g_eng.next_output_high);               /* stop bit high */
}

/* Stop bit displayed and another byte already queued: prefetch it, start bit next, no NEXT_BYTE detour */
TEST(BbUartEngine, StopBitWithNextByteQueuedPrefetchesStartBit)
{
    rs_bb_uart_tx_engine_advance_result_t r;

    g_eng.state = RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT;
    enqueue_byte(0x42U);
    r = rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);

    TEST_ASSERT_TRUE(r.byte_transmitted);
    TEST_ASSERT_FALSE(r.queue_now_empty);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT, g_eng.state);
    TEST_ASSERT_FALSE(g_eng.next_output_high);
    TEST_ASSERT_EQUAL_HEX8(0x42U, g_eng.current_byte);
}

/* Stop bit displayed with an empty queue: byte counted, but do NOT declare "empty" yet — defer to NEXT_BYTE */
TEST(BbUartEngine, StopBitWithEmptyQueueDefersToNextByteWithIdleHigh)
{
    rs_bb_uart_tx_engine_advance_result_t r;

    g_eng.state = RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT;
    r = rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);

    TEST_ASSERT_TRUE(r.byte_transmitted);
    TEST_ASSERT_FALSE(r.queue_now_empty);   /* the prefetch result is never trusted for stopping the timer */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE, g_eng.state);
    TEST_ASSERT_TRUE(g_eng.next_output_high);
}

/* NEXT_BYTE fresh recheck finds nothing: return to IDLE and report the queue freshly empty */
TEST(BbUartEngine, NextByteWithEmptyQueueReturnsToIdleAndReportsEmpty)
{
    rs_bb_uart_tx_engine_advance_result_t r;

    g_eng.state = RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE;
    r = rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);

    TEST_ASSERT_TRUE(r.queue_now_empty);
    TEST_ASSERT_FALSE(r.byte_transmitted);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_IDLE, g_eng.state);
    TEST_ASSERT_TRUE(g_eng.next_output_high);
}

/* THE race window: a byte enqueued AFTER the stop-bit prefetch but BEFORE the NEXT_BYTE recheck is not stranded */
TEST(BbUartEngine, NextByteRecheckPicksUpByteEnqueuedAfterPrefetch)
{
    rs_bb_uart_tx_engine_advance_result_t r;

    /* Tick A: stop bit displayed, queue empty -> tentative idle, NEXT_BYTE. */
    g_eng.state = RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT;
    r = rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE, g_eng.state);

    /* Application Write() lands in the window between the two ticks. */
    enqueue_byte(0x7EU);

    /* Tick B: fresh dequeue must find it and must NOT report empty (timer must keep running). */
    r = rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    TEST_ASSERT_FALSE(r.queue_now_empty);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT, g_eng.state);
    TEST_ASSERT_FALSE(g_eng.next_output_high);
    TEST_ASSERT_EQUAL_HEX8(0x7EU, g_eng.current_byte);
}

/* START and DATA states never touch the ring buffer (that is the point of the pipeline) */
TEST(BbUartEngine, StartAndDataStatesDoNotAccessRingBuffer)
{
    engine_to_start_bit(0x0FU);
    enqueue_byte(0xAAU);                                    /* queued behind the current byte */
    uint32_t head = g_rb.head;
    uint32_t tail = g_rb.tail;

    for (uint32_t i = 0U; i < 9U; i++)                      /* start-bit advance + D0..D7 advances */
    {
        (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
        TEST_ASSERT_EQUAL_UINT32(head, g_rb.head);
        TEST_ASSERT_EQUAL_UINT32(tail, g_rb.tail);
    }
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT, g_eng.state);
}

/* Corrupt/unknown state fails safe to idle-high */
TEST(BbUartEngine, UnknownStateFailsSafeToIdleHigh)
{
    g_eng.state = (rs_bb_uart_tx_bit_state_t) 0x7F;
    g_eng.next_output_high = false;

    (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_IDLE, g_eng.state);
    TEST_ASSERT_TRUE(g_eng.next_output_high);
}

/* byte_transmitted fires exactly once per byte, on the stop-bit tick only */
TEST(BbUartEngine, ByteTransmittedReportedExactlyOncePerByte)
{
    uint8_t levels[64];

    enqueue_byte(0xC3U);
    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, levels, sizeof(levels));
    TEST_ASSERT_TRUE(r.drained);
    TEST_ASSERT_EQUAL_UINT32(1U, r.bytes_transmitted);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartEngine)
{
    RUN_TEST_CASE(BbUartEngine, ResetStateIsIdleWithIdleHighPending);
    RUN_TEST_CASE(BbUartEngine, OutputGetReturnsPipelineRegister);
    RUN_TEST_CASE(BbUartEngine, OutputGetDoesNotModifyEngine);
    RUN_TEST_CASE(BbUartEngine, IdleWithEmptyQueueStaysIdleHigh);
    RUN_TEST_CASE(BbUartEngine, IdleWithQueuedByteLoadsStartBit);
    RUN_TEST_CASE(BbUartEngine, StartBitAdvanceArmsLsbHighForOddByte);
    RUN_TEST_CASE(BbUartEngine, StartBitAdvanceArmsLsbLowForEvenByte);
    RUN_TEST_CASE(BbUartEngine, DataBitsShiftLsbFirst);
    RUN_TEST_CASE(BbUartEngine, LastDataBitTransitionsToStopBitHigh);
    RUN_TEST_CASE(BbUartEngine, StopBitWithNextByteQueuedPrefetchesStartBit);
    RUN_TEST_CASE(BbUartEngine, StopBitWithEmptyQueueDefersToNextByteWithIdleHigh);
    RUN_TEST_CASE(BbUartEngine, NextByteWithEmptyQueueReturnsToIdleAndReportsEmpty);
    RUN_TEST_CASE(BbUartEngine, NextByteRecheckPicksUpByteEnqueuedAfterPrefetch);
    RUN_TEST_CASE(BbUartEngine, StartAndDataStatesDoNotAccessRingBuffer);
    RUN_TEST_CASE(BbUartEngine, UnknownStateFailsSafeToIdleHigh);
    RUN_TEST_CASE(BbUartEngine, ByteTransmittedReportedExactlyOncePerByte);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: BbUartEngineFrame
 * Waveform-level checks: the line level sampled once per bit period.
 * ═════════════════════════════════════════════════════════════════════════ */

#define LEVELS_MAX   (2048U)

static uint8_t g_levels[LEVELS_MAX];

TEST_GROUP(BbUartEngineFrame);

TEST_SETUP(BbUartEngineFrame)
{
    engine_fixture_reset();
    memset(g_levels, 0xEE, sizeof(g_levels));
}

TEST_TEAR_DOWN(BbUartEngineFrame)
{
}

/* One byte: idle, then EXACTLY start(0) D0..D7 stop(1), then one trailing idle tick = 12 ticks */
TEST(BbUartEngineFrame, SingleByteWaveformIsIdleStartDataStopIdle)
{
    uint8_t expect[12];

    expect[0] = 1U;                          /* launch tick: idle-high */
    helper_expected_frame(0xA5U, &expect[1]);
    expect[11] = 1U;                         /* NEXT_BYTE tick: idle-high */

    enqueue_byte(0xA5U);
    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

    TEST_ASSERT_TRUE(r.drained);
    TEST_ASSERT_EQUAL_UINT32(12U, r.ticks);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, g_levels, 12U);
}

/* First tick after a fresh start is idle-high, THEN the start bit (one bit period of launch latency) */
TEST(BbUartEngineFrame, LaunchLatencyIsExactlyOneIdleTick)
{
    enqueue_byte(0x00U);
    (void) helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

    TEST_ASSERT_EQUAL_UINT8(1U, g_levels[0]);   /* idle */
    TEST_ASSERT_EQUAL_UINT8(0U, g_levels[1]);   /* start bit */
}

/* Every one of the 256 byte values survives the trip through the engine and a UART receiver model */
TEST(BbUartEngineFrame, AllByteValuesDecodeCorrectly)
{
    for (uint32_t v = 0U; v < 256U; v++)
    {
        uint8_t decoded = 0U;

        engine_fixture_reset();
        enqueue_byte((uint8_t) v);
        helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

        TEST_ASSERT_TRUE(r.drained);
        TEST_ASSERT_EQUAL_INT32(1, helper_decode_frames(g_levels, r.ticks, &decoded, 1U, NULL));
        TEST_ASSERT_EQUAL_HEX8((uint8_t) v, decoded);
    }
}

/* Multi-byte string decodes in order */
TEST(BbUartEngineFrame, StringDecodesInOrder)
{
    char const msg[] = "Hello, UART!";
    uint8_t    out[sizeof(msg)];

    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, (uint8_t const *) msg, (uint32_t) (sizeof(msg) - 1U)));
    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

    TEST_ASSERT_TRUE(r.drained);
    TEST_ASSERT_EQUAL_INT32((int32_t) (sizeof(msg) - 1U),
                            helper_decode_frames(g_levels, r.ticks, out, (uint32_t) sizeof(out), NULL));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg, out, sizeof(msg) - 1U);
}

/* Back-to-back bytes: start bits are EXACTLY 10 ticks apart — no idle gap, no drift (tn = n * Tbit) */
TEST(BbUartEngineFrame, BackToBackStartToStartIsExactlyTenTicks)
{
    uint8_t  msg[6] = { 0x00U, 0xFFU, 0x55U, 0xAAU, 0x01U, 0x80U };
    uint8_t  out[6];
    uint32_t starts[6];

    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, msg, 6U));
    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

    TEST_ASSERT_EQUAL_INT32(6, helper_decode_frames(g_levels, r.ticks, out, 6U, starts));
    TEST_ASSERT_EQUAL_UINT32(1U, starts[0]);   /* first start bit after the single launch tick */
    for (uint32_t i = 1U; i < 6U; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(10U, starts[i] - starts[i - 1U]);
    }
}

/* Total run length is exactly 10 * N + 2 ticks: launch tick + N frames + one NEXT_BYTE tick (deterministic duration) */
TEST(BbUartEngineFrame, TotalTicksForNBytesIsTenNPlusTwo)
{
    for (uint32_t n = 1U; n <= 32U; n *= 2U)
    {
        uint8_t data[32];

        engine_fixture_reset();
        for (uint32_t i = 0U; i < n; i++) { data[i] = (uint8_t) (i * 7U + 3U); }
        TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, data, n));
        helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

        TEST_ASSERT_TRUE(r.drained);
        TEST_ASSERT_EQUAL_UINT32(10U * n + 2U, r.ticks);
        TEST_ASSERT_EQUAL_UINT32(n, r.bytes_transmitted);
    }
}

/* Stop bit is high for a FULL bit period before the next start bit (spec: Tstop >= 0.99 * Tbit; here exactly 1 tick) */
TEST(BbUartEngineFrame, StopBitIsHighForAtLeastOneBitPeriodBeforeNextStart)
{
    uint8_t msg[3] = { 0x00U, 0x00U, 0x00U };   /* all-zero data: only the stop bit separates frames */
    uint8_t out[3];
    uint32_t starts[3];

    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_rb, msg, 3U));
    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);
    TEST_ASSERT_EQUAL_INT32(3, helper_decode_frames(g_levels, r.ticks, out, 3U, starts));

    for (uint32_t i = 0U; i < 3U; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(1U, g_levels[starts[i] + 9U]);          /* stop bit high */
    }
    TEST_ASSERT_EQUAL_UINT8(0U, g_levels[starts[1] - 2U]);              /* D7 of the previous byte (0x00) */
    TEST_ASSERT_EQUAL_UINT8(1U, g_levels[starts[1] - 1U]);              /* exactly one high tick between frames */
}

/* Start bit is exactly one tick long: the tick after it is D0, never another start-low run */
TEST(BbUartEngineFrame, StartBitLastsExactlyOneTick)
{
    enqueue_byte(0xFFU);                        /* D0 high, so start bit's width is directly visible */
    (void) helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

    TEST_ASSERT_EQUAL_UINT8(0U, g_levels[1]);   /* start */
    TEST_ASSERT_EQUAL_UINT8(1U, g_levels[2]);   /* D0 = 1 */
}

/* A byte enqueued while the previous one is mid-frame is sent immediately after it, with no idle gap */
TEST(BbUartEngineFrame, ByteEnqueuedMidFrameFollowsWithoutGap)
{
    uint8_t out[2];
    uint32_t starts[2];
    uint32_t n = 0U;

    enqueue_byte(0x11U);
    for (uint32_t i = 0U; i < 6U; i++)          /* run 6 ticks: we are inside byte 0's data bits */
    {
        g_levels[n++] = rs_bb_uart_tx_engine_output_get(&g_eng).drive_pin_high ? 1U : 0U;
        (void) rs_bb_uart_tx_engine_advance(&g_eng, &g_rb);
    }
    enqueue_byte(0x22U);                        /* application Write() while transmitting */

    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, &g_levels[n], LEVELS_MAX - n);
    n += r.ticks;

    TEST_ASSERT_TRUE(r.drained);
    TEST_ASSERT_EQUAL_INT32(2, helper_decode_frames(g_levels, n, out, 2U, starts));
    TEST_ASSERT_EQUAL_HEX8(0x11U, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x22U, out[1]);
    TEST_ASSERT_EQUAL_UINT32(10U, starts[1] - starts[0]);
}

/* After the queue drains the engine is idle and the line level stays high */
TEST(BbUartEngineFrame, LineIdlesHighAfterDrain)
{
    enqueue_byte(0x3AU);
    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, g_levels, LEVELS_MAX);

    TEST_ASSERT_TRUE(r.drained);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_IDLE, g_eng.state);
    TEST_ASSERT_TRUE(rs_bb_uart_tx_engine_output_get(&g_eng).drive_pin_high);
    TEST_ASSERT_EQUAL_UINT8(1U, g_levels[r.ticks - 1U]);
}

/* A second burst after idle restarts exactly like the first (idle launch tick, then frame) */
TEST(BbUartEngineFrame, RestartAfterIdleRepeatsLaunchSequence)
{
    uint8_t first[12];
    uint8_t second[12];

    enqueue_byte(0x96U);
    (void) helper_engine_run(&g_eng, &g_rb, first, sizeof(first));

    enqueue_byte(0x96U);
    helper_engine_run_t r = helper_engine_run(&g_eng, &g_rb, second, sizeof(second));

    TEST_ASSERT_EQUAL_UINT32(12U, r.ticks);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(first, second, 12U);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartEngineFrame)
{
    RUN_TEST_CASE(BbUartEngineFrame, SingleByteWaveformIsIdleStartDataStopIdle);
    RUN_TEST_CASE(BbUartEngineFrame, LaunchLatencyIsExactlyOneIdleTick);
    RUN_TEST_CASE(BbUartEngineFrame, AllByteValuesDecodeCorrectly);
    RUN_TEST_CASE(BbUartEngineFrame, StringDecodesInOrder);
    RUN_TEST_CASE(BbUartEngineFrame, BackToBackStartToStartIsExactlyTenTicks);
    RUN_TEST_CASE(BbUartEngineFrame, TotalTicksForNBytesIsTenNPlusTwo);
    RUN_TEST_CASE(BbUartEngineFrame, StopBitIsHighForAtLeastOneBitPeriodBeforeNextStart);
    RUN_TEST_CASE(BbUartEngineFrame, StartBitLastsExactlyOneTick);
    RUN_TEST_CASE(BbUartEngineFrame, ByteEnqueuedMidFrameFollowsWithoutGap);
    RUN_TEST_CASE(BbUartEngineFrame, LineIdlesHighAfterDrain);
    RUN_TEST_CASE(BbUartEngineFrame, RestartAfterIdleRepeatsLaunchSequence);
}
