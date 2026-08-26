/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_ring_buffer.c
 * @brief Internal TX ring buffer manager implementation.
 *
 * See rs_bb_uart_tx_ring_buffer.h for design notes.
 *
 * Critical section: this file uses the CMSIS PRIMASK save/restore pattern
 * (__get_PRIMASK / __disable_irq / __enable_irq) rather than a PES-specific
 * or FSP-version-specific macro, so it has no dependency on which FSP
 * release's fsp_common_api.h is present. This is the same primitive FSP's
 * own critical-section helpers are built on for single-core Cortex-M
 * targets.
 *
 * IMPORTANT — correction to an earlier claim in this comment (found via
 * real FPB-RA2E3 hardware testing, not caught at design time): the
 * enqueue-side critical section in rs_bb_uart_tx_ring_buffer_enqueue()
 * below is NOT a fixed handful of instructions — it holds interrupts
 * disabled for the full memcpy() of however many bytes are being
 * enqueued, so its duration scales with the caller's request length, not
 * a small constant. For a large single Write()/WriteString() call, or a
 * rapid sequence of calls landing while a transmission is already active
 * (each with its own such critical section), this can approach or exceed
 * a single bit period on slower CPUs/higher baud rates — delaying the GPT
 * ISR enough to corrupt the bit(s) due during that window. Confirmed on
 * FPB-RA2E3 (Cortex-M23 @ 48 MHz); not observed on FPB-RA4E2/FPB-RA6E2
 * (Cortex-M33 @ 100 MHz, more timing margin for the same code). See
 * PhasePlan.md and docs/README.md for the full diagnosis and the
 * demo-level mitigation applied (src/rs_bb_uart_tx_demo.c builds its
 * banner as one combined string instead of many rapid separate calls).
 * A real customer application making large/frequent Write() calls,
 * especially while a previous transmission may still be in progress,
 * should be aware of this — the dequeue-side critical section in
 * rs_bb_uart_tx_hal_gpt_callback() is genuinely small and unaffected
 * (it moves at most one byte per ISR call), only the enqueue side scales
 * with request length.
 */

#include <rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h>
#include <string.h>

#ifndef RS_BB_UART_TX_TEST_HOST_BUILD
 #include "bsp_api.h"       /* __get_PRIMASK / __disable_irq / __enable_irq (CMSIS, via core_cm33.h) */
#endif

/***********************************************************************************************************************
 * Local critical-section helpers
 **********************************************************************************************************************/

#ifndef RS_BB_UART_TX_TEST_HOST_BUILD

#define RS_BB_UART_TX_RB_CRITICAL_ENTER(saved_primask)         \
    do {                                                        \
        (saved_primask) = __get_PRIMASK();                      \
        __disable_irq();                                        \
    } while (0)

#define RS_BB_UART_TX_RB_CRITICAL_EXIT(saved_primask)          \
    do {                                                        \
        if (0U == (saved_primask))                              \
        {                                                       \
            __enable_irq();                                     \
        }                                                       \
    } while (0)

#else /* RS_BB_UART_TX_TEST_HOST_BUILD — host unit tests are single-threaded, no ISR preemption */

#define RS_BB_UART_TX_RB_CRITICAL_ENTER(saved_primask)  ((void) (saved_primask))
#define RS_BB_UART_TX_RB_CRITICAL_EXIT(saved_primask)   ((void) (saved_primask))

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
    p_rb->head  = 0U;
    p_rb->tail  = 0U;
    p_rb->count = 0U;
    /* Buffer contents are not cleared — only indices matter; avoids an
     * unnecessary memset of up to RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES. */
}

/**
 * @brief Enqueue bytes using an all-or-nothing policy.
 *
 * Either all @p length bytes are copied into the buffer and the head index
 * advances, or (if insufficient free space exists) nothing is written and
 * `false` is returned. Uses a short critical section to update `count` and
 * `head` atomically with respect to the GPT ISR's dequeue path
 * (Architecture & Design Section 13.3, Section 17.2).
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
 *       Architecture & Design Section 17.1). Not ISR-safe.
 */
bool rs_bb_uart_tx_ring_buffer_enqueue(rs_bb_uart_tx_ring_buffer_t * p_rb,
                                        uint8_t const               * p_data,
                                        uint32_t                      length)
{
    uint32_t saved_primask;
    uint32_t free_bytes;
    uint32_t head;

    if (0U == length)
    {
        return true; /* Nothing to do; trivially successful, matches all-or-nothing semantics. */
    }

    RS_BB_UART_TX_RB_CRITICAL_ENTER(saved_primask);

    free_bytes = (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - p_rb->count;

    if (length > free_bytes)
    {
        /* All-or-nothing: reject the whole request, buffer state unchanged. */
        RS_BB_UART_TX_RB_CRITICAL_EXIT(saved_primask);
        return false;
    }

    /* Copy bytes into the circular buffer starting at head. Split into at
     * most two contiguous memcpy() calls to handle wrap-around without a
     * byte-by-byte loop. */
    head = p_rb->head;

    if ((head + length) <= (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES)
    {
        memcpy(&p_rb->data[head], p_data, length);
    }
    else
    {
        uint32_t const first_chunk = (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - head;
        uint32_t const second_chunk = length - first_chunk;
        memcpy(&p_rb->data[head], p_data, first_chunk);
        memcpy(&p_rb->data[0], &p_data[first_chunk], second_chunk);
    }

    p_rb->head = (head + length) % (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES;
    p_rb->count += length;

    RS_BB_UART_TX_RB_CRITICAL_EXIT(saved_primask);

    return true;
}

/**
 * @brief Dequeue a single byte for transmission.
 *
 * Called exclusively from the GPT ISR (via the TX engine) when the next
 * byte is needed (TX_NEXT_BYTE -> TX_START_BIT transition, or the very
 * first byte out of TX_IDLE). Uses a short critical section to update
 * `count` and `tail` atomically with respect to the enqueue path.
 *
 * @param[in,out] p_rb    Ring buffer.
 * @param[out]    p_byte  Populated with the dequeued byte on success.
 *                        Left unmodified if the buffer was empty.
 *
 * @retval true   A byte was available and has been written to *p_byte.
 * @retval false  Buffer was empty; *p_byte is not modified.
 *
 * @note ISR-safe. Call only from GPT ISR context.
 */
bool rs_bb_uart_tx_ring_buffer_dequeue(rs_bb_uart_tx_ring_buffer_t * p_rb,
                                        uint8_t                     * p_byte)
{
    uint32_t saved_primask;
    uint32_t tail;
    bool     got_byte = false;

    RS_BB_UART_TX_RB_CRITICAL_ENTER(saved_primask);

    if (p_rb->count > 0U)
    {
        tail     = p_rb->tail;
        *p_byte  = p_rb->data[tail];
        p_rb->tail = (tail + 1U) % (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES;
        p_rb->count--;
        got_byte = true;
    }

    RS_BB_UART_TX_RB_CRITICAL_EXIT(saved_primask);

    return got_byte;
}

/**
 * @brief Query the number of bytes currently queued.
 *
 * Single volatile read — no critical section needed for a snapshot value
 * used only for diagnostics/status reporting.
 *
 * @param[in] p_rb  Ring buffer.
 *
 * @return Number of bytes currently queued (0..RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES).
 */
uint32_t rs_bb_uart_tx_ring_buffer_count_get(rs_bb_uart_tx_ring_buffer_t const * p_rb)
{
    return p_rb->count;
}

/**
 * @brief Query the number of free bytes remaining.
 *
 * @param[in] p_rb  Ring buffer.
 *
 * @return Number of bytes that could currently be enqueued
 *         (0..RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES).
 */
uint32_t rs_bb_uart_tx_ring_buffer_free_get(rs_bb_uart_tx_ring_buffer_t const * p_rb)
{
    return (uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - p_rb->count;
}
