/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file test_rs_bb_uart_tx_core.c
 * @brief Unit tests for the Bit-Banged UART TX configuration bridge, internal
 *        lifecycle, and Write()/WriteString()/StatusGet(), using a lightweight
 *        mock HAL shim.
 *
 * Strategy
 * --------
 * rs_bb_uart_tx.c reaches hardware only through the rs_bb_uart_tx_hal_*
 * functions (rs_bb_uart_tx_hal.h).  This file links the REAL core
 * (rs_bb_uart_tx.c), ring buffer, engine and board data file
 * (ConfigGen/ConfigCheck), but does NOT link rs_bb_uart_tx_hal.c.  Instead a
 * mock HAL shim (below the helpers) provides every rs_bb_uart_tx_hal_*
 * symbol.  It records call counts and call order and lets tests inject a
 * failure status per function.
 *
 * Because the mock owns rs_bb_uart_tx_hal_gpt_callback() (a no-op here),
 * nothing "drains" the queue in this file: tests observe what Write() put in
 * the ring buffer by dequeuing it directly.  Real ISR behaviour (pin writes,
 * timer stop, waveform) is covered by the host-only HAL/waveform tests, which
 * link the real hal.c against a fake FSP layer instead.
 *
 * The same file is compiled into the on-target Unity image, where the mock
 * again replaces hal.c.  It therefore uses only real-FSP-compatible types.
 *
 * The module is a singleton with no public Close(); every fixture restores
 * g_bb_uart_tx_ctrl to its as-linked state with helper_singleton_reset().
 *
 * Test groups:
 *   BbUartConfig    — ConfigGen / ConfigCheck / ConfigGetSize.
 *   BbUartLifecycle — UNCONFIGURED -> READY -> TRANSMITTING, ERROR handling.
 *   BbUartWrite     — Write / WriteString / StatusGet, all-or-nothing policy,
 *                     diagnostics counters.
 **********************************************************************************************************************/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity_fixture.h"

#include "rs_bb_uart_tx/rs_bb_uart_tx.h"
#include "rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h"
#include "rs_bb_uart_tx/rs_bb_uart_tx_hal.h"
#include "rs_bb_uart_tx_test_helpers.h"

/* ── Mock HAL shim ───────────────────────────────────────────────────────── */
/*
 * These implementations replace rs_bb_uart_tx_hal.c in this executable (see
 * tests/meson.build — hal.c is excluded).  Every function appends to a call
 * log so tests can verify ordering, and returns an injectable status.
 */

typedef enum
{
    MOCK_GPIO_OPEN = 1,
    MOCK_GPT_OPEN,
    MOCK_GPT_START,
    MOCK_GPT_STOP,
    MOCK_GPT_CLOSE,
    MOCK_GPIO_CLOSE,
    MOCK_GPIO_WRITE,
} mock_call_t;

#define MOCK_LOG_MAX   (32U)

static mock_call_t            mock_log[MOCK_LOG_MAX];
static uint32_t               mock_log_n;
static uint32_t               mock_gpio_open_count;
static uint32_t               mock_gpt_open_count;
static uint32_t               mock_gpt_start_count;
static uint32_t               mock_gpt_stop_count;
static rs_bb_uart_tx_status_t mock_gpio_open_ret;
static rs_bb_uart_tx_status_t mock_gpt_open_ret;
static rs_bb_uart_tx_status_t mock_gpt_start_ret;
static bool                   mock_cfg_bound_at_gpio_open;
static bool                   mock_cfg_bound_at_gpt_open;

static void mock_note(mock_call_t c)
{
    if (mock_log_n < MOCK_LOG_MAX)
    {
        mock_log[mock_log_n] = c;
    }
    mock_log_n++;
}

static void mock_hal_reset(void)
{
    memset(mock_log, 0, sizeof(mock_log));
    mock_log_n                  = 0U;
    mock_gpio_open_count        = 0U;
    mock_gpt_open_count         = 0U;
    mock_gpt_start_count        = 0U;
    mock_gpt_stop_count         = 0U;
    mock_gpio_open_ret          = RS_BB_UART_TX_STATUS_SUCCESS;
    mock_gpt_open_ret           = RS_BB_UART_TX_STATUS_SUCCESS;
    mock_gpt_start_ret          = RS_BB_UART_TX_STATUS_SUCCESS;
    mock_cfg_bound_at_gpio_open = false;
    mock_cfg_bound_at_gpt_open  = false;
}

/* Implementations compiled in place of rs_bb_uart_tx_hal.c */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpio_open(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    mock_note(MOCK_GPIO_OPEN);
    mock_gpio_open_count++;
    mock_cfg_bound_at_gpio_open = (NULL != p_ctrl->p_cfg);
    return mock_gpio_open_ret;
}

rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_open(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    mock_note(MOCK_GPT_OPEN);
    mock_gpt_open_count++;
    mock_cfg_bound_at_gpt_open = (NULL != p_ctrl->p_cfg);
    return mock_gpt_open_ret;
}

rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_start(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    (void) p_ctrl;
    mock_note(MOCK_GPT_START);
    mock_gpt_start_count++;
    return mock_gpt_start_ret;
}

rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_stop(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    (void) p_ctrl;
    mock_note(MOCK_GPT_STOP);
    mock_gpt_stop_count++;
    return RS_BB_UART_TX_STATUS_SUCCESS;
}

rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_close(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    (void) p_ctrl;
    mock_note(MOCK_GPT_CLOSE);
    return RS_BB_UART_TX_STATUS_SUCCESS;
}

rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpio_close(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    (void) p_ctrl;
    mock_note(MOCK_GPIO_CLOSE);
    return RS_BB_UART_TX_STATUS_SUCCESS;
}

void rs_bb_uart_tx_hal_gpio_write(rs_bb_uart_tx_ctrl_t * p_ctrl, bool level_high)
{
    (void) p_ctrl;
    (void) level_high;
    mock_note(MOCK_GPIO_WRITE);
}

/* Referenced by the FSP-Configurator-generated hal_data.c and (when
 * RS_BB_UART_TX_CFG_PRESET_CONFIG_USE == 1) by the board data file.  Never
 * invoked in this executable: no timer runs. */
void rs_bb_uart_tx_hal_gpt_callback(timer_callback_args_t * p_args)
{
    (void) p_args;
}

/* ── Shared test fixtures ────────────────────────────────────────────────── */

#define SIZE   ((uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES)

/* Dummy GPT instance — only its address matters: ConfigGen()/ConfigCheck()
 * check NULL != p_gpt_instance, and the mock HAL never dereferences it. */
static timer_instance_t const g_dummy_timer = {0};

/* The singleton retains a POINTER to the cfg (not a copy), so it must have
 * static storage duration. */
static rs_bb_uart_tx_cfg_t g_cfg;

/** ConfigGen() with the dummy timer and the RA2E3 TX pin. */
static void helper_config_gen(void)
{
    memset(&g_cfg, 0xEE, sizeof(g_cfg));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS,
                      rs_bb_uart_tx_ConfigGen(&g_cfg, &g_dummy_timer, BSP_IO_PORT_01_PIN_09));
}

/** Bring the singleton to READY through the public API. */
static void helper_open_default(void)
{
    helper_config_gen();
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
}

/** Dequeue everything Write() put in the ring buffer (stands in for the ISR consumer). */
static uint32_t helper_drain(uint8_t * p_out, uint32_t max)
{
    uint32_t n = 0U;
    while ((n < max) && rs_bb_uart_tx_ring_buffer_dequeue(&g_bb_uart_tx_ctrl.ring_buffer, &p_out[n]))
    {
        n++;
    }
    return n;
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: BbUartConfig
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(BbUartConfig);

TEST_SETUP(BbUartConfig)
{
    mock_hal_reset();
    helper_singleton_reset();
}

TEST_TEAR_DOWN(BbUartConfig)
{
    helper_singleton_reset();
}

/* ConfigGen fills every field with the documented Phase-1 defaults */
TEST(BbUartConfig, ConfigGenPopulatesPhase1Defaults)
{
    helper_config_gen();

    TEST_ASSERT_EQUAL_UINT32(RS_BB_UART_TX_VERSION, g_cfg.version);
    TEST_ASSERT_EQUAL_UINT32(sizeof(rs_bb_uart_tx_cfg_t), g_cfg.size);
    TEST_ASSERT_EQUAL_UINT32(RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE, g_cfg.baud_rate);
    TEST_ASSERT_EQUAL_UINT32(RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES, g_cfg.buffer_size);
    TEST_ASSERT_EQUAL_UINT8(8U, g_cfg.data_bits);
    TEST_ASSERT_EQUAL_UINT8(0U, g_cfg.parity);
    TEST_ASSERT_EQUAL_UINT8(1U, g_cfg.stop_bits);
    TEST_ASSERT_EQUAL_UINT8(RS_BB_UART_TX_CFG_OVERFLOW_POLICY_RETURN_ERROR, g_cfg.overflow_policy);
    TEST_ASSERT_EQUAL_PTR(&g_dummy_timer, g_cfg.p_gpt_instance);
    TEST_ASSERT_NULL(g_cfg.p_gpio_instance);
    TEST_ASSERT_EQUAL(BSP_IO_PORT_01_PIN_09, g_cfg.tx_pin);
}

/* ConfigGen overwrites whatever was in the struct (helper pre-fills 0xEE): no stale field survives */
TEST(BbUartConfig, ConfigGenOverwritesPriorContents)
{
    helper_config_gen();   /* pre-fills with 0xEE, then ConfigGen */

    TEST_ASSERT_EQUAL_UINT8(0U, g_cfg.parity);
    TEST_ASSERT_NULL(g_cfg.p_gpio_instance);
    TEST_ASSERT_EQUAL_UINT32(RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE, g_cfg.baud_rate);
}

/* ConfigGen touches only the cfg struct — never the singleton or the HAL */
TEST(BbUartConfig, ConfigGenDoesNotTouchSingletonOrHal)
{
    helper_config_gen();

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED, g_bb_uart_tx_ctrl.state);
    TEST_ASSERT_NULL(g_bb_uart_tx_ctrl.p_cfg);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_log_n);
}

#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
/* ConfigGen(NULL cfg) -> INVALID_POINTER */
TEST(BbUartConfig, ConfigGenNullCfgReturnsInvalidPointer)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER,
                      rs_bb_uart_tx_ConfigGen(NULL, &g_dummy_timer, BSP_IO_PORT_01_PIN_09));
}

/* ConfigGen(NULL timer) -> INVALID_POINTER, and the cfg is left untouched */
TEST(BbUartConfig, ConfigGenNullTimerReturnsInvalidPointer)
{
    memset(&g_cfg, 0xEE, sizeof(g_cfg));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER,
                      rs_bb_uart_tx_ConfigGen(&g_cfg, NULL, BSP_IO_PORT_01_PIN_09));
    TEST_ASSERT_EQUAL_HEX8(0xEEU, ((uint8_t *) &g_cfg)[0]);
}

/* ConfigCheck(NULL) -> INVALID_POINTER; nothing opened */
TEST(BbUartConfig, ConfigCheckNullCfgReturnsInvalidPointer)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER, rs_bb_uart_tx_ConfigCheck(NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_log_n);
}

/* ConfigCheck with cfg->p_gpt_instance == NULL -> INVALID_POINTER; nothing opened */
TEST(BbUartConfig, ConfigCheckNullTimerReturnsInvalidPointer)
{
    helper_config_gen();
    g_cfg.p_gpt_instance = NULL;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_log_n);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED, g_bb_uart_tx_ctrl.state);
}

/* ConfigGetSize(NULL) -> INVALID_POINTER */
TEST(BbUartConfig, ConfigGetSizeNullReturnsInvalidPointer)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER, rs_bb_uart_tx_ConfigGetSize(NULL));
}
#endif /* RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE */

/* Baud rate below the supported minimum is rejected before any HAL access */
TEST(BbUartConfig, ConfigCheckBaudBelowMinRejected)
{
    helper_config_gen();
    g_cfg.baud_rate = RS_BB_UART_TX_CFG_MIN_BAUD_RATE - 1U;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_log_n);                                    /* HAL untouched */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED, g_bb_uart_tx_ctrl.state);
}

/* Baud rate above the supported maximum is rejected before any HAL access */
TEST(BbUartConfig, ConfigCheckBaudAboveMaxRejected)
{
    helper_config_gen();
    g_cfg.baud_rate = RS_BB_UART_TX_CFG_MAX_BAUD_RATE + 1U;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_log_n);
}

/* Baud 0 is rejected (below min) */
TEST(BbUartConfig, ConfigCheckZeroBaudRejected)
{
    helper_config_gen();
    g_cfg.baud_rate = 0U;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE, rs_bb_uart_tx_ConfigCheck(&g_cfg));
}

/* Boundary: the minimum baud rate itself is accepted */
TEST(BbUartConfig, ConfigCheckMinBaudAccepted)
{
    helper_config_gen();
    g_cfg.baud_rate = RS_BB_UART_TX_CFG_MIN_BAUD_RATE;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));
}

/* Boundary: the maximum baud rate itself is accepted */
TEST(BbUartConfig, ConfigCheckMaxBaudAccepted)
{
    helper_config_gen();
    g_cfg.baud_rate = RS_BB_UART_TX_CFG_MAX_BAUD_RATE;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));
}

/* buffer_size that disagrees with the compiled buffer size -> INVALID_BUFFER_SIZE */
TEST(BbUartConfig, ConfigCheckBufferSizeMismatchRejected)
{
    helper_config_gen();
    g_cfg.buffer_size = RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES / 2U;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_BUFFER_SIZE, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_log_n);
}

/* Success: binds cfg, opens GPIO then GPT (in that order), and reaches READY */
TEST(BbUartConfig, ConfigCheckSuccessBindsCfgOpensGpioThenGptAndIsReady)
{
    helper_config_gen();
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
    TEST_ASSERT_EQUAL_PTR(&g_cfg, g_bb_uart_tx_ctrl.p_cfg);
    TEST_ASSERT_EQUAL_UINT32(2U, mock_log_n);
    TEST_ASSERT_EQUAL(MOCK_GPIO_OPEN, mock_log[0]);
    TEST_ASSERT_EQUAL(MOCK_GPT_OPEN, mock_log[1]);
    TEST_ASSERT_TRUE(mock_cfg_bound_at_gpio_open);   /* the real HAL dereferences p_ctrl->p_cfg on open */
    TEST_ASSERT_TRUE(mock_cfg_bound_at_gpt_open);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_start_count);   /* opening never starts the timer */
}

/* Open resets the ring buffer and engine, even if the RAM held stale state */
TEST(BbUartConfig, ConfigCheckResetsRingBufferAndEngine)
{
    g_bb_uart_tx_ctrl.ring_buffer.head = 100U;
    g_bb_uart_tx_ctrl.ring_buffer.tail = 40U;
    g_bb_uart_tx_ctrl.engine.state     = RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS;
    g_bb_uart_tx_ctrl.engine.next_output_high = false;

    helper_open_default();

    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.ring_buffer.head);
    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.ring_buffer.tail);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_ENGINE_STATE_TX_IDLE, g_bb_uart_tx_ctrl.engine.state);
    TEST_ASSERT_TRUE(g_bb_uart_tx_ctrl.engine.next_output_high);
}

/* A second ConfigCheck is rejected (not idempotent), counted, and does not re-open the HAL */
TEST(BbUartConfig, ConfigCheckTwiceReturnsInvalidState)
{
    helper_open_default();
    uint32_t log_before = mock_log_n;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_STATE, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL_UINT32(log_before, mock_log_n);
    TEST_ASSERT_EQUAL_UINT32(1U, g_bb_uart_tx_ctrl.invalid_call_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
}

/* GPIO open failure propagates, enters ERROR, and the timer is never opened */
TEST(BbUartConfig, ConfigCheckGpioErrorPropagatesAndEntersError)
{
    mock_gpio_open_ret = RS_BB_UART_TX_STATUS_GPIO_ERROR;
    helper_config_gen();

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_GPIO_ERROR, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_ERROR, g_bb_uart_tx_ctrl.state);
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_open_count);
}

/* GPT open failure propagates and enters ERROR */
TEST(BbUartConfig, ConfigCheckTimerErrorPropagatesAndEntersError)
{
    mock_gpt_open_ret = RS_BB_UART_TX_STATUS_TIMER_ERROR;
    helper_config_gen();

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_ERROR, g_bb_uart_tx_ctrl.state);
}

/* ERROR is terminal: ConfigCheck cannot be retried without a reset (characterises current behaviour) */
TEST(BbUartConfig, ConfigCheckAfterErrorReturnsInvalidState)
{
    mock_gpt_open_ret = RS_BB_UART_TX_STATUS_TIMER_ERROR;
    helper_config_gen();
    (void) rs_bb_uart_tx_ConfigCheck(&g_cfg);

    mock_gpt_open_ret = RS_BB_UART_TX_STATUS_SUCCESS;   /* fault cleared */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_STATE, rs_bb_uart_tx_ConfigCheck(&g_cfg));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_ERROR, g_bb_uart_tx_ctrl.state);
}

/* ConfigGetSize reports the control block size, valid before any configuration */
TEST(BbUartConfig, ConfigGetSizeReturnsControlBlockSize)
{
    size_t size = 0U;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigGetSize(&size));
    TEST_ASSERT_EQUAL_UINT32(sizeof(rs_bb_uart_tx_ctrl_t), size);
    TEST_ASSERT_EQUAL_UINT32(g_bb_uart_tx_ctrl.ctrl_size, size);
}

/* Control block honours the RAM budget guard */
TEST(BbUartConfig, ControlBlockWithinRamLimit)
{
    size_t size = 0U;

    (void) rs_bb_uart_tx_ConfigGetSize(&size);
    TEST_ASSERT_TRUE(size <= RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartConfig)
{
    RUN_TEST_CASE(BbUartConfig, ConfigGenPopulatesPhase1Defaults);
    RUN_TEST_CASE(BbUartConfig, ConfigGenOverwritesPriorContents);
    RUN_TEST_CASE(BbUartConfig, ConfigGenDoesNotTouchSingletonOrHal);
#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
    RUN_TEST_CASE(BbUartConfig, ConfigGenNullCfgReturnsInvalidPointer);
    RUN_TEST_CASE(BbUartConfig, ConfigGenNullTimerReturnsInvalidPointer);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckNullCfgReturnsInvalidPointer);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckNullTimerReturnsInvalidPointer);
    RUN_TEST_CASE(BbUartConfig, ConfigGetSizeNullReturnsInvalidPointer);
#endif
    RUN_TEST_CASE(BbUartConfig, ConfigCheckBaudBelowMinRejected);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckBaudAboveMaxRejected);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckZeroBaudRejected);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckMinBaudAccepted);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckMaxBaudAccepted);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckBufferSizeMismatchRejected);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckSuccessBindsCfgOpensGpioThenGptAndIsReady);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckResetsRingBufferAndEngine);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckTwiceReturnsInvalidState);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckGpioErrorPropagatesAndEntersError);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckTimerErrorPropagatesAndEntersError);
    RUN_TEST_CASE(BbUartConfig, ConfigCheckAfterErrorReturnsInvalidState);
    RUN_TEST_CASE(BbUartConfig, ConfigGetSizeReturnsControlBlockSize);
    RUN_TEST_CASE(BbUartConfig, ControlBlockWithinRamLimit);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: BbUartLifecycle
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(BbUartLifecycle);

TEST_SETUP(BbUartLifecycle)
{
    mock_hal_reset();
    helper_singleton_reset();
}

TEST_TEAR_DOWN(BbUartLifecycle)
{
    helper_singleton_reset();
}

/* Power-on state: UNCONFIGURED with every status field zero and the full buffer free */
TEST(BbUartLifecycle, InitialStatusIsUnconfiguredAndZeroed)
{
    rs_bb_uart_tx_state_t st;

    memset(&st, 0xEE, sizeof(st));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_StatusGet(&st));

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED, st.state);
    TEST_ASSERT_EQUAL_UINT32(0U, st.baud_rate);
    TEST_ASSERT_EQUAL_UINT32(0U, st.queued_bytes);
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, st.free_bytes);
    TEST_ASSERT_EQUAL_UINT32(0U, st.total_bytes_accepted);
    TEST_ASSERT_EQUAL_UINT32(0U, st.total_bytes_transmitted);
    TEST_ASSERT_EQUAL_UINT32(0U, st.buffer_full_count);
    TEST_ASSERT_EQUAL_UINT32(0U, st.invalid_call_count);
}

/* StatusGet stamps version and structure size for compatibility checks */
TEST(BbUartLifecycle, StatusGetReportsVersionAndSize)
{
    rs_bb_uart_tx_state_t st;

    (void) rs_bb_uart_tx_StatusGet(&st);
    TEST_ASSERT_EQUAL_UINT32(RS_BB_UART_TX_VERSION, st.version);
    TEST_ASSERT_EQUAL_UINT32(sizeof(rs_bb_uart_tx_state_t), st.size);
}

/* Version macro encodes MAJOR.MINOR.PATCH */
TEST(BbUartLifecycle, VersionMacroEncodesMajorMinorPatch)
{
    TEST_ASSERT_EQUAL_HEX32((RS_BB_UART_TX_VERSION_MAJOR << 16) | (RS_BB_UART_TX_VERSION_MINOR << 8) |
                            RS_BB_UART_TX_VERSION_PATCH, RS_BB_UART_TX_VERSION);
}

#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
/* StatusGet(NULL) -> INVALID_POINTER */
TEST(BbUartLifecycle, StatusGetNullReturnsInvalidPointer)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER, rs_bb_uart_tx_StatusGet(NULL));
}
#endif

/* After ConfigCheck the status shows READY and the configured baud rate */
TEST(BbUartLifecycle, StatusAfterOpenIsReadyWithConfiguredBaud)
{
    rs_bb_uart_tx_state_t st;

    helper_config_gen();
    g_cfg.baud_rate = 9600U;
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_ConfigCheck(&g_cfg));

    (void) rs_bb_uart_tx_StatusGet(&st);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, st.state);
    TEST_ASSERT_EQUAL_UINT32(9600U, st.baud_rate);
}

/* Write before ConfigCheck -> INVALID_STATE, counted, nothing queued, no timer */
TEST(BbUartLifecycle, WriteBeforeConfigReturnsInvalidState)
{
    uint8_t b = 0x41U;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_STATE, rs_bb_uart_tx_Write(&b, 1U));
    TEST_ASSERT_EQUAL_UINT32(1U, g_bb_uart_tx_ctrl.invalid_call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, rs_bb_uart_tx_ring_buffer_count_get(&g_bb_uart_tx_ctrl.ring_buffer));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_start_count);
}

/* WriteString before ConfigCheck behaves the same */
TEST(BbUartLifecycle, WriteStringBeforeConfigReturnsInvalidState)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_STATE, rs_bb_uart_tx_WriteString("hi"));
    TEST_ASSERT_EQUAL_UINT32(1U, g_bb_uart_tx_ctrl.invalid_call_count);
}

/* Zero-length Write is documented as a no-op success in ANY state (checked before the state gate) */
TEST(BbUartLifecycle, ZeroLengthWriteIsNoOpSuccessEvenBeforeConfig)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(NULL, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.invalid_call_count);
}

/* First Write from READY starts the timer once and enters TRANSMITTING */
TEST(BbUartLifecycle, WriteFromReadyStartsTimerAndEntersTransmitting)
{
    helper_open_default();

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("A"));
    TEST_ASSERT_EQUAL_UINT32(1U, mock_gpt_start_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_TRANSMITTING, g_bb_uart_tx_ctrl.state);
}

/* Further Writes while TRANSMITTING do not restart the timer */
TEST(BbUartLifecycle, WriteWhileTransmittingDoesNotRestartTimer)
{
    helper_open_default();

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("A"));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("B"));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("C"));
    TEST_ASSERT_EQUAL_UINT32(1U, mock_gpt_start_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_TRANSMITTING, g_bb_uart_tx_ctrl.state);
}

/* Timer start failure: TIMER_ERROR returned, state -> ERROR.  Characterisation: the bytes stay queued and counted */
TEST(BbUartLifecycle, TimerStartFailureEntersErrorAndKeepsBytesQueued)
{
    rs_bb_uart_tx_state_t st;

    helper_open_default();
    mock_gpt_start_ret = RS_BB_UART_TX_STATUS_TIMER_ERROR;

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TIMER_ERROR, rs_bb_uart_tx_WriteString("A"));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_ERROR, g_bb_uart_tx_ctrl.state);

    (void) rs_bb_uart_tx_StatusGet(&st);
    TEST_ASSERT_EQUAL_UINT32(1U, st.total_bytes_accepted);
    TEST_ASSERT_EQUAL_UINT32(1U, st.queued_bytes);
}

/* ERROR state rejects further writes and counts them */
TEST(BbUartLifecycle, WriteInErrorStateReturnsInvalidState)
{
    helper_open_default();
    mock_gpt_start_ret = RS_BB_UART_TX_STATUS_TIMER_ERROR;
    (void) rs_bb_uart_tx_WriteString("A");

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_STATE, rs_bb_uart_tx_WriteString("B"));
    TEST_ASSERT_EQUAL_UINT32(1U, g_bb_uart_tx_ctrl.invalid_call_count);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartLifecycle)
{
    RUN_TEST_CASE(BbUartLifecycle, InitialStatusIsUnconfiguredAndZeroed);
    RUN_TEST_CASE(BbUartLifecycle, StatusGetReportsVersionAndSize);
    RUN_TEST_CASE(BbUartLifecycle, VersionMacroEncodesMajorMinorPatch);
#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
    RUN_TEST_CASE(BbUartLifecycle, StatusGetNullReturnsInvalidPointer);
#endif
    RUN_TEST_CASE(BbUartLifecycle, StatusAfterOpenIsReadyWithConfiguredBaud);
    RUN_TEST_CASE(BbUartLifecycle, WriteBeforeConfigReturnsInvalidState);
    RUN_TEST_CASE(BbUartLifecycle, WriteStringBeforeConfigReturnsInvalidState);
    RUN_TEST_CASE(BbUartLifecycle, ZeroLengthWriteIsNoOpSuccessEvenBeforeConfig);
    RUN_TEST_CASE(BbUartLifecycle, WriteFromReadyStartsTimerAndEntersTransmitting);
    RUN_TEST_CASE(BbUartLifecycle, WriteWhileTransmittingDoesNotRestartTimer);
    RUN_TEST_CASE(BbUartLifecycle, TimerStartFailureEntersErrorAndKeepsBytesQueued);
    RUN_TEST_CASE(BbUartLifecycle, WriteInErrorStateReturnsInvalidState);
}

/* ═════════════════════════════════════════════════════════════════════════
 * Test group: BbUartWrite
 * Write()/WriteString() enqueue policy, all-or-nothing, diagnostics counters.
 * ═════════════════════════════════════════════════════════════════════════ */

TEST_GROUP(BbUartWrite);

TEST_SETUP(BbUartWrite)
{
    mock_hal_reset();
    helper_singleton_reset();
    helper_open_default();
}

TEST_TEAR_DOWN(BbUartWrite)
{
    helper_singleton_reset();
}

/* Write enqueues the bytes verbatim and in order */
TEST(BbUartWrite, WriteEnqueuesBytesInOrder)
{
    uint8_t in[5]  = { 0x00U, 0xFFU, 0x10U, 0x80U, 0x7FU };
    uint8_t out[5];

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(in, 5U));
    TEST_ASSERT_EQUAL_UINT32(5U, helper_drain(out, 5U));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, 5U);
}

/* Binary data containing NUL bytes is not truncated by Write() */
TEST(BbUartWrite, WriteHandlesEmbeddedNulBytes)
{
    uint8_t in[4] = { 'A', 0x00U, 'B', 0x00U };
    uint8_t out[4];

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(in, 4U));
    TEST_ASSERT_EQUAL_UINT32(4U, helper_drain(out, 4U));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(in, out, 4U);
}

#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
/* NULL data with non-zero length -> INVALID_POINTER; nothing queued, timer not started */
TEST(BbUartWrite, WriteNullPointerReturnsInvalidPointer)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER, rs_bb_uart_tx_Write(NULL, 4U));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_start_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
}

/* WriteString(NULL) -> INVALID_POINTER */
TEST(BbUartWrite, WriteStringNullReturnsInvalidPointer)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_INVALID_POINTER, rs_bb_uart_tx_WriteString(NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_start_count);
}
#endif

/* Zero-length Write: success, no timer start, no counters touched */
TEST(BbUartWrite, ZeroLengthWriteIsNoOp)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(NULL, 0U));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_start_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.total_bytes_accepted);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
}

/* WriteString enqueues the characters and never the terminating NUL */
TEST(BbUartWrite, WriteStringEnqueuesCharactersWithoutTerminator)
{
    uint8_t out[16];

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString("Hi!\r\n"));
    TEST_ASSERT_EQUAL_UINT32(5U, helper_drain(out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY("Hi!\r\n", out, 5U);
}

/* Empty string is a no-op success (does not start the timer) */
TEST(BbUartWrite, WriteEmptyStringIsNoOp)
{
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString(""));
    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_start_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
}

/* WriteString(s) and Write(s, strlen(s)) queue identical bytes */
TEST(BbUartWrite, WriteStringMatchesWriteForSameBytes)
{
    char const msg[] = "identical";
    uint8_t    a[16];
    uint8_t    b[16];

    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_WriteString(msg));
    uint32_t na = helper_drain(a, sizeof(a));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write((uint8_t const *) msg, (uint32_t) strlen(msg)));
    uint32_t nb = helper_drain(b, sizeof(b));

    TEST_ASSERT_EQUAL_UINT32(na, nb);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(a, b, na);
}

/* total_bytes_accepted accumulates over successful calls */
TEST(BbUartWrite, AcceptedCounterAccumulates)
{
    rs_bb_uart_tx_state_t st;

    (void) rs_bb_uart_tx_WriteString("abc");
    (void) rs_bb_uart_tx_WriteString("de");
    (void) rs_bb_uart_tx_Write((uint8_t const *) "fghi", 4U);

    (void) rs_bb_uart_tx_StatusGet(&st);
    TEST_ASSERT_EQUAL_UINT32(9U, st.total_bytes_accepted);
    TEST_ASSERT_EQUAL_UINT32(9U, st.queued_bytes);
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY - 9U, st.free_bytes);
}

/* Exactly SIZE-1 bytes are accepted in one call ... */
TEST(BbUartWrite, WriteExactlyUsableCapacityAccepted)
{
    static uint8_t big[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];
    rs_bb_uart_tx_state_t st;

    memset(big, 0x5A, sizeof(big));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(big, HELPER_RB_CAPACITY));
    (void) rs_bb_uart_tx_StatusGet(&st);
    TEST_ASSERT_EQUAL_UINT32(0U, st.free_bytes);
    TEST_ASSERT_EQUAL_UINT32(0U, st.buffer_full_count);
}

/* ... and one more byte is rejected with TX_BUFFER_FULL, counted, and leaves everything intact */
TEST(BbUartWrite, OneByteBeyondCapacityRejectedAsBufferFull)
{
    static uint8_t big[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];
    rs_bb_uart_tx_state_t st;

    memset(big, 0x5A, sizeof(big));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(big, HELPER_RB_CAPACITY));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TX_BUFFER_FULL, rs_bb_uart_tx_Write(big, 1U));

    (void) rs_bb_uart_tx_StatusGet(&st);
    TEST_ASSERT_EQUAL_UINT32(1U, st.buffer_full_count);
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, st.total_bytes_accepted);   /* rejected bytes are not counted */
    TEST_ASSERT_EQUAL_UINT32(HELPER_RB_CAPACITY, st.queued_bytes);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_TRANSMITTING, st.state);        /* rejection is not a fault */
}

/* All-or-nothing: an oversized request leaves the existing queue byte-for-byte intact */
TEST(BbUartWrite, RejectedWriteLeavesQueuedDataIntact)
{
    static uint8_t big[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];
    uint8_t        out[8];

    (void) rs_bb_uart_tx_WriteString("KEEPME");
    memset(big, 0xCC, sizeof(big));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TX_BUFFER_FULL, rs_bb_uart_tx_Write(big, HELPER_RB_CAPACITY));   /* 6 queued -> no room */

    TEST_ASSERT_EQUAL_UINT32(6U, helper_drain(out, sizeof(out)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY("KEEPME", out, 6U);
}

/* A rejected Write from READY must not start the timer or leave READY */
TEST(BbUartWrite, BufferFullFromReadyDoesNotStartTimer)
{
    static uint8_t big[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];

    memset(big, 0x11, sizeof(big));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TX_BUFFER_FULL, rs_bb_uart_tx_Write(big, SIZE));   /* SIZE > SIZE-1 */

    TEST_ASSERT_EQUAL_UINT32(0U, mock_gpt_start_count);
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_LIFECYCLE_READY, g_bb_uart_tx_ctrl.state);
    TEST_ASSERT_EQUAL_UINT32(1U, g_bb_uart_tx_ctrl.buffer_full_count);
}

/* Draining frees space: a rejected size fits after the consumer has removed bytes */
TEST(BbUartWrite, SpaceFreedByConsumerIsUsableAgain)
{
    static uint8_t big[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];
    uint8_t        out[64];

    memset(big, 0x22, sizeof(big));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(big, HELPER_RB_CAPACITY));
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_TX_BUFFER_FULL, rs_bb_uart_tx_Write(big, 40U));

    TEST_ASSERT_EQUAL_UINT32(64U, helper_drain(out, 64U));                                   /* consumer removes 64 */
    TEST_ASSERT_EQUAL(RS_BB_UART_TX_STATUS_SUCCESS, rs_bb_uart_tx_Write(big, 40U));
}

/* Rejections increment only buffer_full_count, never invalid_call_count */
TEST(BbUartWrite, BufferFullDoesNotCountAsInvalidCall)
{
    static uint8_t big[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES];

    (void) rs_bb_uart_tx_Write(big, SIZE);
    TEST_ASSERT_EQUAL_UINT32(0U, g_bb_uart_tx_ctrl.invalid_call_count);
}

/* ── Runner ──────────────────────────────────────────────────────────────── */

TEST_GROUP_RUNNER(BbUartWrite)
{
    RUN_TEST_CASE(BbUartWrite, WriteEnqueuesBytesInOrder);
    RUN_TEST_CASE(BbUartWrite, WriteHandlesEmbeddedNulBytes);
#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
    RUN_TEST_CASE(BbUartWrite, WriteNullPointerReturnsInvalidPointer);
    RUN_TEST_CASE(BbUartWrite, WriteStringNullReturnsInvalidPointer);
#endif
    RUN_TEST_CASE(BbUartWrite, ZeroLengthWriteIsNoOp);
    RUN_TEST_CASE(BbUartWrite, WriteStringEnqueuesCharactersWithoutTerminator);
    RUN_TEST_CASE(BbUartWrite, WriteEmptyStringIsNoOp);
    RUN_TEST_CASE(BbUartWrite, WriteStringMatchesWriteForSameBytes);
    RUN_TEST_CASE(BbUartWrite, AcceptedCounterAccumulates);
    RUN_TEST_CASE(BbUartWrite, WriteExactlyUsableCapacityAccepted);
    RUN_TEST_CASE(BbUartWrite, OneByteBeyondCapacityRejectedAsBufferFull);
    RUN_TEST_CASE(BbUartWrite, RejectedWriteLeavesQueuedDataIntact);
    RUN_TEST_CASE(BbUartWrite, BufferFullFromReadyDoesNotStartTimer);
    RUN_TEST_CASE(BbUartWrite, SpaceFreedByConsumerIsUsableAgain);
    RUN_TEST_CASE(BbUartWrite, BufferFullDoesNotCountAsInvalidCall);
}
