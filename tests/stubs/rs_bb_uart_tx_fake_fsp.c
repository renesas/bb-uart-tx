/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_bb_uart_tx_fake_fsp.c
 * @brief Fake FSP timer driver and BSP pin functions for host-only tests.
 *
 * See rs_bb_uart_tx_fake_fsp.h for the role of this file.  HOST BUILD ONLY:
 * on target the real R_BSP_Pin* functions are __STATIC_INLINE and cannot be
 * replaced, which is why the tests that need this file live in the separate
 * unittest_hal_on_pc executable and are not part of the on-target image.
 **********************************************************************************************************************/

#include <string.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_hal.h>
#include "rs_bb_uart_tx_fake_fsp.h"

fake_fsp_t g_fake_fsp;

/* Callback + context captured from the cfg passed to open(), like r_gpt.c. */
static void (* g_registered_callback)(timer_callback_args_t * p_args);
static void  * g_registered_context;

/* ── Logging helpers ─────────────────────────────────────────────────────── */

static void log_event(fake_fsp_event_t evt)
{
    if (g_fake_fsp.event_count < FAKE_FSP_EVENT_LOG_MAX)
    {
        g_fake_fsp.event_log[g_fake_fsp.event_count] = evt;
    }
    g_fake_fsp.event_count++;
}

/* ── Fake timer driver (timer_api_t) ─────────────────────────────────────── */

static fsp_err_t fake_timer_open(timer_ctrl_t * const p_ctrl, timer_cfg_t const * const p_cfg)
{
    (void) p_ctrl;
    log_event(FAKE_EVT_TIMER_OPEN);
    g_fake_fsp.open_count++;
    if (FSP_SUCCESS != g_fake_fsp.open_ret)
    {
        return g_fake_fsp.open_ret;
    }
    g_registered_callback = p_cfg->p_callback;
    g_registered_context  = p_cfg->p_context;
    return FSP_SUCCESS;
}

static fsp_err_t fake_timer_info_get(timer_ctrl_t * const p_ctrl, timer_info_t * const p_info)
{
    (void) p_ctrl;
    log_event(FAKE_EVT_TIMER_INFOGET);
    g_fake_fsp.info_get_count++;
    if (FSP_SUCCESS != g_fake_fsp.info_get_ret)
    {
        return g_fake_fsp.info_get_ret;
    }
    memset(p_info, 0, sizeof(*p_info));
    p_info->clock_frequency = g_fake_fsp.clock_frequency;
    return FSP_SUCCESS;
}

static fsp_err_t fake_timer_period_set(timer_ctrl_t * const p_ctrl, uint32_t const period)
{
    (void) p_ctrl;
    log_event(FAKE_EVT_TIMER_PERIODSET);
    g_fake_fsp.period_set_count++;
    g_fake_fsp.last_period_counts = period;
    return g_fake_fsp.period_set_ret;
}

static fsp_err_t fake_timer_reset(timer_ctrl_t * const p_ctrl)
{
    (void) p_ctrl;
    log_event(FAKE_EVT_TIMER_RESET);
    g_fake_fsp.reset_count++;
    return g_fake_fsp.reset_ret;
}

static fsp_err_t fake_timer_start(timer_ctrl_t * const p_ctrl)
{
    (void) p_ctrl;
    log_event(FAKE_EVT_TIMER_START);
    g_fake_fsp.start_count++;
    if (FSP_SUCCESS == g_fake_fsp.start_ret)
    {
        g_fake_fsp.running = true;
    }
    return g_fake_fsp.start_ret;
}

static fsp_err_t fake_timer_stop(timer_ctrl_t * const p_ctrl)
{
    (void) p_ctrl;
    log_event(FAKE_EVT_TIMER_STOP);
    g_fake_fsp.stop_count++;
    if (FSP_SUCCESS == g_fake_fsp.stop_ret)
    {
        g_fake_fsp.running = false;
    }
    return g_fake_fsp.stop_ret;
}

static fsp_err_t fake_timer_close(timer_ctrl_t * const p_ctrl)
{
    (void) p_ctrl;
    log_event(FAKE_EVT_TIMER_CLOSE);
    g_fake_fsp.close_count++;
    g_fake_fsp.running = false;
    return g_fake_fsp.close_ret;
}

static timer_api_t const g_fake_timer_api =
{
    .open      = fake_timer_open,
    .start     = fake_timer_start,
    .stop      = fake_timer_stop,
    .reset     = fake_timer_reset,
    .periodSet = fake_timer_period_set,
    .infoGet   = fake_timer_info_get,
    .close     = fake_timer_close,
};

/* Mirrors g_timer0_cfg_preset in the board data file: callback = the PES ISR
 * callback, context = the singleton control block. */
static timer_cfg_t const g_fake_timer_cfg =
{
    .mode          = TIMER_MODE_PERIODIC,
    .period_counts = 0x10000U,
    .p_callback    = rs_bb_uart_tx_hal_gpt_callback,
    .p_context     = &g_bb_uart_tx_ctrl,
};

static uint32_t g_fake_timer_ctrl_dummy;

timer_instance_t const g_fake_timer_instance =
{
    .p_ctrl = &g_fake_timer_ctrl_dummy,
    .p_cfg  = &g_fake_timer_cfg,
    .p_api  = &g_fake_timer_api,
};

/* ── Fake BSP pin functions ──────────────────────────────────────────────── */

void R_BSP_PinAccessEnable(void)
{
    log_event(FAKE_EVT_PIN_ACCESS_ENABLE);
    g_fake_fsp.pin_access_enable_count++;
    g_fake_fsp.pin_access_depth++;
}

void R_BSP_PinAccessDisable(void)
{
    log_event(FAKE_EVT_PIN_ACCESS_DISABLE);
    g_fake_fsp.pin_access_disable_count++;
    g_fake_fsp.pin_access_depth--;
}

void R_BSP_PinCfg(bsp_io_port_pin_t pin, uint32_t cfg)
{
    log_event(FAKE_EVT_PIN_CFG);
    g_fake_fsp.pin_cfg_count++;
    g_fake_fsp.pin_cfg_pin   = pin;
    g_fake_fsp.pin_cfg_value = cfg;
}

void R_BSP_PinWrite(bsp_io_port_pin_t pin, bsp_io_level_t level)
{
    log_event((BSP_IO_LEVEL_HIGH == level) ? FAKE_EVT_PIN_WRITE_HIGH : FAKE_EVT_PIN_WRITE_LOW);
    g_fake_fsp.pin_write_count++;
    g_fake_fsp.pin_last_write_pin = pin;

    if (g_fake_fsp.pin_access_depth <= 0)
    {
        /* On real silicon this write is silently dropped by PWPR write-protect. */
        g_fake_fsp.pin_writes_outside_bracket++;
    }

    if (g_fake_fsp.level_count < FAKE_FSP_LEVEL_LOG_MAX)
    {
        g_fake_fsp.level_log[g_fake_fsp.level_count] = (uint8_t) ((BSP_IO_LEVEL_HIGH == level) ? 1U : 0U);
    }
    g_fake_fsp.level_count++;
}

/* ── Test-facing control functions ───────────────────────────────────────── */

void fake_fsp_reset(void)
{
    memset(&g_fake_fsp, 0, sizeof(g_fake_fsp));
    g_fake_fsp.clock_frequency = 48000000U;   /* FPB-RA2E3: undivided 48 MHz PCLKD */
    g_registered_callback      = NULL;
    g_registered_context       = NULL;
}

bool fake_timer_tick(void)
{
    if ((!g_fake_fsp.running) || (NULL == g_registered_callback))
    {
        return false;
    }

    timer_callback_args_t args;
    memset(&args, 0, sizeof(args));
    args.p_context = g_registered_context;
    args.event     = TIMER_EVENT_CYCLE_END;

    g_fake_fsp.tick_count++;
    g_registered_callback(&args);
    return true;
}

uint32_t fake_timer_run_until_stopped(uint32_t max_ticks)
{
    uint32_t n = 0U;

    while ((n < max_ticks) && fake_timer_tick())
    {
        n++;
    }
    return n;
}

uint32_t fake_fsp_event_count(fake_fsp_event_t evt)
{
    uint32_t n = 0U;
    uint32_t lim = (g_fake_fsp.event_count < FAKE_FSP_EVENT_LOG_MAX) ? g_fake_fsp.event_count : FAKE_FSP_EVENT_LOG_MAX;

    for (uint32_t i = 0U; i < lim; i++)
    {
        if (g_fake_fsp.event_log[i] == evt)
        {
            n++;
        }
    }
    return n;
}

uint32_t fake_fsp_event_find(fake_fsp_event_t evt, uint32_t from)
{
    uint32_t lim = (g_fake_fsp.event_count < FAKE_FSP_EVENT_LOG_MAX) ? g_fake_fsp.event_count : FAKE_FSP_EVENT_LOG_MAX;

    for (uint32_t i = from; i < lim; i++)
    {
        if (g_fake_fsp.event_log[i] == evt)
        {
            return i;
        }
    }
    return UINT32_MAX;
}
