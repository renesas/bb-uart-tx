/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_hal.h
 * @brief HAL abstraction shim for the Bit-Banged UART TX PES.
 *
 * Internal header — not part of the public API. Declares the thin shim
 * functions that wrap FSP GPT (timer_instance_t) and direct GPIO pin
 * control, isolating rs_bb_uart_tx.c and rs_bb_uart_tx_engine.c from FSP
 * HAL implementation details (Engineering Guidelines Section 9.2,
 * "target-specific implementation details should be isolated"; Architecture
 * & Design Section 8.1, "GPT Timer Adapter" / "GPIO Adapter" modules).
 *
 * This is the ONLY file in the PES that includes GPT-specific FSP headers
 * and drives the TX pin via direct register/BSP pin control. If a future
 * board port needs a different timer peripheral (e.g. AGT instead of GPT),
 * this file (plus the board's config data file) is what changes — the core
 * orchestration (rs_bb_uart_tx.c) and the bit-engine (rs_bb_uart_tx_engine.c)
 * do not.
 */

#ifndef RS_BB_UART_TX_HAL_H
#define RS_BB_UART_TX_HAL_H

#include <rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * Internal function prototypes — GPT timer adapter
 **********************************************************************************************************************/

/**
 * @brief Open and configure the GPT timer for bit-period generation.
 *
 * Wraps p_ctrl->p_cfg->p_gpt_instance->p_api->open(), then computes and
 * applies the period counts corresponding to p_ctrl->p_cfg->baud_rate via
 * R_GPT_PeriodSet() (Architecture & Design Section 14.2). Registers
 * rs_bb_uart_tx_hal_gpt_callback() as the GPT ISR-context callback with
 * p_context = p_ctrl, so the callback can reach the owning control block.
 *
 * Does NOT start the timer — the timer is started/stopped on demand by
 * rs_bb_uart_tx_Write()/the GPT callback itself as the ring buffer
 * transitions between empty and non-empty (Section 10: READY <-> TRANSMITTING).
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block (p_cfg must already be bound).
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS       GPT opened and period configured.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR   Propagated failure from the GPT HAL driver.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_open(rs_bb_uart_tx_ctrl_t * p_ctrl);

/**
 * @brief Start the GPT timer (begin generating bit-period interrupts).
 *
 * Called by rs_bb_uart_tx_Write()/WriteString() when the ring buffer
 * transitions from empty to non-empty while READY.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS      Timer started.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR  Propagated failure from the GPT HAL driver.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_start(rs_bb_uart_tx_ctrl_t * p_ctrl);

/**
 * @brief Stop the GPT timer (queue has emptied; no more bits to send).
 *
 * Called from GPT ISR context (rs_bb_uart_tx_hal_gpt_callback()) once the TX
 * bit-engine returns to TX_IDLE. Safe to call from ISR context — this is a
 * simple GPT register write, not a blocking operation.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS      Timer stopped.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR  Propagated failure from the GPT HAL driver.
 *
 * @note ISR-safe.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_stop(rs_bb_uart_tx_ctrl_t * p_ctrl);

/**
 * @brief Close the GPT timer and release the HAL driver instance.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS      Timer closed.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR  Propagated failure from the GPT HAL driver.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpt_close(rs_bb_uart_tx_ctrl_t * p_ctrl);

/***********************************************************************************************************************
 * Internal function prototypes — GPIO adapter
 **********************************************************************************************************************/

/**
 * @brief Configure the TX pin as a push-pull digital output, idling high.
 *
 * Uses direct BSP pin control (R_BSP_PinAccessEnable() / R_BSP_PinCfg() /
 * R_BSP_PinWrite()) rather than a full ioport_instance_t, since a single
 * statically-configured output pin does not need the generality of the
 * IOPORT driver's instance/table model (Architecture & Design Section 16.1).
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS     Pin configured and idling high.
 * @retval RS_BB_UART_TX_STATUS_GPIO_ERROR  Pin configuration failed.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpio_open(rs_bb_uart_tx_ctrl_t * p_ctrl);

/**
 * @brief Drive the TX pin to the requested level.
 *
 * Called once per GPT ISR tick from rs_bb_uart_tx_hal_gpt_callback(). Must
 * be fast and deterministic — a single BSP pin-write call.
 *
 * @param[in] p_ctrl      Bit-Banged UART TX control block.
 * @param[in] level_high  true = drive high, false = drive low.
 *
 * @note ISR-safe.
 */
void rs_bb_uart_tx_hal_gpio_write(rs_bb_uart_tx_ctrl_t * p_ctrl, bool level_high);

/**
 * @brief Return the TX pin to a safe idle-high state and release pin access.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS  Always (best-effort; direct pin control
 *                                       has no failure mode worth reporting).
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_hal_gpio_close(rs_bb_uart_tx_ctrl_t * p_ctrl);

/***********************************************************************************************************************
 * GPT ISR-context callback — registered with the GPT HAL driver by
 * rs_bb_uart_tx_hal_gpt_open(). Not called directly by application or core
 * orchestration code.
 **********************************************************************************************************************/

/**
 * @brief GPT compare-match/overflow callback — advances the TX bit-engine
 *        by exactly one bit period and drives the TX pin accordingly.
 *
 * Registered as p_cfg->p_callback when the GPT timer_instance_t is opened.
 * p_args->p_context is the owning rs_bb_uart_tx_ctrl_t pointer.
 *
 * Per Architecture & Design Section 12.3, this function:
 *   - Clears the timer interrupt condition (handled by the FSP GPT driver
 *     itself before invoking the callback).
 *   - Advances the TX state machine by one bit period
 *     (rs_bb_uart_tx_engine_tick()).
 *   - Drives the GPIO TX pin to the required level.
 *   - Dequeues the next byte from the ring buffer when required (inside
 *     rs_bb_uart_tx_engine_tick()).
 *   - Stops the GPT timer and transitions the lifecycle state back to READY
 *     when the ring buffer has emptied.
 *   - Returns quickly: no allocation, no blocking, no application callback.
 *
 * @param[in] p_args  GPT callback arguments (FSP timer_callback_args_t).
 */
void rs_bb_uart_tx_hal_gpt_callback(timer_callback_args_t * p_args);

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_HAL_H */
