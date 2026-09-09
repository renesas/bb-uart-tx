/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_engine.h
 * @brief Internal TX bit-level state machine for the Bit-Banged UART TX.
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
 *
 * PIPELINE: split into two calls instead of one, so that no tick's GPIO
 * write ever has to wait on a ring-buffer access:
 *
 *   rs_bb_uart_tx_engine_output_get() — O(1), no ring-buffer access. Returns
 *     the pin level for the tick about to run, already decided by the
 *     *previous* tick's rs_bb_uart_tx_engine_advance() call. Call this
 *     FIRST, before anything else, so the GPIO write happens with zero
 *     variable-cost work ahead of it on every tick, not just most of them.
 *
 *   rs_bb_uart_tx_engine_advance() — prepares the output for the *next*
 *     tick (including any ring-buffer dequeue) and performs the FSM
 *     transition. Call this AFTER writing the GPIO pin for the current
 *     tick — its cost no longer affects that tick's pin timing, only how
 *     much of the *next* bit period's ~8.68us (at 115200 baud) slack it
 *     consumes, which comfortably covers it. The dequeue this needs is
 *     therefore run one tick early, inside the *previous* tick's
 *     TX_STOP_BIT case, entirely after that tick's own (already fast) pin
 *     write.
 *
 * Race safety: the only ring-buffer access with a real, consequential side
 * effect is the one that decides to stop the GPT timer and return to READY
 * when the queue empties. This design never acts on that decision based on
 * a *prefetched* (potentially stale) check — TX_STOP_BIT's advance()
 * prefetches one tick early (dequeue() is a no-op when the queue is empty,
 * so speculatively calling it early changes nothing if it fails), but if
 * that prefetch finds nothing, the *only* thing committed for the next
 * tick is a tentative idle-high output — correct regardless of the
 * eventual outcome, since idle and the stop bit are both electrically
 * high, so there is no wrong answer to commit to speculatively here. The
 * actual "stop the timer" action is only ever taken by TX_NEXT_BYTE's
 * advance() — one tick later — performing one more, fully fresh dequeue()
 * call rather than trusting the earlier prefetch. This means a `Write()`
 * call landing in the narrow window between the prefetch and the fresh
 * recheck is always picked up correctly; nothing is ever stranded. See
 * rs_bb_uart_tx_engine.c's per-state comments for the implementation of
 * this argument.
 *
 * One accepted, deliberate, one-time behavior change from this design: the
 * very first bit period after a fresh Start() always outputs idle-high (a
 * "wasted" bit period, since TX_IDLE's own output is never dependent on
 * the ring buffer, only its transition is) before the first byte's start
 * bit begins on the tick after that — about one bit period (~8.68us at
 * 115200 baud) of extra launch latency, once per idle-to-transmitting
 * transition. This is not a bit-to-bit jitter concern (jitter is about
 * timing *within* an ongoing transmission, not Write()-to-first-edge
 * launch latency) and has no effect on a real receiver, which does not
 * care when a transmission begins, only that its bit timing is accurate
 * once it does.
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
    RS_BB_UART_TX_ENGINE_STATE_TX_NEXT_BYTE,  ///< Reached only when TX_STOP_BIT's prefetch found nothing; re-checks fresh.
} rs_bb_uart_tx_bit_state_t;

/**
 * @brief TX bit-engine working state.
 *
 * Embedded directly in rs_bb_uart_tx_ctrl_t. Written exclusively from GPT
 * ISR context after Start() — the main-loop/task side never touches these
 * fields directly.
 *
 * `next_output_high` is the pipeline register: the level
 * rs_bb_uart_tx_engine_output_get() will return on the *next* call, decided
 * by the *current* call to rs_bb_uart_tx_engine_advance(). `state` and
 * `bit_index`, at entry to rs_bb_uart_tx_engine_advance(), describe the role
 * that was just displayed (i.e. what `next_output_high` held going into the
 * tick that just ran) — not the role about to be displayed, which is what
 * these fields meant under the pre-pipeline single-call design.
 */
typedef struct st_rs_bb_uart_tx_engine_state
{
    rs_bb_uart_tx_bit_state_t state;             ///< Role most recently displayed (see field-group comment above).
    uint8_t                   current_byte;      ///< Byte currently being shifted out.
    uint8_t                   bit_index;         ///< Index (0..7) of the data bit most recently displayed.
    bool                      next_output_high;  ///< Pipeline register: level for the *next* tick.
} rs_bb_uart_tx_engine_state_t;

/**
 * @brief Result of one rs_bb_uart_tx_engine_output_get() call — the only
 *        thing the GPT ISR needs before it can write the GPIO pin.
 */
typedef struct st_rs_bb_uart_tx_engine_output
{
    bool drive_pin_high;      ///< true = drive TX pin high for this bit period, false = low.
} rs_bb_uart_tx_engine_output_t;

/**
 * @brief Result of one rs_bb_uart_tx_engine_advance() call — tells the
 *        HAL/ISR shim whether a byte's stop bit was just displayed, and
 *        whether the queue has just been freshly confirmed empty.
 */
typedef struct st_rs_bb_uart_tx_engine_advance_result
{
    bool byte_transmitted;    ///< true = a full byte's stop bit was just displayed this tick.
    bool queue_now_empty;     ///< true = a fresh check just confirmed the queue is empty; engine returned to TX_IDLE.
} rs_bb_uart_tx_engine_advance_result_t;

/***********************************************************************************************************************
 * Internal function prototypes
 **********************************************************************************************************************/

/**
 * @brief Reset the TX bit-engine to TX_IDLE, idle-high output pending.
 *
 * Called once from the internal Open() helper, before the GPT ISR is enabled.
 *
 * @param[out] p_engine  Engine state to reset.
 */
void rs_bb_uart_tx_engine_reset(rs_bb_uart_tx_engine_state_t * p_engine);

/**
 * @brief Get the pin level for the tick about to run. O(1), no ring-buffer
 *        access — call this FIRST in the GPT ISR, before anything else.
 *
 * @param[in] p_engine  TX bit-engine state.
 *
 * @return The level to drive the TX pin to for this bit period.
 *
 * @note ISR-safe. Call only from GPT ISR context, and only while the Bit-Banged UART is
 *       in the TRANSMITTING lifecycle state.
 */
rs_bb_uart_tx_engine_output_t rs_bb_uart_tx_engine_output_get(rs_bb_uart_tx_engine_state_t const * p_engine);

/**
 * @brief Prepare the output for the *next* bit period and advance the FSM.
 *
 * Call this AFTER writing the GPIO pin for the current tick (i.e. after
 * rs_bb_uart_tx_engine_output_get()'s result has been used) — this is what
 * makes it safe for this call to access the ring buffer (a variable-cost
 * operation) without affecting the current tick's pin timing; see
 * rs_bb_uart_tx_engine.h's file-level comment for the full design
 * rationale, including why this cannot strand data enqueued while a
 * transmission is in progress.
 *
 * @param[in,out] p_engine  TX bit-engine state.
 * @param[in,out] p_rb      Ring buffer to dequeue from when a new byte is needed.
 *
 * @return Advance result describing whether a byte's stop bit was just
 *         displayed, and whether the queue was just freshly confirmed empty.
 *
 * @note ISR-safe. Call only from GPT ISR context, and only while the Bit-Banged UART is
 *       in the TRANSMITTING lifecycle state.
 */
rs_bb_uart_tx_engine_advance_result_t rs_bb_uart_tx_engine_advance(rs_bb_uart_tx_engine_state_t * p_engine,
                                                                     rs_bb_uart_tx_ring_buffer_t   * p_rb);

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_ENGINE_H */
