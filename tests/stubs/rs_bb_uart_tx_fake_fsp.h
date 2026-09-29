/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_bb_uart_tx_fake_fsp.h
 * @brief Fake FSP layer for the host-only HAL / ISR / waveform tests.
 *
 * The Bit-Banged UART's HAL shim (rs_bb_uart_tx_hal.c) talks to hardware in
 * exactly two ways:
 *   1. a timer_instance_t function-pointer table (GPT: open/infoGet/
 *      periodSet/start/reset/stop/close), and
 *   2. four BSP pin functions (R_BSP_PinAccessEnable/Disable/PinCfg/PinWrite).
 *
 * This file fakes both, so the REAL rs_bb_uart_tx_hal.c — including the GPT
 * ISR callback that glues the TX engine to the pin — runs unmodified on the
 * host:
 *
 *   - fake_timer_tick() plays the role of the GPT ISR: while the fake timer
 *     is "running" it invokes the callback registered in the timer_cfg_t,
 *     with the p_context from that cfg, exactly as r_gpt.c does.
 *   - Every FSP-facing call is appended to one ordered event log so tests can
 *     assert sequencing (open -> infoGet -> periodSet, reset before start,
 *     pin writes inside an Enable/Disable bracket, ...).
 *   - Every pin write is also appended to a level log.  Because the SBB writes
 *     the pin exactly once per GPT tick, that log IS the TX waveform sampled
 *     once per bit period.
 *   - Return codes of every timer API are injectable for error-path tests.
 **********************************************************************************************************************/

#ifndef RS_BB_UART_TX_FAKE_FSP_H
#define RS_BB_UART_TX_FAKE_FSP_H

#include <stdint.h>
#include <stdbool.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max entries in the ordered event log / pin level log. */
#define FAKE_FSP_EVENT_LOG_MAX   (256U)
#define FAKE_FSP_LEVEL_LOG_MAX   (8192U)

typedef enum e_fake_fsp_event
{
    FAKE_EVT_TIMER_OPEN = 1,
    FAKE_EVT_TIMER_INFOGET,
    FAKE_EVT_TIMER_PERIODSET,
    FAKE_EVT_TIMER_RESET,
    FAKE_EVT_TIMER_START,
    FAKE_EVT_TIMER_STOP,
    FAKE_EVT_TIMER_CLOSE,
    FAKE_EVT_PIN_ACCESS_ENABLE,
    FAKE_EVT_PIN_ACCESS_DISABLE,
    FAKE_EVT_PIN_CFG,
    FAKE_EVT_PIN_WRITE_LOW,
    FAKE_EVT_PIN_WRITE_HIGH,
} fake_fsp_event_t;

typedef struct st_fake_fsp
{
    /* --- injectable behaviour -------------------------------------------- */
    fsp_err_t open_ret;
    fsp_err_t info_get_ret;
    fsp_err_t period_set_ret;
    fsp_err_t reset_ret;
    fsp_err_t start_ret;
    fsp_err_t stop_ret;
    fsp_err_t close_ret;
    uint32_t  clock_frequency;         ///< Value reported by infoGet().

    /* --- observed timer state -------------------------------------------- */
    uint32_t  open_count;
    uint32_t  info_get_count;
    uint32_t  period_set_count;
    uint32_t  reset_count;
    uint32_t  start_count;
    uint32_t  stop_count;
    uint32_t  close_count;
    uint32_t  last_period_counts;      ///< Argument of the last periodSet().
    bool      running;                 ///< true between a successful start() and stop().
    uint32_t  tick_count;              ///< Number of ISR callbacks delivered.

    /* --- observed pin state ---------------------------------------------- */
    uint32_t          pin_access_enable_count;
    uint32_t          pin_access_disable_count;
    int32_t           pin_access_depth;        ///< Enable increments, Disable decrements.
    uint32_t          pin_cfg_count;
    bsp_io_port_pin_t pin_cfg_pin;
    uint32_t          pin_cfg_value;
    uint32_t          pin_write_count;
    uint32_t          pin_writes_outside_bracket; ///< Writes made while access depth == 0.
    bsp_io_port_pin_t pin_last_write_pin;

    /* --- ordered logs ---------------------------------------------------- */
    fake_fsp_event_t event_log[FAKE_FSP_EVENT_LOG_MAX];
    uint32_t         event_count;
    uint8_t          level_log[FAKE_FSP_LEVEL_LOG_MAX];   ///< 1 = high, 0 = low; one entry per pin write.
    uint32_t         level_count;
} fake_fsp_t;

extern fake_fsp_t g_fake_fsp;

/** Timer instance handed to ConfigGen().  Its cfg registers
 *  rs_bb_uart_tx_hal_gpt_callback with p_context = &g_bb_uart_tx_ctrl,
 *  mirroring g_timer0_cfg_preset in the board data file. */
extern timer_instance_t const g_fake_timer_instance;

/** Reset every counter, log and injected return code; clock defaults to 48 MHz. */
void fake_fsp_reset(void);

/** Deliver one GPT cycle-end interrupt.  No-op (returns false) if the fake
 *  timer is not running or no callback was registered at open(). */
bool fake_timer_tick(void);

/** Deliver ticks until the fake timer stops or @p max_ticks is reached.
 *  @return number of ticks delivered. */
uint32_t fake_timer_run_until_stopped(uint32_t max_ticks);

/** Count entries of a given event in the ordered log. */
uint32_t fake_fsp_event_count(fake_fsp_event_t evt);

/** Index of the first occurrence of @p evt at or after @p from, or UINT32_MAX. */
uint32_t fake_fsp_event_find(fake_fsp_event_t evt, uint32_t from);

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_FAKE_FSP_H */
