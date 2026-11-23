/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_engine.c
 * @brief Internal TX bit-level state machine implementation — pipelined
 *        (see rs_bb_uart_tx_engine.h for the full design rationale).
 *
 * This file is intentionally hardware-agnostic (no FSP includes) so it can
 * be compiled and unit tested on a host build; rs_bb_uart_tx_hal.c is the
 * only file that owns the GPT/GPIO-facing side of the ISR.
 */

#include <rs_bb_uart_tx/rs_bb_uart_tx_engine.h>

/**
 * @brief Reset the TX bit-engine to TX_IDLE, idle-high output pending.
 *
 * Called once from the internal Open() helper, before the GPT ISR is
 * enabled. `next_output_high = true` matches the GPIO pin's own idle level
 * from rs_bb_uart_tx_hal_gpio_open() — the very first tick after any
 * subsequent Start() correctly reads back "idle-high" before the first
 * byte's start bit begins one tick later (see the file-level comment in
 * rs_bb_uart_tx_engine.h for why this one-tick launch delay is accepted).
 *
 * @param[out] p_engine  Engine state to reset.
 *
 * @return None.
 */
void rs_bb_uart_tx_engine_reset(rs_bb_uart_tx_engine_state_t * p_engine)
{
    p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_IDLE;
    p_engine->current_byte     = 0U;
    p_engine->bit_index        = 0U;
    p_engine->next_output_high = true;
}

/**
 * @brief Get the pin level for the tick about to run.
 *
 * O(1): a single field read, no ring-buffer access, no switch dispatch.
 * This is deliberately as small as possible — see rs_bb_uart_tx_engine.h's
 * file-level comment for why even a cheap conditional here would defeat
 * the point of this restructuring.
 *
 * @param[in] p_engine  TX bit-engine state.
 *
 * @return The level to drive the TX pin to for this bit period.
 *
 * @note ISR-safe. Call only from GPT ISR context, and only while the Bit-Banged UART
 *       is in the TRANSMITTING lifecycle state.
 */
rs_bb_uart_tx_engine_output_t rs_bb_uart_tx_engine_output_get(rs_bb_uart_tx_engine_state_t const * p_engine)
{
    rs_bb_uart_tx_engine_output_t out;

    out.drive_pin_high = p_engine->next_output_high;

    return out;
}

/**
 * @brief Prepare the output for the *next* bit period and advance the FSM.
 *
 * Call this AFTER writing the GPIO pin for the current tick. `p_engine->state`
 * at entry describes the role that was just displayed (i.e. what
 * `next_output_high` held going into the tick that just ran, already
 * consumed via rs_bb_uart_tx_engine_output_get()) — this function's job is
 * to compute what the *next* tick should show and store it back into
 * `next_output_high`, plus advance `state`/`bit_index`/`current_byte` for
 * the following call.
 *
 * @param[in,out] p_engine  TX bit-engine state.
 * @param[in,out] p_rb      Ring buffer to dequeue from when a new byte is needed.
 *
 * @return Advance result (rs_bb_uart_tx_engine_advance_result_t).
 *
 * @note ISR-safe. Call only from GPT ISR context, and only while the Bit-Banged UART
 *       is in the TRANSMITTING lifecycle state.
 */
rs_bb_uart_tx_engine_advance_result_t rs_bb_uart_tx_engine_advance(rs_bb_uart_tx_engine_state_t * p_engine,
                                                                     rs_bb_uart_tx_ring_buffer_t   * p_rb)
{
    rs_bb_uart_tx_engine_advance_result_t result;
    uint8_t                               next_byte;

    result.byte_transmitted = false;
    result.queue_now_empty  = false;

    switch (p_engine->state)
    {
        case RS_BB_UART_TX_ENGINE_STATE_TX_IDLE:
        {
            /* Reached on the very first tick after a fresh Start() (that
             * tick's own output, "idle-high", was already displayed via
             * output_get() — see rs_bb_uart_tx_engine_reset()), and
             * defensively on any tick that should not normally occur
             * (mirrors the original design's own framing of this branch).
             * rs_bb_uart_tx_Write()'s internal start helper only calls
             * Start() immediately after that *same* call's own successful
             * enqueue(), so this dequeue() is expected to always succeed
             * here in normal operation. */
            if (rs_bb_uart_tx_ring_buffer_dequeue(p_rb, &next_byte))
            {
                p_engine->current_byte     = next_byte;
                p_engine->bit_index        = 0U;
                p_engine->next_output_high = false; /* next tick: start bit, low */
                p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT;
            }
            else
            {
                p_engine->next_output_high = true; /* nothing queued; remain idle */
                p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_IDLE;
            }
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT:
        {
            /* Start bit was just displayed. Prepare data bit 0 (LSB) for
             * the next tick — pure function of current_byte, no ring
             * buffer access, no jitter risk from this state either way. */
            p_engine->next_output_high = (0U != (p_engine->current_byte & 0x01U));
            p_engine->bit_index        = 0U;
            p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS;
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS:
        {
            /* Data bit `bit_index` was just displayed. */
            if (p_engine->bit_index >= (uint8_t) (RS_BB_UART_TX_DATA_BITS - 1U))
            {
                /* That was data bit 7 (MSB); the stop bit is always high —
                 * pure, no ring-buffer access needed to prepare it. */
                p_engine->next_output_high = true;
                p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT;
            }
            else
            {
                p_engine->next_output_high = (0U != ((p_engine->current_byte >> (p_engine->bit_index + 1U)) & 0x01U));
                p_engine->bit_index++;
                p_engine->state = RS_BB_UART_TX_ENGINE_STATE_TX_DATA_BITS;
            }
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_STOP_BIT:
        {
            /* Stop bit was just displayed — a full byte has now been sent.
             *
             * Prefetch the next byte NOW, one tick ahead of when it's
             * needed, instead of waiting for the TX_NEXT_BYTE tick to do it
             * right before that tick's own pin write. This call runs in the
             * back half of the ISR that just displayed the stop bit — it
             * has a full bit period of slack (~8.68us at 115200 baud)
             * before its result is needed, not the sub-microsecond budget
             * a pre-write dequeue() would have.
             *
             * If found: 100% safe to consume for real immediately and
             * commit to starting its start bit next tick. No race is
             * possible for this path — every byte of a given
             * Write()/WriteString() call is fully enqueued before that
             * call ever starts the timer (Architecture & Design Section
             * 10/13), so there is nothing "not yet enqueued" to race
             * against for any within-message byte boundary; this dequeue
             * merely happens ~8.68us earlier than it strictly needs to,
             * not before the data actually exists.
             *
             * If NOT found: do *not* act on that yet. Cache a tentative
             * idle-high output for next tick — correct regardless of the
             * eventual outcome, since idle and the stop bit are both
             * electrically high, so there is no wrong answer to commit to
             * speculatively here — and defer the real decision to
             * TX_NEXT_BYTE's own advance() one tick later. dequeue() is a
             * no-op when the queue is empty (does not mutate anything), so
             * speculatively calling it here changes nothing if it fails. */
            result.byte_transmitted = true;

            if (rs_bb_uart_tx_ring_buffer_dequeue(p_rb, &next_byte))
            {
                p_engine->current_byte     = next_byte;
                p_engine->bit_index        = 0U;
                p_engine->next_output_high = false; /* next tick: start bit, low */
                p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT;
            }
            else
            {
                p_engine->next_output_high = true; /* tentative; re-verified fresh below */
                p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE;
            }
            break;
        }

        case RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE:
        {
            /* Reached only when TX_STOP_BIT's prefetch above found nothing.
             * This tick's own output (already displayed: idle-high) was
             * safe regardless of what we find here. Now: one more, fully
             * fresh dequeue() call — not trusting the earlier prefetch —
             * decides for real whether to continue or to (finally) commit
             * to stopping the timer. A `Write()` call landing in the window
             * between the prefetch and this recheck is picked up here
             * correctly — nothing is ever stranded, since this fresh check
             * always reflects the ring buffer's true, current state. */
            if (rs_bb_uart_tx_ring_buffer_dequeue(p_rb, &next_byte))
            {
                p_engine->current_byte     = next_byte;
                p_engine->bit_index        = 0U;
                p_engine->next_output_high = false; /* next tick: start bit, low */
                p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_START_BIT;
            }
            else
            {
                p_engine->next_output_high = true;
                p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_IDLE;
                result.queue_now_empty     = true;
            }
            break;
        }

        default:
        {
            /* Unreachable in normal operation; fail safe to idle-high. */
            p_engine->next_output_high = true;
            p_engine->state            = RS_BB_UART_TX_ENGINE_STATE_TX_IDLE;
            break;
        }
    }

    return result;
}
