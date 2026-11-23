/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_ring_buffer.h
 * @brief Internal TX ring buffer manager for the Bit-Banged UART TX.
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
 *     byte at a time as the TX engine needs it).
 *
 * SYNCHRONIZATION: this is a classic single-producer/single-consumer (SPSC)
 * ring buffer — `head` is written ONLY by the producer (enqueue,
 * main-loop/task context) and `tail` is written ONLY by the consumer
 * (dequeue, GPT ISR context). That property means correctness does NOT
 * require disabling interrupts at all: a naturally-aligned 32-bit store to
 * `head` or `tail` is indivisible with respect to interrupts on this MCU
 * family, so the other side can never observe a torn value, and each side
 * only ever *reads* the index it does not own. Occupancy is derived on
 * demand from `head - tail` rather than tracked in a separate shared
 * `count` field, which removes any need for a critical section on either
 * `enqueue()` or `dequeue()`.
 *
 * Because head==tail must remain an unambiguous "empty" signal with no
 * separate `count` field, this design reserves one slot: the buffer can
 * hold at most (RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1) bytes, one fewer
 * than the raw storage size. This is the standard, well-known trade-off
 * for a head/tail-only circular buffer.
 *
 * `head`/`tail` wrap using a bitmask (`& (SIZE - 1)`), not `%`, which is
 * why RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES must be a power of two — enforced
 * by a `_Static_assert` below. Every currently-supported reference size
 * (64/128/256/512/1024, see rs_bb_uart_tx_cfg.h) already satisfies this.
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
 * Compile-time configuration checks
 **********************************************************************************************************************/

/** RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES must be a power of two — see the
 *  file-level comment above for why (mask-based index wraparound). */
_Static_assert(0U != (RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES),
               "RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES must not be 0.");
_Static_assert(0U == ((RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES) & ((RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES) - 1U)),
               "RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES must be a power of two "
               "(required for lock-free head/tail mask-based wraparound) - "
               "use one of the documented reference sizes 64/128/256/512/1024.");

/***********************************************************************************************************************
 * Types
 **********************************************************************************************************************/

/**
 * @brief TX ring buffer storage and indices.
 *
 * Embedded directly in rs_bb_uart_tx_ctrl_t — no dynamic allocation, no
 * pointer indirection to a separately-owned buffer.
 *
 * `head` is written ONLY by the producer (enqueue, main-loop/task context);
 * `tail` is written ONLY by the consumer (dequeue, GPT ISR context). Both
 * are `volatile` so each side always observes the other's latest published
 * value rather than a compiler-cached stale one — this, not a critical
 * section, is what makes the SPSC design in this file's header comment
 * safe. Usable capacity is (RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1) bytes;
 * see the file-level comment for why one slot is reserved.
 */
typedef struct st_rs_bb_uart_tx_ring_buffer
{
    uint8_t           data[RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES]; ///< Byte storage
    volatile uint32_t head;                                      ///< Next free write index (producer-owned)
    volatile uint32_t tail;                                      ///< Next byte to dequeue (consumer-owned)
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
 * `false` is returned. Lock-free (Section 13.3, Section 17.2) — the data
 * copy touches only slots not yet visible to the consumer, and `head` is
 * published with a single volatile store after a memory barrier; see the
 * file-level comment for why this is safe without disabling interrupts.
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
 * first byte out of TX_IDLE). Lock-free and O(1)/constant-time (no
 * critical section, no division) — see the file-level comment.
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
