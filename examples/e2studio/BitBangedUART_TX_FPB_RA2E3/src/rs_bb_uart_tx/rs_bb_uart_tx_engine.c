/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_engine.c
 * @brief Internal TX bit-level state machine implementation.
 *
 * See rs_bb_uart_tx_engine.h for design notes. This file is intentionally
 * hardware-agnostic (no FSP includes) so it can be compiled and unit tested
 * on a host build; rs_bb_uart_tx_hal.c is the only file that owns the
 * GPT/GPIO-facing side of the ISR.
 */

#include <rs_bb_uart_tx/rs_bb_uart_tx_engine.h>

/**
 * @brief Reset the TX bit-engine to TX_IDLE.
 *
 * Called once from the internal Open() helper, before the GPT ISR is
 * enabled.
 *
 * @param[out] p_engine  Engine state to reset.
 *
 * @return None.
 */
void rs_bb_uart_tx_engine_reset(rs_bb_uart_tx_engine_state_t * p_engine)
{
    p_engine->state        = RS_BB_UART_TX_ENGINE_STATE_TX_IDLE;
    p_engine->current_byte = 0U;
    p_engine->bit_index    = 0U;
}

/**
 * @brief Advance the TX bit-engine by exactly one bit period.
 *
 * Called once per GPT compare-match interrupt (Architecture & Design
 * Section 12.3). Pulls a new byte from the ring buffer via
 * rs_bb_uart_tx_ring_buffer_dequeue() exactly when a new byte is needed
 * (TX_IDLE -> TX_START_BIT, or TX_NEXT_BYTE -> TX_START_BIT); never
 * blocks, never allocates, never formats strings (Section 12.3 ISR rules).
 *
 * @param[in,out] p_engine  TX bit-engine state.
 * @param[in,out] p_rb      Ring buffer to dequeue from when a new byte is needed.
 *
 * @return Tick result (rs_bb_uart_tx_engine_tick_result_t) describing the
 *         pin level to drive this bit period, whether a byte's stop bit
 *         was just completed, and whether the queue just emptied.
 *
 * @note ISR-safe. Call only from GPT ISR context, and only while the PES
 *       is in the TRANSMITTING lifecycle state.
 */
rs_bb_uart_tx_engine_tick_result_t rs_bb_uart_tx_engine_tick(rs_bb_uart_tx_engine_state_t * p_engine,
                                                               rs_bb_uart_tx_ring_buffer_t   * p_rb)
{
    rs_bb_uart_tx_engine_tick_result_t result;
    uint8_t next_byte;

    result.drive_pin_high   = true;  /* Idle-high default; overridden below where needed. */
    result.byte_transmitted = false;
    result.queue_now_empty  = false;

    switch (p_engine->state)
    {
        case RS_BB_UART_TX_ENGINE_STATE_TX_IDLE:
        {
            /* Only reached on the very first tick after Start(), or after the
             * queue emptied. Try to pull the first byte; if one is ready,
             * begin its start bit immediately in this same tick so the first
             * byte is not delayed by an extra idle bit period. */
            if (rs_bb_uart_tx_ring_buffer_dequeue(p_rb, &next_byte))
            {
                p_engine->current_byte = next_byte;
                p_engine->bit_index    = 0U;
                p_engine->state        = RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT;
                result.drive_pin_high  = false; /* Start bit is low. */
            }
            else
            {
                result.drive_pin_high = true; /* Nothing queued; remain idle high. */
            }
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT:
        {
            /* Start bit period has elapsed; begin data bit 0 (LSB). */
            result.drive_pin_high = (0U != (p_engine->current_byte & 0x01U));
            p_engine->bit_index   = 1U;
            p_engine->state       = RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS;
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS:
        {
            /* Drive the bit selected by bit_index (which was already advanced
             * past the bit driven on the previous tick). */
            result.drive_pin_high = (0U != ((p_engine->current_byte >> p_engine->bit_index) & 0x01U));

            if (p_engine->bit_index >= (uint8_t) (RS_BB_UART_TX_DATA_BITS - 1U))
            {
                /* That was data bit 7 (MSB) being driven this tick; next tick
                 * is the stop bit. */
                p_engine->state = RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT;
            }
            else
            {
                p_engine->bit_index++;
            }
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT:
        {
            /* Stop bit is high for one full bit period. */
            result.drive_pin_high   = true;
            result.byte_transmitted = true;
            p_engine->state         = RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE;
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE:
        {
            /* Decide, within this same bit period, whether another byte is
             * queued (continue immediately with its start bit — no extra
             * idle gap between back-to-back bytes) or the queue is empty
             * (return to TX_IDLE, remaining high). */
            if (rs_bb_uart_tx_ring_buffer_dequeue(p_rb, &next_byte))
            {
                p_engine->current_byte = next_byte;
                p_engine->bit_index    = 0U;
                p_engine->state        = RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT;
                result.drive_pin_high  = false; /* Start bit is low. */
            }
            else
            {
                p_engine->state        = RS_BB_UART_TX_ENGINE_STATE_TX_IDLE;
                result.drive_pin_high  = true;
                result.queue_now_empty = true;
            }
            break;
        }

        default:
        {
            /* Unreachable in normal operation; fail safe to idle-high. */
            p_engine->state       = RS_BB_UART_TX_ENGINE_STATE_TX_IDLE;
            result.drive_pin_high = true;
            break;
        }
    }

    return result;
}
