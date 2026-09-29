/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file test_rs_bb_uart_tx_waveform.c
 * @brief Host-only end-to-end tests: public API -> ring buffer -> engine ->
 *        real HAL ISR callback -> fake GPIO pin, decoded by a UART receiver model.
 *
 * Everything from ConfigGen()/ConfigCheck()/Write() down to the pin is the
 * real SBB code.  Only the FSP boundary is faked (tests/stubs/
 * rs_bb_uart_tx_fake_fsp.*): fake_timer_tick() plays the GPT interrupt, and
 * the pin write log — one entry per tick, one tick per bit period — is the TX
 * waveform sampled once per bit period.
 *
 * Relation to tests/Bit_Banged_UART_TX_Jitter_Specification.txt
 * --------------------------------------------------------------
 * That specification is about analogue edge timing measured with a logic
 * analyser (see the FPB-RA2E3 verification PDF) and cannot be replayed on a
 * host.  What CAN be proven here, exactly and deterministically, is the
 * logical timing contract the analogue result depends on:
 *   - every frame is exactly 10 bit periods, start-to-start (t_n = n * Tbit,
 *     zero drift);
 *   - the stop bit is high for a full bit period (spec: Tstop >= 0.99 Tbit);
 *   - the only extra latency is a single idle bit period before the first
 *     start bit of a burst (spec: request-to-start latency is excluded from
 *     the jitter figure);
 *   - the pin is written exactly once per tick, before any ring-buffer work.
 *
 * Test groups:
 *   BbUartWaveform — decoded content, frame spacing, stop bit, launch latency,
 *                    burst duration, back-to-back and restart behaviour.
 **********************************************************************************************************************/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity_fixture.h"

#include "rs_bb_uart_tx/rs_bb_uart_tx.h"
#include "rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h"
#include "rs_bb_uart_tx_fake_fsp.h"
#include "rs_bb_uart_tx_test_helpers.h"

#define TICKS_MAX   (FAKE_FSP_LEVEL_LOG_MAX - 16U)

static rs_bb_uart_tx_cfg_t g_cfg;

static void configure(uint32_t baud)
{
    fake_fsp_reset();
    helper_singleton_reset();
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS,
                      rs_bb_uart_tx_ConfigGen(&g_cfg, &g_fake_timer_instance, BSP_IO_PORT_01_PIN_09));
    g_cfg.baud_rate = baud;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));
}

/** Run the fake GPT until the SBB stops it; asserts it did stop (no runaway). */
static uint32_t drain(void)
{
    uint32_t n = fake_timer_run_until_stopped(TICKS_MAX);
    TEST_ASSERT_FALSE_MESSAGE(g_fake_fsp.running, "timer still running after TICKS_MAX ticks");
    return n;
}

TEST_GROUP(BbUartWaveform);

TEST_SETUP(BbUartWaveform)
{
    configure(RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE);
}

TEST_TEAR_DOWN(BbUartWaveform)
{
    helper_singleton_reset();
}

/* Opening the port drives NO waveform: the pin is only configured (idle-high), never written */
TEST(BbUartWaveform, OpenWritesNoLevelAndConfiguresIdleHigh)
{
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.pin_write_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.pin_cfg_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.open_count);
    TEST_ASSERT_FALSE(g_fake_fsp.running);
}

/* A written string comes out of the pin as exactly that string */
TEST(BbUartWaveform, WriteStringDecodesToSameString)
{
    char const msg[] = "Hello, world\r\n";
    uint8_t    out[sizeof(msg)];

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString(msg));
    uint32_t ticks = drain();

    TEST_ASSERT_EQUAL_UINT32(ticks, g_fake_fsp.level_count);
    TEST_ASSERT_EQUAL_INT32((int32_t) (sizeof(msg) - 1U),
                            helper_decode_frames(g_fake_fsp.level_log, g_fake_fsp.level_count, out, sizeof(out), NULL));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(msg, out, sizeof(msg) - 1U);
}

/* All 256 byte values survive the full stack unchanged */
TEST(BbUartWaveform, AllByteValuesRoundTrip)
{
    uint8_t in[256];
    uint8_t out[256];

    for (uint32_t i = 0U; i < 256U; i++) { in[i] = (uint8_t) i; }

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(in, 256U));
    (void) drain();

    TEST_ASSERT_EQUAL_INT32(256, helper_decode_frames(g_fake_fsp.level_log, g_fake_fsp.level_count, out, 256U, NULL));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, 256U);
}

/* Burst of N bytes lasts EXACTLY 10*N + 2 ticks (launch idle + N frames + one NEXT_BYTE idle): fixed, gap-free duration */
TEST(BbUartWaveform, BurstDurationIsExactlyTenNPlusTwoTicks)
{
    static uint32_t const sizes[] = { 1U, 2U, 10U, 100U, HELPER_RB_CAPACITY };
    static uint8_t data[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];

    for (uint32_t i = 0U; i < RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES; i++) { data[i] = (uint8_t) (i * 13U + 5U); }

    for (uint32_t s = 0U; s < (sizeof(sizes) / sizeof(sizes[0])); s++)
    {
        configure(RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE);
        TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(data, sizes[s]));

        TEST_ASSERT_EQUAL_UINT32(10U * sizes[s] + 2U, drain());
        TEST_ASSERT_EQUAL_UINT32(10U * sizes[s] + 2U, g_fake_fsp.pin_write_count);
    }
}

/* Launch latency: exactly ONE idle-high bit period, then the start bit */
TEST(BbUartWaveform, LaunchLatencyIsExactlyOneIdleBitPeriod)
{
    (void) rs_bb_uart_tx_WriteString("U");
    (void) drain();

    TEST_ASSERT_EQUAL_UINT8(1U, g_fake_fsp.level_log[0]);
    TEST_ASSERT_EQUAL_UINT8(0U, g_fake_fsp.level_log[1]);
}

/* Start-to-start spacing is exactly 10 ticks for every consecutive frame (t_n = n * Tbit, no drift) */
TEST(BbUartWaveform, StartToStartSpacingIsExactlyTenTicks)
{
    uint8_t  in[40];
    uint8_t  out[40];
    uint32_t starts[40];

    for (uint32_t i = 0U; i < 40U; i++) { in[i] = (uint8_t) (i * 37U); }
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(in, 40U));
    (void) drain();

    TEST_ASSERT_EQUAL_INT32(40, helper_decode_frames(g_fake_fsp.level_log, g_fake_fsp.level_count, out, 40U, starts));
    TEST_ASSERT_EQUAL_UINT32(1U, starts[0]);
    for (uint32_t i = 1U; i < 40U; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(10U, starts[i] - starts[i - 1U]);
    }
}

/* Every frame's stop bit is high (no framing error anywhere), including all-zero data where only the stop bit separates frames */
TEST(BbUartWaveform, EveryFrameHasHighStopBit)
{
    uint8_t zeros[64];
    uint8_t out[64];
    uint32_t starts[64];

    memset(zeros, 0, sizeof(zeros));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(zeros, 64U));
    (void) drain();

    TEST_ASSERT_EQUAL_INT32(64, helper_decode_frames(g_fake_fsp.level_log, g_fake_fsp.level_count, out, 64U, starts));
    for (uint32_t i = 0U; i < 64U; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(1U, g_fake_fsp.level_log[starts[i] + 9U]);
    }
}

/* After the burst the timer is stopped, state is READY, and diagnostics agree with what went out */
TEST(BbUartWaveform, DrainStopsTimerReturnsToReadyAndCountersAgree)
{
    rs_bb_uart_tx_state_t st;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("counters"));
    (void) drain();
    (void) rs_bb_uart_tx_StatusGet(&st);

    TEST_ASSERT_FALSE(g_fake_fsp.running);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.stop_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, st.state);
    TEST_ASSERT_EQUAL_UINT32(8U, st.total_bytes_accepted);
    TEST_ASSERT_EQUAL_UINT32(8U, st.total_bytes_transmitted);
    TEST_ASSERT_EQUAL_UINT32(0U, st.queued_bytes);
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, st.free_bytes);
}

/* total_bytes_transmitted ticks up on each STOP bit: 0 after 10 ticks, 1 after 11, 2 after 21 */
TEST(BbUartWaveform, TransmittedCounterIncrementsOnEachStopBit)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("abc"));

    for (uint32_t i = 0U; i < 10U; i++) { TEST_ASSERT_TRUE(fake_timer_tick()); }
    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.total_bytes_transmitted);
    TEST_ASSERT_TRUE(fake_timer_tick());                                             /* tick 11 = first stop bit */
    TEST_ASSERT_EQUAL_UINT32(1U, g_bb_uart_tx_ctrl.total_bytes_transmitted);
    for (uint32_t i = 0U; i < 10U; i++) { TEST_ASSERT_TRUE(fake_timer_tick()); }     /* tick 21 = second stop bit */
    TEST_ASSERT_EQUAL_UINT32(2U, g_bb_uart_tx_ctrl.total_bytes_transmitted);
}

/* The line is HIGH after the last tick (idle) */
TEST(BbUartWaveform, LineIdlesHighAfterDrain)
{
    uint8_t const zero = 0x00U;                 /* all-low data: only the stop bit brings the line back up */

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(&zero, 1U));
    uint32_t ticks = drain();

    TEST_ASSERT_EQUAL_UINT8(1U, g_fake_fsp.level_log[ticks - 1U]);
}

/* Data written while a frame is in flight is appended with NO idle gap */
TEST(BbUartWaveform, WriteDuringTransmissionAppendsWithoutGap)
{
    uint8_t  out[8];
    uint32_t starts[8];

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("AAAA"));
    for (uint32_t i = 0U; i < 25U; i++) { TEST_ASSERT_TRUE(fake_timer_tick()); }   /* mid third byte */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("BBBB"));
    (void) drain();

    TEST_ASSERT_EQUAL_INT32(8, helper_decode_frames(g_fake_fsp.level_log, g_fake_fsp.level_count, out, 8U, starts));
    TEST_ASSERT_EQUAL_UINT8_ARRAY("AAAABBBB", out, 8U);
    for (uint32_t i = 1U; i < 8U; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(10U, starts[i] - starts[i - 1U]);
    }
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.start_count);   /* one continuous run, timer started once */
}

/* Two bursts separated by idle: both decode, the timer is reset+started once per burst */
TEST(BbUartWaveform, SecondBurstAfterIdleRestartsTimerAndDecodes)
{
    uint8_t out[4];

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("AB"));
    (void) drain();
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("CD"));
    (void) drain();

    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_fsp.start_count);
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_fsp.reset_count);
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_fsp.stop_count);
    TEST_ASSERT_EQUAL_INT32(4, helper_decode_frames(g_fake_fsp.level_log, g_fake_fsp.level_count, out, 4U, NULL));
    TEST_ASSERT_EQUAL_UINT8_ARRAY("ABCD", out, 4U);
}

/* Full buffer: extra byte rejected, then after the ISR drains it everything is accepted again and nothing was lost */
TEST(BbUartWaveform, FullBufferRejectsThenAcceptsAfterDrainWithNoDataLoss)
{
    static uint8_t in[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];
    static uint8_t out[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES + 1U];

    for (uint32_t i = 0U; i < HELPER_RB_CAPACITY; i++) { in[i] = (uint8_t) (i * 11U + 1U); }

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(in, HELPER_RB_CAPACITY));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TX_BUFFER_FULL, rs_bb_uart_tx_WriteString("X"));
    (void) drain();
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("Z"));
    (void) drain();

    TEST_ASSERT_EQUAL_INT32((int32_t) (HELPER_RB_CAPACITY + 1U),
                            helper_decode_frames(g_fake_fsp.level_log, g_fake_fsp.level_count, out, sizeof(out), NULL));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, HELPER_RB_CAPACITY);
    TEST_ASSERT_EQUAL_UINT8('Z', out[HELPER_RB_CAPACITY]);
}

/* The bit-level frame structure does not depend on the baud rate (only the timer period does) */
TEST(BbUartWaveform, FrameStructureIsIndependentOfBaudRate)
{
    static uint8_t lo[256];
    uint32_t       n_lo;

    configure(RS_BB_UART_TX_CFG_MIN_BAUD_RATE);
    (void) rs_bb_uart_tx_WriteString("Baud");
    n_lo = drain();
    memcpy(lo, g_fake_fsp.level_log, n_lo);
    uint32_t period_lo = g_fake_fsp.last_period_counts;

    configure(RS_BB_UART_TX_CFG_MAX_BAUD_RATE);
    (void) rs_bb_uart_tx_WriteString("Baud");
    uint32_t n_hi = drain();

    TEST_ASSERT_EQUAL_UINT32(n_lo, n_hi);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(lo, g_fake_fsp.level_log, n_lo);
    TEST_ASSERT_TRUE(period_lo > g_fake_fsp.last_period_counts);   /* slower baud -> longer timer period */
}

/* Every pin write in an entire burst is inside a PFS access bracket (otherwise real hardware drops it) */
TEST(BbUartWaveform, EveryPinWriteIsInsideAccessBracket)
{
    (void) rs_bb_uart_tx_WriteString("bracket test");
    (void) drain();

    TEST_ASSERT_TRUE(g_fake_fsp.pin_write_count > 0U);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.pin_writes_outside_bracket);
    TEST_ASSERT_EQUAL_INT32(0, g_fake_fsp.pin_access_depth);
}

TEST_GROUP_RUNNER(BbUartWaveform)
{
    RUN_TEST_CASE(BbUartWaveform, OpenWritesNoLevelAndConfiguresIdleHigh);
    RUN_TEST_CASE(BbUartWaveform, WriteStringDecodesToSameString);
    RUN_TEST_CASE(BbUartWaveform, AllByteValuesRoundTrip);
    RUN_TEST_CASE(BbUartWaveform, BurstDurationIsExactlyTenNPlusTwoTicks);
    RUN_TEST_CASE(BbUartWaveform, LaunchLatencyIsExactlyOneIdleBitPeriod);
    RUN_TEST_CASE(BbUartWaveform, StartToStartSpacingIsExactlyTenTicks);
    RUN_TEST_CASE(BbUartWaveform, EveryFrameHasHighStopBit);
    RUN_TEST_CASE(BbUartWaveform, DrainStopsTimerReturnsToReadyAndCountersAgree);
    RUN_TEST_CASE(BbUartWaveform, TransmittedCounterIncrementsOnEachStopBit);
    RUN_TEST_CASE(BbUartWaveform, LineIdlesHighAfterDrain);
    RUN_TEST_CASE(BbUartWaveform, WriteDuringTransmissionAppendsWithoutGap);
    RUN_TEST_CASE(BbUartWaveform, SecondBurstAfterIdleRestartsTimerAndDecodes);
    RUN_TEST_CASE(BbUartWaveform, FullBufferRejectsThenAcceptsAfterDrainWithNoDataLoss);
    RUN_TEST_CASE(BbUartWaveform, FrameStructureIsIndependentOfBaudRate);
    RUN_TEST_CASE(BbUartWaveform, EveryPinWriteIsInsideAccessBracket);
}
