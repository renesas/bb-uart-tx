/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_ring_buffer.c
 * @brief Internal TX ring buffer manager implementation.
 *
 * See rs_bb_uart_tx_ring_buffer.h for the full design rationale (lock-free
 * single-producer/single-consumer, head/tail-only synchronization, and why
 * RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES must be a power of two).
 */

#include <rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h>
#include <string.h>

#ifndef RS_BB_UART_TX_TEST_HOST_BUILD
 #include "bsp_api.h"       /* __DMB() (CMSIS, via core_cm33.h / core_cm23.h) */
#endif

/***********************************************************************************************************************
 * Local helpers
 **********************************************************************************************************************/

/** Index wraparound mask. Valid because RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES
 *  is asserted to be a power of two in rs_bb_uart_tx_ring_buffer.h. */
#define RS_BB_UART_TX_RB_MASK  ((uint32_t) (RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES) - 1U)

#ifndef RS_BB_UART_TX_TEST_HOST_BUILD
 #define RS_BB_UART_TX_RB_BARRIER()  __DMB()
#else
 /* Host unit tests are single-threaded, no ISR preemption and no CMSIS
  * intrinsics available — the barrier is a no-op there. */
 #define RS_BB_UART_TX_RB_BARRIER()  ((void) 0)
#endif

/***********************************************************************************************************************
 * Public (internal-linkage) function implementations
 **********************************************************************************************************************/

/**
 * @brief Reset a ring buffer to the empty state.
 *
 * Called once from the internal Open() helper. Not ISR-safe by itself —
 * only safe before the GPT ISR is enabled.
 *
 * @param[in,out] p_rb  Ring buffer to reset.
 *
 * @return None.
 */
void rs_bb_uart_tx_ring_buffer_reset(rs_bb_uart_tx_ring_buffer_t * p_rb)
{
    p_rb->head = 0U;
    p_rb->tail = 0U;
    /* Buffer contents are not cleared — only indices matter; avoids an
     * unnecessary memset of up to RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES. */
}

/**
 * @brief Enqueue bytes using an all-or-nothing policy.
 *
 * Either all @p length bytes are copied into the buffer and the head index
 * advances, or (if insufficient free space exists) nothing is written and
 * `false` is returned. Lock-free: safe without disabling interrupts because
 * this is the sole producer of `head` and the sole reader-for-free-space-
 * check of `tail` (owned by the consumer) — see rs_bb_uart_tx_ring_buffer.h
 * for the full argument. Usable capacity is
 * (RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1) bytes, one slot reserved so
 * head == tail remains an unambiguous "empty" signal.
 *
 * @param[in,out] p_rb    Ring buffer.
 * @param[in]     p_data  Bytes to enqueue. Ignored (may be NULL) when
 *                        @p length is 0.
 * @param[in]     length  Number of bytes to enqueue. 0 is a trivial
 *                        success with no effect on buffer state.
 *
 * @retval true   All bytes were accepted (or @p length was 0).
 * @retval false  Insufficient space; buffer is unchanged.
 *
 * @note Call only from main-loop/task context (matches
 *       rs_bb_uart_tx_Write()'s calling-context restriction — see
 *       Architecture & Design Section 17.1). Not ISR-safe (not re-entrant
 *       with itself — single producer only).
 */
bool rs_bb_uart_tx_ring_buffer_enqueue(rs_bb_uart_tx_ring_buffer_t * p_rb,
                                        uint8_t const               * p_data,
                                        uint32_t                      length)
{
    uint32_t head;
    uint32_t tail;
    uint32_t used;
    uint32_t free_bytes;

    if (0U == length)
    {
        return true; /* Nothing to do; trivially successful, matches all-or-nothing semantics. */
    }

    head = p_rb->head;                 /* Producer-owned; plain read of our own index. */
    tail = p_rb->tail;                 /* Consumer-owned; volatile read of the latest published value.
                                         * A stale (older) value can only under-count `used`, i.e.
                                         * under-report free space — never causes an overflow. */

    used       = (head - tail) & RS_BB_UART_TX_RB_MASK;
    free_bytes = (RS_BB_UART_TX_RB_MASK) - used; /* Capacity - 1 usable, see header comment. */

    if (length > free_bytes)
    {
        /* All-or-nothing: reject the whole request, buffer state unchanged. */
        return false;
    }

    /* Copy bytes into the circular buffer starting at head. Split into at
     * most two contiguous memcpy() calls to handle wrap-around without a
     * byte-by-byte loop. Safe without a critical section: these slots are
     * not yet visible to the consumer until `head` is published below —
     * the consumer only ever reads indices in [tail, head_old), and this
     * function is the only writer of `head`. */
    if ((head & RS_BB_UART_TX_RB_MASK) + length <= (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES)
    {
        memcpy(&p_rb->data[head & RS_BB_UART_TX_RB_MASK], p_data, length);
    }
    else
    {
        uint32_t const start        = head & RS_BB_UART_TX_RB_MASK;
        uint32_t const first_chunk  = (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - start;
        uint32_t const second_chunk = length - first_chunk;
        memcpy(&p_rb->data[start], p_data, first_chunk);
        memcpy(&p_rb->data[0], &p_data[first_chunk], second_chunk);
    }

    /* Publish the new head with a single store, after a barrier that
     * ensures the data writes above are visible to the consumer (GPT ISR,
     * running on the same core, but the barrier documents and enforces the
     * required ordering against both compiler and any store buffering)
     * before the index update that makes those bytes eligible to dequeue. */
    RS_BB_UART_TX_RB_BARRIER();
    p_rb->head = (head + length) & RS_BB_UART_TX_RB_MASK;

    return true;
}

/**
 * @brief Dequeue a single byte for transmission.
 *
 * Called exclusively from the GPT ISR (via the TX engine) when the next
 * byte is needed (TX_NEXT_BYTE -> TX_START_BIT transition, or the very
 * first byte out of TX_IDLE). Lock-free and O(1)/constant-time: a volatile
 * read of `head` (producer-owned), an equality compare, an array read, and
 * a single published store to `tail` — no critical section, no division.
 * See rs_bb_uart_tx_ring_buffer.h for why this is safe without disabling
 * interrupts.
 *
 * @param[in,out] p_rb    Ring buffer.
 * @param[out]    p_byte  Populated with the dequeued byte on success.
 *                        Left unmodified if the buffer was empty.
 *
 * @retval true   A byte was available and has been written to *p_byte.
 * @retval false  Buffer was empty; *p_byte is not modified.
 *
 * @note ISR-safe. Call only from GPT ISR context (single consumer only —
 *       not re-entrant with itself).
 */
bool rs_bb_uart_tx_ring_buffer_dequeue(rs_bb_uart_tx_ring_buffer_t * p_rb,
                                        uint8_t                     * p_byte)
{
    uint32_t head;
    uint32_t tail;

    head = p_rb->head;   /* Producer-owned; volatile read of the latest published value.
                           * A stale (older) value can only make us think the buffer is
                           * emptier than it really is — never causes an under-read. */
    tail = p_rb->tail;   /* Consumer-owned; plain read of our own index. */

    if (head == tail)
    {
        return false;    /* Empty. */
    }

    *p_byte = p_rb->data[tail & RS_BB_UART_TX_RB_MASK];

    /* Publish the new tail with a single store, after a barrier that
     * ensures the read above has completed before the slot is marked free
     * for the producer to reuse. */
    RS_BB_UART_TX_RB_BARRIER();
    p_rb->tail = (tail + 1U) & RS_BB_UART_TX_RB_MASK;

    return true;
}

/**
 * @brief Query the number of bytes currently queued.
 *
 * Two volatile reads and a subtract — no critical section needed for a
 * snapshot value used only for diagnostics/status reporting (may be
 * momentarily stale with respect to a concurrently-running enqueue/dequeue,
 * which is acceptable for a diagnostics snapshot — see
 * rs_bb_uart_tx_StatusGet()'s own contract).
 *
 * @param[in] p_rb  Ring buffer.
 *
 * @return Number of bytes currently queued (0..RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1).
 */
uint32_t rs_bb_uart_tx_ring_buffer_count_get(rs_bb_uart_tx_ring_buffer_t const * p_rb)
{
    return (p_rb->head - p_rb->tail) & RS_BB_UART_TX_RB_MASK;
}

/**
 * @brief Query the number of free bytes remaining.
 *
 * @param[in] p_rb  Ring buffer.
 *
 * @return Number of bytes that could currently be enqueued
 *         (0..RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1).
 */
uint32_t rs_bb_uart_tx_ring_buffer_free_get(rs_bb_uart_tx_ring_buffer_t const * p_rb)
{
    return (RS_BB_UART_TX_RB_MASK) - rs_bb_uart_tx_ring_buffer_count_get(p_rb);
}
