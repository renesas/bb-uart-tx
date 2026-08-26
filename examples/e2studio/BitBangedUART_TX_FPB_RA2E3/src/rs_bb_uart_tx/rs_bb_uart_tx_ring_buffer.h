/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_ring_buffer.h
 * @brief Internal TX ring buffer manager for the Bit-Banged UART TX PES.
 *
 * Internal header — not part of the public API. Not included by
 * rs_bb_uart_tx.h. Implements the "Ring Buffer Manager" module described in
 * Architecture & Design v0.3 Sections 8.1 and 13.
 *
 * Design points (Section 13):
 *   - Statically allocated, fixed capacity (RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES).
 *   - All-or-nothing enqueue: either the whole requested write fits, or
 *     nothing is written and the caller gets a buffer-full status (Section 13.3).
 *   - Shared between bare-metal main-loop/task context (enqueue, via
 *     rs_bb_uart_tx_Write()/WriteString()) and GPT ISR context (dequeue, one
 *     byte at a time as the TX engine needs it). All head/tail/count updates
 *     that cross contexts are protected by a short critical section
 *     (Section 17.2) — see rs_bb_uart_tx_ring_buffer_enqueue() and
 *     rs_bb_uart_tx_ring_buffer_dequeue() implementations.
 */

#ifndef RS_BB_UART_TX_RING_BUFFER_H
#define RS_BB_UART_TX_RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_cfg.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief TX ring buffer storage and indices.
 *
 * Embedded directly in rs_bb_uart_tx_ctrl_t — no dynamic allocation, no
 * pointer indirection to a separately-owned buffer.
 *
 * head/tail/count are volatile because they are written from both
 * main-loop/task context (head, via enqueue) and GPT ISR context (tail,
 * count, via dequeue); count is written from both sides under a critical
 * section.
 */
typedef struct st_rs_bb_uart_tx_ring_buffer
{
    uint8_t           data[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES]; ///< Byte storage
    volatile uint32_t head;                                      ///< Next free write index
    volatile uint32_t tail;                                      ///< Next byte to dequeue
    volatile uint32_t count;                                     ///< Bytes currently queued
} rs_bb_uart_tx_ring_buffer_t;

/***********************************************************************************************************************
 * Internal function prototypes
 **********************************************************************************************************************/

/**
 * @brief Reset a ring buffer to the empty state.
 *
 * Called once from the internal Open() helper. Not ISR-safe by itself —
 * only safe before the GPT ISR is enabled.
 *
 * @param[in,out] p_rb  Ring buffer to reset.
 */
void rs_bb_uart_tx_ring_buffer_reset(rs_bb_uart_tx_ring_buffer_t * p_rb);

/**
 * @brief Enqueue bytes using an all-or-nothing policy.
 *
 * Either all `length` bytes are copied into the buffer and the head index
 * advances, or (if insufficient free space exists) nothing is written and
 * `false` is returned. Uses a short critical section to update `count` and
 * `head` atomically with respect to the GPT ISR's dequeue path
 * (Section 13.3, Section 17.2).
 *
 * @param[in,out] p_rb    Ring buffer.
 * @param[in]     p_data  Bytes to enqueue.
 * @param[in]     length  Number of bytes to enqueue.
 *
 * @retval true   All bytes were accepted.
 * @retval false  Insufficient space; buffer is unchanged.
 *
 * @note Call only from main-loop/task context (matches rs_bb_uart_tx_Write()'s
 *       calling-context restriction — see Section 17.1). Not ISR-safe.
 */
bool rs_bb_uart_tx_ring_buffer_enqueue(rs_bb_uart_tx_ring_buffer_t * p_rb,
                                        uint8_t const               * p_data,
                                        uint32_t                      length);

/**
 * @brief Dequeue a single byte for transmission.
 *
 * Called exclusively from the GPT ISR (via the TX engine) when the next
 * byte is needed (TX_NEXT_BYTE -> TX_START_BIT transition, or the very
 * first byte out of TX_IDLE). Uses a short critical section to update
 * `count` and `tail` atomically with respect to the enqueue path.
 *
 * @param[in,out]  p_rb     Ring buffer.
 * @param[out]     p_byte   Populated with the dequeued byte on success.
 *
 * @retval true   A byte was available and has been written to *p_byte.
 * @retval false  Buffer was empty; *p_byte is not modified.
 *
 * @note ISR-safe. Call only from GPT ISR context.
 */
bool rs_bb_uart_tx_ring_buffer_dequeue(rs_bb_uart_tx_ring_buffer_t * p_rb,
                                        uint8_t                     * p_byte);

/**
 * @brief Query the number of bytes currently queued.
 *
 * Single volatile read — no critical section needed for a snapshot value
 * used only for diagnostics/status reporting.
 *
 * @param[in] p_rb  Ring buffer.
 * @return Number of bytes currently queued.
 */
uint32_t rs_bb_uart_tx_ring_buffer_count_get(rs_bb_uart_tx_ring_buffer_t const * p_rb);

/**
 * @brief Query the number of free bytes remaining.
 *
 * @param[in] p_rb  Ring buffer.
 * @return Number of bytes that could currently be enqueued.
 */
uint32_t rs_bb_uart_tx_ring_buffer_free_get(rs_bb_uart_tx_ring_buffer_t const * p_rb);

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_RING_BUFFER_H */
