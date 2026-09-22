/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file test_rs_bb_uart_tx_hal.c
 * @brief Host-only unit tests for the Bit-Banged UART TX HAL shim and the
 *        GPT ISR-context callback, running the REAL rs_bb_uart_tx_hal.c
 *        against the fake FSP layer (tests/stubs/rs_bb_uart_tx_fake_fsp.h).
 *
 * Why this file is host-only
 * --------------------------
 * The HAL shim drives the TX pin with R_BSP_PinAccessEnable()/PinWrite()/...
 * which are __STATIC_INLINE in the real BSP and therefore cannot be replaced
 * on target.  On the host they are ordinary functions provided by the fake,
 * which records every call.  This executable (unittest_hal_on_pc) links the
 * real hal.c, core, engine, ring buffer and board data file; the mock-HAL
 * executable (unittest_on_pc) is the one that also runs on target.
 *
 * Test groups:
 *   BbUartHal — GPT adapter (open/start/stop/close), period computation,
 *               GPIO adapter (open/write/close), PFS access bracketing.
 *   BbUartIsr — rs_bb_uart_tx_hal_gpt_callback(): state gating, pin-write-
 *               before-advance ordering, byte accounting, timer stop.
 **********************************************************************************************************************/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity_fixture.h"

#include "rs_bb_uart_tx/rs_bb_uart_tx.h"
#include "rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h"
#include "rs_bb_uart_tx/rs_bb_uart_tx_hal.h"
#include "rs_bb_uart_tx_fake_fsp.h"
#include "rs_bb_uart_tx_test_helpers.h"
#include "r_ioport.h"

/* ── Shared fixtures ─────────────────────────────────────────────────────── */

static rs_bb_uart_tx_cfg_t g_cfg;

/** Fresh fake + singleton, cfg generated against the fake timer and bound to the
 *  singleton WITHOUT running ConfigCheck() — so HAL functions can be called one at a time. */
static void hal_fixture_bind(uint32_t baud, uint32_t clock_hz)
{
    fake_fsp_reset();
    helper_singleton_reset();
    g_fake_fsp.clock_frequency = clock_hz;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS,
                      rs_bb_uart_tx_ConfigGen(&g_cfg, &g_fake_timer_instance, BSP_IO_PORT_01_PIN_09));
    g_cfg.baud_rate = baud;
    g_bb_uart_tx_ctrl.p_cfg = &g_cfg;
}

/** Bind and bring the singleton to READY through the public API. */
static void hal_fixture_open(void)
{
    hal_fixture_bind(RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE, 48000000U);
    g_bb_uart_tx_ctrl.p_cfg = NULL;                 /* let ConfigCheck() do the binding */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
    g_fake_fsp.event_count = 0U;                    /* forget the open sequence; tests look at what follows */
    g_fake_fsp.pin_write_count = 0U;
    g_fake_fsp.level_count = 0U;
}

/** Deliver one GPT callback directly (bypasses the fake timer's running flag). */
static void isr_call(void)
{
    timer_callback_args_t args;
    memset(&args, 0, sizeof(args));
    args.p_context = &g_bb_uart_tx_ctrl;
    args.event     = TIMER_EVENT_CYCLE_END;
    rs_bb_uart_tx_hal_gpt_callback(&args);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: BbUartHal
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(BbUartHal);

TEST_SETUP(BbUartHal)
{
    hal_fixture_bind(RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE, 48000000U);
}

TEST_TEAR_DOWN(BbUartHal)
{
    helper_singleton_reset();
}

/* gpt_open: open -> infoGet -> periodSet, in that order, and it never starts the timer */
TEST(BbUartHal, GptOpenCallsOpenInfoGetPeriodSetInOrderAndDoesNotStart)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));

    TEST_ASSERT_EQUAL_UINT32(3U, g_fake_fsp.event_count);
    TEST_ASSERT_EQUAL(FAKE_EVT_TIMER_OPEN,      g_fake_fsp.event_log[0]);
    TEST_ASSERT_EQUAL(FAKE_EVT_TIMER_INFOGET,   g_fake_fsp.event_log[1]);
    TEST_ASSERT_EQUAL(FAKE_EVT_TIMER_PERIODSET, g_fake_fsp.event_log[2]);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.start_count);
    TEST_ASSERT_FALSE(g_fake_fsp.running);
}

/* period_counts = round(clock / baud) - 1 for representative board clocks (RA2E3 48 MHz, RA4E2/RA6E2 25 MHz) */
TEST(BbUartHal, GptOpenPeriodCountsMatchDocumentedFormula)
{
    static struct { uint32_t clock; uint32_t baud; uint32_t expect; } const cases[] =
    {
        { 48000000U, 115200U,   416U },   /* 416.67 -> 417 -> 416 */
        { 48000000U,   9600U,  4999U },   /* 5000 exact -> 4999   */
        { 48000000U,   1200U, 39999U },   /* 40000 exact -> 39999 */
        { 25000000U, 115200U,   216U },   /* 217.01 -> 217 -> 216 */
        { 25000000U,   9600U,  2603U },   /* 2604.17 -> 2604 -> 2603 */
        { 25000000U,   1200U, 20832U },   /* 20833.3 -> 20833 -> 20832 */
    };

    for (uint32_t i = 0U; i < (sizeof(cases) / sizeof(cases[0])); i++)
    {
        hal_fixture_bind(cases[i].baud, cases[i].clock);
        TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));
        TEST_ASSERT_EQUAL_UINT32(cases[i].expect, g_fake_fsp.last_period_counts);
    }
}

/* The quotient rounds to nearest: exactly halfway rounds UP ... */
TEST(BbUartHal, GptOpenRoundsHalfwayUp)
{
    hal_fixture_bind(100U, 250U);            /* 2.5 -> 3 -> 2 */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_fsp.last_period_counts);
}

/* ... and just below halfway rounds DOWN */
TEST(BbUartHal, GptOpenRoundsJustBelowHalfwayDown)
{
    hal_fixture_bind(100U, 249U);            /* 2.49 -> 2 -> 1 */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.last_period_counts);
}

/* Timing accuracy (jitter spec: average baud error <= +/-1%) across the supported baud range and both board clocks.
 * r_gpt.c programs GTPR = period_counts - 1, so the real timer period is exactly period_counts clock cycles. */
TEST(BbUartHal, BaudErrorWithinOnePercentAcrossSupportedRange)
{
    static uint32_t const clocks[] = { 48000000U, 25000000U };
    static uint32_t const bauds[]  = { 1200U, 2400U, 4800U, 9600U, 19200U, 38400U, 57600U, 115200U };

    for (uint32_t c = 0U; c < (sizeof(clocks) / sizeof(clocks[0])); c++)
    {
        for (uint32_t b = 0U; b < (sizeof(bauds) / sizeof(bauds[0])); b++)
        {
            hal_fixture_bind(bauds[b], clocks[c]);
            TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));

            /* achieved baud = clock / period_counts; error = |clock - baud*period| / (baud*period) */
            uint64_t period   = g_fake_fsp.last_period_counts;
            uint64_t ideal    = (uint64_t) bauds[b] * period;
            uint64_t diff     = (clocks[c] > ideal) ? (clocks[c] - ideal) : (ideal - clocks[c]);

            TEST_ASSERT_TRUE_MESSAGE((diff * 1000U) <= (10U * ideal), "baud error exceeds 1%");
            TEST_ASSERT_TRUE_MESSAGE((diff * 1000U) <= ((uint64_t) RS_BB_UART_TX_CFG_BAUD_ERROR_PPT_MAX * ideal),
                                     "baud error exceeds the module's own BAUD_ERROR_PPT_MAX");
        }
    }
}

/* ConfigCheck() programs the period for the CONFIGURED baud (full public path, fake clock 48 MHz) */
TEST(BbUartHal, ConfigCheckProgramsPeriodForConfiguredBaud)
{
    hal_fixture_bind(9600U, 48000000U);
    g_bb_uart_tx_ctrl.p_cfg = NULL;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL_UINT32(4999U, g_fake_fsp.last_period_counts);
}

/* open() failure: TIMER_ERROR, nothing further is called, and no close() (it never opened) */
TEST(BbUartHal, GptOpenFailureReturnsTimerErrorWithoutFurtherCalls)
{
    g_fake_fsp.open_ret = FSP_ERR_IN_USE;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.info_get_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.period_set_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.close_count);
}

/* infoGet() failure: TIMER_ERROR and the already-open timer is closed again */
TEST(BbUartHal, GptOpenInfoGetFailureClosesTimer)
{
    g_fake_fsp.info_get_ret = FSP_ERR_NOT_OPEN;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.close_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.period_set_count);
}

/* periodSet() failure: TIMER_ERROR and the timer is closed again */
TEST(BbUartHal, GptOpenPeriodSetFailureClosesTimer)
{
    g_fake_fsp.period_set_ret = FSP_ERR_INVALID_ARGUMENT;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.close_count);
}

/* baud_rate == 0 would divide by zero: reported as UNSUPPORTED_BAUD_RATE, timer closed, no periodSet */
TEST(BbUartHal, GptOpenZeroBaudReturnsUnsupportedAndClosesTimer)
{
    g_cfg.baud_rate = 0U;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE, rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.close_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.period_set_count);
}

/* gpt_start: reset THEN start, so every frame begins from counter 0 */
TEST(BbUartHal, GptStartResetsThenStarts)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_start(&g_bb_uart_tx_ctrl));

    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_fsp.event_count);
    TEST_ASSERT_EQUAL(FAKE_EVT_TIMER_RESET, g_fake_fsp.event_log[0]);
    TEST_ASSERT_EQUAL(FAKE_EVT_TIMER_START, g_fake_fsp.event_log[1]);
    TEST_ASSERT_TRUE(g_fake_fsp.running);
}

/* reset() failure: TIMER_ERROR and the timer is NOT started */
TEST(BbUartHal, GptStartResetFailureDoesNotStart)
{
    g_fake_fsp.reset_ret = FSP_ERR_NOT_OPEN;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_hal_gpt_start(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.start_count);
    TEST_ASSERT_FALSE(g_fake_fsp.running);
}

/* start() failure -> TIMER_ERROR */
TEST(BbUartHal, GptStartFailureReturnsTimerError)
{
    g_fake_fsp.start_ret = FSP_ERR_IN_USE;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_hal_gpt_start(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_FALSE(g_fake_fsp.running);
}

/* gpt_stop stops the timer; a driver failure is reported */
TEST(BbUartHal, GptStopStopsTimerAndReportsFailure)
{
    (void) rs_bb_uart_tx_hal_gpt_start(&g_bb_uart_tx_ctrl);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_stop(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_FALSE(g_fake_fsp.running);

    g_fake_fsp.stop_ret = FSP_ERR_NOT_OPEN;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_hal_gpt_stop(&g_bb_uart_tx_ctrl));
}

/* gpt_close closes the driver; a driver failure is reported */
TEST(BbUartHal, GptCloseClosesTimerAndReportsFailure)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpt_close(&g_bb_uart_tx_ctrl));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.close_count);

    g_fake_fsp.close_ret = FSP_ERR_NOT_OPEN;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_hal_gpt_close(&g_bb_uart_tx_ctrl));
}

/* gpio_open: configures the configured pin as output-high, all inside one PFS access bracket */
TEST(BbUartHal, GpioOpenConfiguresOutputHighInsideAccessBracket)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpio_open(&g_bb_uart_tx_ctrl));

    TEST_ASSERT_EQUAL_UINT32(3U, g_fake_fsp.event_count);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_ENABLE,  g_fake_fsp.event_log[0]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_CFG,            g_fake_fsp.event_log[1]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_DISABLE, g_fake_fsp.event_log[2]);
    TEST_ASSERT_EQUAL(BSP_IO_PORT_01_PIN_09, g_fake_fsp.pin_cfg_pin);
    TEST_ASSERT_EQUAL_HEX32((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_HIGH,
                            g_fake_fsp.pin_cfg_value);
    TEST_ASSERT_EQUAL_INT32(0, g_fake_fsp.pin_access_depth);
}

/* gpio_open never writes the pin level itself (idle-high comes from the config flag) */
TEST(BbUartHal, GpioOpenDoesNotWriteLevel)
{
    (void) rs_bb_uart_tx_hal_gpio_open(&g_bb_uart_tx_ctrl);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.pin_write_count);
}

/* gpio_write(high) and gpio_write(low): each write is bracketed by Enable/Disable (else PWPR silently drops it) */
TEST(BbUartHal, GpioWriteBracketsEveryWriteWithAccessEnableDisable)
{
    rs_bb_uart_tx_hal_gpio_write(&g_bb_uart_tx_ctrl, true);
    rs_bb_uart_tx_hal_gpio_write(&g_bb_uart_tx_ctrl, false);

    TEST_ASSERT_EQUAL_UINT32(6U, g_fake_fsp.event_count);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_ENABLE,  g_fake_fsp.event_log[0]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_WRITE_HIGH,     g_fake_fsp.event_log[1]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_DISABLE, g_fake_fsp.event_log[2]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_ENABLE,  g_fake_fsp.event_log[3]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_WRITE_LOW,      g_fake_fsp.event_log[4]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_DISABLE, g_fake_fsp.event_log[5]);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.pin_writes_outside_bracket);
    TEST_ASSERT_EQUAL(BSP_IO_PORT_01_PIN_09, g_fake_fsp.pin_last_write_pin);
}

/* gpio_close leaves the pin HIGH (never parks the line low), inside a bracket */
TEST(BbUartHal, GpioCloseDrivesPinHighInsideBracket)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_hal_gpio_close(&g_bb_uart_tx_ctrl));

    TEST_ASSERT_EQUAL_UINT32(3U, g_fake_fsp.event_count);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_ENABLE,  g_fake_fsp.event_log[0]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_WRITE_HIGH,     g_fake_fsp.event_log[1]);
    TEST_ASSERT_EQUAL(FAKE_EVT_PIN_ACCESS_DISABLE, g_fake_fsp.event_log[2]);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.pin_writes_outside_bracket);
}

/* Whole open path via the public API: GPIO is configured before the timer is opened */
TEST(BbUartHal, ConfigCheckOpensGpioBeforeTimer)
{
    g_bb_uart_tx_ctrl.p_cfg = NULL;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));

    uint32_t i_cfg  = fake_fsp_event_find(FAKE_EVT_PIN_CFG, 0U);
    uint32_t i_open = fake_fsp_event_find(FAKE_EVT_TIMER_OPEN, 0U);
    TEST_ASSERT_TRUE(i_cfg  != UINT32_MAX);
    TEST_ASSERT_TRUE(i_open != UINT32_MAX);
    TEST_ASSERT_TRUE(i_cfg < i_open);
    TEST_ASSERT_FALSE(g_fake_fsp.running);   /* opened, not started */
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartHal)
{
    RUN_TEST_CASE(BbUartHal, GptOpenCallsOpenInfoGetPeriodSetInOrderAndDoesNotStart);
    RUN_TEST_CASE(BbUartHal, GptOpenPeriodCountsMatchDocumentedFormula);
    RUN_TEST_CASE(BbUartHal, GptOpenRoundsHalfwayUp);
    RUN_TEST_CASE(BbUartHal, GptOpenRoundsJustBelowHalfwayDown);
    RUN_TEST_CASE(BbUartHal, BaudErrorWithinOnePercentAcrossSupportedRange);
    RUN_TEST_CASE(BbUartHal, ConfigCheckProgramsPeriodForConfiguredBaud);
    RUN_TEST_CASE(BbUartHal, GptOpenFailureReturnsTimerErrorWithoutFurtherCalls);
    RUN_TEST_CASE(BbUartHal, GptOpenInfoGetFailureClosesTimer);
    RUN_TEST_CASE(BbUartHal, GptOpenPeriodSetFailureClosesTimer);
    RUN_TEST_CASE(BbUartHal, GptOpenZeroBaudReturnsUnsupportedAndClosesTimer);
    RUN_TEST_CASE(BbUartHal, GptStartResetsThenStarts);
    RUN_TEST_CASE(BbUartHal, GptStartResetFailureDoesNotStart);
    RUN_TEST_CASE(BbUartHal, GptStartFailureReturnsTimerError);
    RUN_TEST_CASE(BbUartHal, GptStopStopsTimerAndReportsFailure);
    RUN_TEST_CASE(BbUartHal, GptCloseClosesTimerAndReportsFailure);
    RUN_TEST_CASE(BbUartHal, GpioOpenConfiguresOutputHighInsideAccessBracket);
    RUN_TEST_CASE(BbUartHal, GpioOpenDoesNotWriteLevel);
    RUN_TEST_CASE(BbUartHal, GpioWriteBracketsEveryWriteWithAccessEnableDisable);
    RUN_TEST_CASE(BbUartHal, GpioCloseDrivesPinHighInsideBracket);
    RUN_TEST_CASE(BbUartHal, ConfigCheckOpensGpioBeforeTimer);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: BbUartIsr
 * Exercises rs_bb_uart_tx_hal_gpt_callback() one tick at a time.
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(BbUartIsr);

TEST_SETUP(BbUartIsr)
{
    hal_fixture_open();
}

TEST_TEAR_DOWN(BbUartIsr)
{
    helper_singleton_reset();
}

/* Spurious tick in any state other than TRANSMITTING: no pin write, no engine change, no counters */
TEST(BbUartIsr, CallbackIgnoredWhenNotTransmitting)
{
    static rs_bb_uart_tx_lifecycle_state_t const states[] =
    {
        RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED, RS_BB_UART_TX_LIFECYCLE_CONFIGURED,
        RS_BB_UART_TX_LIFECYCLE_READY,        RS_BB_UART_TX_LIFECYCLE_ERROR,
    };

    for (uint32_t i = 0U; i < (sizeof(states) / sizeof(states[0])); i++)
    {
        rs_bb_uart_tx_engine_state_t before;

        g_bb_uart_tx_ctrl.state = states[i];
        memcpy(&before, &g_bb_uart_tx_ctrl.engine, sizeof(before));

        isr_call();

        TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.pin_write_count);
        TEST_ASSERT_EQUAL_MEMORY(&before, &g_bb_uart_tx_ctrl.engine, sizeof(before));
        TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.total_bytes_transmitted);
        TEST_ASSERT_EQUAL(states[i], g_bb_uart_tx_ctrl.state);
    }
}

/* The level written is the one decided by the PREVIOUS tick; the engine then advances for the NEXT tick */
TEST(BbUartIsr, CallbackWritesPreviouslyDecidedLevelThenAdvances)
{
    g_bb_uart_tx_ctrl.state                    = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    g_bb_uart_tx_ctrl.engine.state             = RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT;
    g_bb_uart_tx_ctrl.engine.current_byte      = 0x01U;
    g_bb_uart_tx_ctrl.engine.next_output_high  = false;     /* start bit is what this tick must display */

    isr_call();

    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.level_count);
    TEST_ASSERT_EQUAL_UINT8(0U, g_fake_fsp.level_log[0]);   /* start bit shown */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS, g_bb_uart_tx_ctrl.engine.state);
    TEST_ASSERT_TRUE(g_bb_uart_tx_ctrl.engine.next_output_high);   /* D0 of 0x01 armed for the next tick */
}

/* Ordering: the pin is written BEFORE the engine touches the ring buffer / stops the timer */
TEST(BbUartIsr, CallbackWritesPinBeforeStoppingTimer)
{
    g_bb_uart_tx_ctrl.state                   = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    g_bb_uart_tx_ctrl.engine.state            = RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE;
    g_bb_uart_tx_ctrl.engine.next_output_high = true;
    g_fake_fsp.running = true;

    isr_call();

    uint32_t i_write = fake_fsp_event_find(FAKE_EVT_PIN_WRITE_HIGH, 0U);
    uint32_t i_stop  = fake_fsp_event_find(FAKE_EVT_TIMER_STOP, 0U);
    TEST_ASSERT_TRUE(i_write != UINT32_MAX);
    TEST_ASSERT_TRUE(i_stop  != UINT32_MAX);
    TEST_ASSERT_TRUE(i_write < i_stop);
}

/* Exactly one pin write per tick, always inside a PFS access bracket */
TEST(BbUartIsr, CallbackWritesExactlyOnePinLevelPerTickInsideBracket)
{
    g_bb_uart_tx_ctrl.state = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_bb_uart_tx_ctrl.ring_buffer, (uint8_t const *) "AB", 2U));

    for (uint32_t i = 1U; i <= 5U; i++)
    {
        isr_call();
        TEST_ASSERT_EQUAL_UINT32(i, g_fake_fsp.pin_write_count);
    }
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.pin_writes_outside_bracket);
    TEST_ASSERT_EQUAL_UINT32(g_fake_fsp.pin_access_enable_count, g_fake_fsp.pin_access_disable_count);
}

/* total_bytes_transmitted increments on the stop-bit tick only */
TEST(BbUartIsr, CallbackCountsByteOnStopBitTick)
{
    g_bb_uart_tx_ctrl.state        = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    g_bb_uart_tx_ctrl.engine.state = RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS;
    g_bb_uart_tx_ctrl.engine.bit_index = 6U;
    g_bb_uart_tx_ctrl.engine.next_output_high = false;

    isr_call();                                              /* shows D6, arms D7 */
    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.total_bytes_transmitted);
    isr_call();                                              /* shows D7, arms stop bit */
    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.total_bytes_transmitted);
    isr_call();                                              /* shows the STOP bit -> byte complete */
    TEST_ASSERT_EQUAL_UINT32(1U, g_bb_uart_tx_ctrl.total_bytes_transmitted);
}

/* Queue freshly empty: timer is stopped and the lifecycle returns to READY */
TEST(BbUartIsr, CallbackStopsTimerAndReturnsToReadyWhenQueueEmpty)
{
    g_bb_uart_tx_ctrl.state        = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    g_bb_uart_tx_ctrl.engine.state = RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE;
    g_fake_fsp.running = true;

    isr_call();

    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.stop_count);
    TEST_ASSERT_FALSE(g_fake_fsp.running);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
}

/* A byte that arrived in the NEXT_BYTE window keeps the timer running and the state TRANSMITTING */
TEST(BbUartIsr, CallbackKeepsTimerRunningWhileBytesRemain)
{
    g_bb_uart_tx_ctrl.state        = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    g_bb_uart_tx_ctrl.engine.state = RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE;
    g_fake_fsp.running = true;
    TEST_ASSERT_TRUE(rs_bb_uart_tx_ring_buffer_enqueue(&g_bb_uart_tx_ctrl.ring_buffer, (uint8_t const *) "Z", 1U));

    isr_call();

    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.stop_count);
    TEST_ASSERT_TRUE(g_fake_fsp.running);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_TRANSMITTING, g_bb_uart_tx_ctrl.state);
}

/* Characterisation: a timer stop() failure inside the ISR is ignored and the state still becomes READY */
TEST(BbUartIsr, CallbackReturnsToReadyEvenIfTimerStopFails)
{
    g_bb_uart_tx_ctrl.state        = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    g_bb_uart_tx_ctrl.engine.state = RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE;
    g_fake_fsp.running  = true;
    g_fake_fsp.stop_ret = FSP_ERR_NOT_OPEN;

    isr_call();

    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.stop_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
}

/* The fake timer delivers ticks only while running, using the cfg's callback and context */
TEST(BbUartIsr, FakeTimerDeliversTicksOnlyWhileRunning)
{
    TEST_ASSERT_FALSE(fake_timer_tick());                    /* not started -> no ISR */
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_fsp.tick_count);

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("x"));
    TEST_ASSERT_TRUE(fake_timer_tick());
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.tick_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_fsp.pin_write_count);   /* the SBB callback ran and wrote the pin */
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartIsr)
{
    RUN_TEST_CASE(BbUartIsr, CallbackIgnoredWhenNotTransmitting);
    RUN_TEST_CASE(BbUartIsr, CallbackWritesPreviouslyDecidedLevelThenAdvances);
    RUN_TEST_CASE(BbUartIsr, CallbackWritesPinBeforeStoppingTimer);
    RUN_TEST_CASE(BbUartIsr, CallbackWritesExactlyOnePinLevelPerTickInsideBracket);
    RUN_TEST_CASE(BbUartIsr, CallbackCountsByteOnStopBitTick);
    RUN_TEST_CASE(BbUartIsr, CallbackStopsTimerAndReturnsToReadyWhenQueueEmpty);
    RUN_TEST_CASE(BbUartIsr, CallbackKeepsTimerRunningWhileBytesRemain);
    RUN_TEST_CASE(BbUartIsr, CallbackReturnsToReadyEvenIfTimerStopFails);
    RUN_TEST_CASE(BbUartIsr, FakeTimerDeliversTicksOnlyWhileRunning);
}
