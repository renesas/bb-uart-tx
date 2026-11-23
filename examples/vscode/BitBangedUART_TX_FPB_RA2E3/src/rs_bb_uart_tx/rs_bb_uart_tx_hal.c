/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_hal.c
 * @brief HAL abstraction shim implementation — GPT timer adapter, GPIO
 *        adapter, and the GPT ISR-context callback.
 *
 * See rs_bb_uart_tx_hal.h for design notes.
 *
 * Period calculation (Architecture & Design Section 14.2): rather than
 * assuming a fixed PCLKD frequency, this file queries the *actual* applied
 * timer clock frequency from the HAL driver itself via
 * p_api->infoGet()->clock_frequency immediately after open(). This is the
 * generic, driver-agnostic way to compute period_counts for any
 * timer_instance_t (GPT, AGT, ...) without hard-coding a clock tree
 * assumption here — the clock tree is the board data file's / Configurator's
 * responsibility, not this adapter's.
 *
 *   period_counts = round(clock_frequency / baud_rate) - 1
 *
 * A PCLKD/4 clock divider is used (see the board config data file) so
 * period_counts cannot overflow the RA4E2's 16-bit GPT counters
 * (BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0 — every GPT channel on this MCU
 * is 16-bit) across the full supported baud range (1200-115200 baud).
 */

#include <rs_bb_uart_tx/rs_bb_uart_tx_hal.h>

/* IOPORT_CFG_PORT_DIRECTION_OUTPUT / IOPORT_CFG_PORT_OUTPUT_HIGH (used in
 * rs_bb_uart_tx_hal_gpio_open() below) are declared in r_ioport.h, not in
 * anything rs_bb_uart_tx_hal.h's include chain (bsp_api.h / r_timer_api.h)
 * pulls in transitively. Included explicitly here rather than relying on
 * another translation unit's unrelated include chain (e.g. hal_warmstart.c
 * only happens to get r_ioport.h via hal_data.h -> common_data.h). */
#include "r_ioport.h"

/***********************************************************************************************************************
 * GPT timer adapter
 **********************************************************************************************************************/

/**
 * @brief Open and configure the GPT timer for bit-period generation.
 *
 * Opens the GPT HAL instance (`p_ctrl->p_cfg->p_gpt_instance`), queries its
 * actual applied clock frequency via `infoGet()`, computes the
 * bit-period count for the configured baud rate (rounded to nearest,
 * Architecture & Design Section 14.2), and applies it via `periodSet()`.
 * Does NOT start the timer — the timer is started/stopped on demand by
 * rs_bb_uart_tx_Write()/the GPT callback itself as the ring buffer
 * transitions between empty and non-empty (Section 10: READY <-> TRANSMITTING).
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block. `p_ctrl->p_cfg`
 *                        must already be bound (non-NULL, valid
 *                        `p_gpt_instance` and `baud_rate`).
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS                Timer opened and period configured.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR             Propagated failure from
 *                                                       the GPT HAL driver's
 *                                                       open()/infoGet()/periodSet().
 * @retval RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE   `p_ctrl->p_cfg->baud_rate` is 0.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_open(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    timer_instance_t const * p_timer = p_ctrl->p_cfg->p_gpt_instance;
    fsp_err_t                err;
    timer_info_t              info;
    uint32_t                  period_counts;

    err = p_timer->p_api->open(p_timer->p_ctrl, p_timer->p_cfg);
    if (FSP_SUCCESS != err)
    {
        return RS_BB_UART_TX_STATUS_TIMER_ERROR;
    }

    err = p_timer->p_api->infoGet(p_timer->p_ctrl, &info);
    if (FSP_SUCCESS != err)
    {
        (void) p_timer->p_api->close(p_timer->p_ctrl);
        return RS_BB_UART_TX_STATUS_TIMER_ERROR;
    }

    if (0U == p_ctrl->p_cfg->baud_rate)
    {
        (void) p_timer->p_api->close(p_timer->p_ctrl);
        return RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE;
    }

    /* Round to nearest counter value rather than truncating, to minimise
     * baud-rate error (Architecture & Design Section 5.5 / 14.2). */
    period_counts = ((info.clock_frequency + (p_ctrl->p_cfg->baud_rate / 2U))
                      / p_ctrl->p_cfg->baud_rate);
    if (period_counts > 0U)
    {
        period_counts -= 1U;
    }

    err = p_timer->p_api->periodSet(p_timer->p_ctrl, period_counts);
    if (FSP_SUCCESS != err)
    {
        (void) p_timer->p_api->close(p_timer->p_ctrl);
        return RS_BB_UART_TX_STATUS_TIMER_ERROR;
    }

    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/**
 * @brief Start the GPT timer (begin generating bit-period interrupts).
 *
 * Resets the counter to 0 before starting, so every frame's first bit
 * period is a full, consistent duration regardless of when Start() is
 * called relative to any prior count state. Called by
 * rs_bb_uart_tx_Write()/WriteString() when the ring buffer transitions
 * from empty to non-empty while READY.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS      Timer reset and started.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR  Propagated failure from the
 *                                           GPT HAL driver's reset()/start().
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_start(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    timer_instance_t const * p_timer = p_ctrl->p_cfg->p_gpt_instance;
    fsp_err_t                err;

    err = p_timer->p_api->reset(p_timer->p_ctrl); /* start each frame from count 0 for consistent first-bit timing */
    if (FSP_SUCCESS != err)
    {
        return RS_BB_UART_TX_STATUS_TIMER_ERROR;
    }

    err = p_timer->p_api->start(p_timer->p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return RS_BB_UART_TX_STATUS_TIMER_ERROR;
    }

    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/**
 * @brief Stop the GPT timer (queue has emptied; no more bits to send).
 *
 * Called from GPT ISR context (rs_bb_uart_tx_hal_gpt_callback()) once the
 * TX bit-engine returns to TX_IDLE. Safe to call from ISR context — this
 * is a simple GPT register write, not a blocking operation.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS      Timer stopped.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR  Propagated failure from the GPT HAL driver's stop().
 *
 * @note ISR-safe.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_stop(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    timer_instance_t const * p_timer = p_ctrl->p_cfg->p_gpt_instance;
    fsp_err_t                err;

    err = p_timer->p_api->stop(p_timer->p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return RS_BB_UART_TX_STATUS_TIMER_ERROR;
    }

    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/**
 * @brief Close the GPT timer and release the HAL driver instance.
 *
 * Not currently called by any Phase 1 code path (this Bit-Banged UART does not expose
 * a public Close()-equivalent — see Architecture & Design Section 26 Open
 * Items). Provided for completeness / for a future runtime-reconfiguration
 * feature to use.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS      Timer closed.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR  Propagated failure from the GPT HAL driver's close().
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_close(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    timer_instance_t const * p_timer = p_ctrl->p_cfg->p_gpt_instance;
    fsp_err_t                err;

    err = p_timer->p_api->close(p_timer->p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return RS_BB_UART_TX_STATUS_TIMER_ERROR;
    }

    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/***********************************************************************************************************************
 * GPIO adapter
 **********************************************************************************************************************/

/**
 * @brief Configure the TX pin as a push-pull digital output, idling high.
 *
 * Uses direct BSP pin control (`R_BSP_PinAccessEnable()` /
 * `R_BSP_PinCfg()` / `R_BSP_PinAccessDisable()`) rather than a full
 * `ioport_instance_t`, since a single statically-configured output pin
 * does not need the generality of the IOPORT driver's instance/table model
 * (Architecture & Design Section 16.1).
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block (`p_ctrl->p_cfg->tx_pin`
 *                        identifies the pin to configure).
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS  Pin configured and idling high (this
 *                                       direct BSP pin-control path has no
 *                                       failure mode worth reporting, so this
 *                                       function always succeeds).
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpio_open(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    /* Idle-high output, push-pull (Architecture & Design Section 16.1: "The
     * TX pin shall idle high"). Configured directly via BSP pin control
     * rather than a full ioport_instance_t — see rs_bb_uart_tx_hal.h. */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg(p_ctrl->p_cfg->tx_pin,
                 ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_HIGH));
    R_BSP_PinAccessDisable();

    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/**
 * @brief Drive the TX pin to the requested level.
 *
 * Called once per GPT ISR tick from rs_bb_uart_tx_hal_gpt_callback(). Must
 * be fast and deterministic.
 *
 * On this FSP/MCU combination, R_BSP_PinWrite() writes directly to the
 * same write-protected PmnPFS register that R_BSP_PinCfg() uses — it is
 * NOT a separate always-writable output-data register on every RA family
 * (this differs from some other Cortex-M BSPs' GPIO models). Per
 * R_BSP_PinWrite()'s own doc comment in bsp_io.h ("If PFS protection is
 * enabled, disable PFS protection using R_BSP_PinAccessEnable() before
 * calling this function"), every call must be bracketed by
 * R_BSP_PinAccessEnable()/R_BSP_PinAccessDisable() or the write is
 * silently dropped by the PWPR write-protect hardware — the pin simply
 * never changes after its initial rs_bb_uart_tx_hal_gpio_open() state.
 * R_BSP_PinAccessEnable()/Disable() are a lightweight, reference-counted,
 * critical-section-protected pair explicitly documented as safe to call
 * from ISR context ("protect against interrupts that could occur via
 * multiple threads or an ISR re-entering this code" — bsp_io.h), so the
 * per-tick overhead here is a few register writes, not a blocking
 * operation.
 *
 * @param[in] p_ctrl      Bit-Banged UART TX control block (`p_ctrl->p_cfg->tx_pin`
 *                        identifies the pin to write).
 * @param[in] level_high  true = drive high, false = drive low.
 *
 * @return None.
 *
 * @note ISR-safe.
 */
void rs_bb_uart_tx_hal_gpio_write(rs_bb_uart_tx_ctrl_t * p_ctrl, bool level_high)
{
    R_BSP_PinAccessEnable();
    R_BSP_PinWrite(p_ctrl->p_cfg->tx_pin, level_high ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW);
    R_BSP_PinAccessDisable();
}

/**
 * @brief Return the TX pin to a safe idle-high state and release pin access.
 *
 * Best-effort: leaves the pin idling high (safe/known state) rather than
 * reverting pin muxing, so a partially-transmitted line never gets left
 * low. Does not release/reconfigure the pin to its reset function — that
 * is an application-level decision outside this Bit-Banged UART's scope. Not
 * currently called by any Phase 1 code path (paired with
 * rs_bb_uart_tx_hal_gpt_close(), provided for completeness).
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS  Always (best-effort; direct pin
 *                                       control has no failure mode worth
 *                                       reporting).
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpio_close(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    /* Best-effort: leave the pin idling high (safe/known state) rather than
     * reverting pin muxing, so a partially-transmitted line never gets left
     * low. Does not release/reconfigure the pin to its reset function —
     * that is an application-level decision outside this Bit-Banged UART's scope.
     * See rs_bb_uart_tx_hal_gpio_write() for why the Enable/Disable pair
     * is required around every R_BSP_PinWrite() call on this MCU. */
    R_BSP_PinAccessEnable();
    R_BSP_PinWrite(p_ctrl->p_cfg->tx_pin, BSP_IO_LEVEL_HIGH);
    R_BSP_PinAccessDisable();
    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/***********************************************************************************************************************
 * GPT ISR-context callback
 **********************************************************************************************************************/

/**
 * @brief GPT compare-match/overflow callback — advances the TX bit-engine
 *        by exactly one bit period and drives the TX pin accordingly.
 *
 * Registered as `p_cfg->p_callback` when the GPT `timer_instance_t` is
 * opened (see `g_timer0_cfg`/`g_timer0_cfg_preset` in the board config data
 * file). `p_args->p_context` is the owning `rs_bb_uart_tx_ctrl_t` pointer.
 *
 * Per Architecture & Design Section 12.3, this function:
 *   - Clears the timer interrupt condition (handled by the FSP GPT driver
 *     itself before invoking the callback).
 *   - Reads the pin level already decided for this tick by the *previous*
 *     tick's rs_bb_uart_tx_engine_advance() call
 *     (rs_bb_uart_tx_engine_output_get() — O(1), no ring-buffer access) and
 *     writes it to the GPIO pin immediately.
 *   - Only then prepares the *next* tick's output and advances the TX state
 *     machine (rs_bb_uart_tx_engine_advance()), dequeuing the next byte
 *     from the ring buffer when required — this happens strictly after the
 *     GPIO write above, so no tick's pin write ever waits on a ring-buffer
 *     access.
 *   - Stops the GPT timer and transitions the lifecycle state back to READY
 *     when the ring buffer has been freshly confirmed empty.
 *   - Returns quickly: no allocation, no blocking, no application callback.
 *
 * @param[in] p_args  GPT callback arguments (FSP `timer_callback_args_t`);
 *                    `p_args->p_context` must be a valid
 *                    `rs_bb_uart_tx_ctrl_t *`.
 *
 * @return None.
 *
 * @note ISR-safe (this function only ever runs in ISR context). Defensively
 *       returns immediately without touching engine/ring-buffer state if
 *       the lifecycle state is not TRANSMITTING (should not happen in
 *       normal operation, since the timer is stopped whenever the queue is
 *       empty, but guarded against spurious/late interrupts).
 */
void rs_bb_uart_tx_hal_gpt_callback(timer_callback_args_t * p_args)
{
    rs_bb_uart_tx_ctrl_t * p_ctrl = (rs_bb_uart_tx_ctrl_t *) p_args->p_context;
    rs_bb_uart_tx_engine_output_t         out;
    rs_bb_uart_tx_engine_advance_result_t adv;

    if (RS_BB_UART_TX_LIFECYCLE_TRANSMITTING != p_ctrl->state)
    {
        /* Defensive: should not fire outside TRANSMITTING (the timer is
         * stopped whenever the queue is empty), but fail safe rather than
         * touch engine state that might not be freshly reset. */
        return;
    }

    /* O(1), no ring-buffer access on ANY tick — this is the fix. Write the
     * pin first, using the level already decided by the previous tick's
     * advance() call, before doing anything else. */
    out = rs_bb_uart_tx_engine_output_get(&p_ctrl->engine);
    rs_bb_uart_tx_hal_gpio_write(p_ctrl, out.drive_pin_high);

    /* Back half: prepare the *next* tick's output (and do any ring-buffer
     * access that requires — see rs_bb_uart_tx_engine.h for why this is
     * safe here). This has a full bit period of slack before it must
     * complete, not the write above's sub-microsecond budget. */
    adv = rs_bb_uart_tx_engine_advance(&p_ctrl->engine, &p_ctrl->ring_buffer);

    if (adv.byte_transmitted)
    {
        p_ctrl->total_bytes_transmitted++;
    }

    if (adv.queue_now_empty)
    {
        /* No more work: stop the timer (Architecture & Design Section 10:
         * TRANSMITTING -> READY when the ring buffer empties) rather than
         * continuing to interrupt on every bit period while idle. */
        (void) rs_bb_uart_tx_hal_gpt_stop(p_ctrl);
        p_ctrl->state = RS_BB_UART_TX_LIFECYCLE_READY;
    }
}
