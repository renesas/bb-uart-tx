/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_engine.h
 * @brief Internal TX bit-level state machine for the Bit-Banged UART TX PES.
 *
 * Internal header — not part of the public API. Implements the "UART TX
 * Engine" module described in Architecture & Design v0.3 Sections 8.1 and
 * 12. Advances exactly one bit period per call, driven by the GPT ISR
 * (Section 12.3). Hardware-agnostic: this module never touches FSP types
 * directly — it calls back into the HAL shim (rs_bb_uart_tx_hal.h) to
 * actually drive the GPIO pin level, so it can be unit-tested on the host.
 *
 * Frame format (Section 12.1, fixed for Phase 1): 8N1, idle high, LSB-first.
 *
 *   Idle: High
 *   Start bit: Low                     (1 bit period)
 *   Data bits 0..7: LSB first           (8 bit periods)
 *   Stop bit: High                      (1 bit period)
 *
 * State machine (Section 12.2):
 *
 *   TX_IDLE --(byte available)--> TX_START_BIT --> TX_DATA_BITS --> TX_STOP_BIT
 *      ^                                                                |
 *      |                                                                v
 *      +----------(queue empty)----------------- TX_NEXT_BYTE <---------+
 *                                                      |
 *                                                      +--(another byte queued)--> TX_START_BIT
 */

#ifndef RS_BB_UART_TX_ENGINE_H
#define RS_BB_UART_TX_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_cfg.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/** Internal TX bit-engine states (Section 12.2). */
typedef enum e_rs_bb_uart_tx_bit_state
{
    RS_BB_UART_TX_ENGINE_STATE_TX_IDLE = 0,   ///< No transmission in progress; line idle high.
    RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT,  ///< Driving the start bit (low) for one bit period.
    RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS,  ///< Driving data bits 0..7, LSB first.
    RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT,   ///< Driving the stop bit (high) for one bit period.
    RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE,  ///< Between bytes; deciding whether to continue or idle.
} rs_bb_uart_tx_bit_state_t;

/**
 * @brief TX bit-engine working state.
 *
 * Embedded directly in rs_bb_uart_tx_ctrl_t. Written exclusively from GPT
 * ISR context after Start() — the main-loop/task side never touches these
 * fields directly.
 */
typedef struct st_rs_bb_uart_tx_engine_state
{
    rs_bb_uart_tx_bit_state_t state;         ///< Current TX bit-engine state.
    uint8_t                   current_byte;  ///< Byte currently being shifted out.
    uint8_t                   bit_index;     ///< Index (0..7) of the next data bit to send.
} rs_bb_uart_tx_engine_state_t;

/**
 * @brief Result of one rs_bb_uart_tx_engine_tick() call — tells the HAL/ISR
 *        shim what to do with the GPIO pin, and whether transmission just
 *        finished (queue emptied).
 */
typedef struct st_rs_bb_uart_tx_engine_tick_result
{
    bool drive_pin_high;      ///< true = drive TX pin high for this bit period, false = low.
    bool byte_transmitted;    ///< true = a full byte's stop bit was just completed this tick.
    bool queue_now_empty;     ///< true = engine returned to TX_IDLE this tick (queue was empty).
} rs_bb_uart_tx_engine_tick_result_t;

/***********************************************************************************************************************
 * Internal function prototypes
 **********************************************************************************************************************/

/**
 * @brief Reset the TX bit-engine to TX_IDLE.
 *
 * Called once from the internal Open() helper, before the GPT ISR is enabled.
 *
 * @param[out] p_engine  Engine state to reset.
 */
void rs_bb_uart_tx_engine_reset(rs_bb_uart_tx_engine_state_t * p_engine);

/**
 * @brief Advance the TX bit-engine by exactly one bit period.
 *
 * Called once per GPT compare-match interrupt (Section 12.3). Pulls a new
 * byte from the ring buffer via rs_bb_uart_tx_ring_buffer_dequeue() exactly
 * when a new byte is needed (TX_IDLE -> TX_START_BIT, or
 * TX_NEXT_BYTE -> TX_START_BIT); never blocks, never allocates, never
 * formats strings (Section 12.3 ISR rules).
 *
 * @param[in,out] p_engine  TX bit-engine state.
 * @param[in,out] p_rb      Ring buffer to dequeue from when a new byte is needed.
 *
 * @return Tick result describing the pin level to drive this bit period and
 *         whether the queue just emptied.
 *
 * @note ISR-safe. Call only from GPT ISR context, and only while the PES is
 *       in the TRANSMITTING lifecycle state.
 */
rs_bb_uart_tx_engine_tick_result_t rs_bb_uart_tx_engine_tick(rs_bb_uart_tx_engine_state_t * p_engine,
                                                               rs_bb_uart_tx_ring_buffer_t   * p_rb);

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_ENGINE_H */
