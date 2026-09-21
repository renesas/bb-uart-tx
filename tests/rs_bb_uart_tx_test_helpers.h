/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_bb_uart_tx_test_helpers.h
 * @brief Helpers shared by the Bit-Banged UART TX test files.
 *
 * Header-only (static inline) and free of any FSP / host-only dependency, so
 * it is used unchanged by both the host executables and the on-target image.
 *
 *   helper_singleton_reset()  — the PES is a singleton with no public Close();
 *                               tests restore its as-linked initial state.
 *   helper_decode_frames()    — 8N1 receiver model operating on a stream of
 *                               line levels sampled once per bit period.
 *   helper_engine_run()       — ISR-equivalent driver for the pure TX engine.
 **********************************************************************************************************************/

#ifndef RS_BB_UART_TX_TEST_HELPERS_H
#define RS_BB_UART_TX_TEST_HELPERS_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_engine.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h>

/** Ring-buffer usable capacity: one slot is reserved (head == tail means empty). */
#define HELPER_RB_CAPACITY   ((uint32_t) RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1U)

/** Decoder error codes (negative return values of helper_decode_frames()). */
#define HELPER_DECODE_FRAMING_ERROR   (-1)   /* stop bit was low                       */
#define HELPER_DECODE_INCOMPLETE      (-2)   /* start bit seen but frame ran off the end */

/** Restore the singleton control block to exactly its as-linked initial state
 *  (rs_bb_uart_tx.c: g_bb_uart_tx_ctrl initialiser).  The PES exposes no public
 *  Close(), so every test fixture starts from here. */
static inline void helper_singleton_reset(void)
{
    memset(&g_bb_uart_tx_ctrl, 0, sizeof(g_bb_uart_tx_ctrl));
    g_bb_uart_tx_ctrl.version   = RS_BB_UART_TX_VERSION;
    g_bb_uart_tx_ctrl.ctrl_size = (uint32_t) sizeof(rs_bb_uart_tx_ctrl_t);
    g_bb_uart_tx_ctrl.state     = RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED;
    g_bb_uart_tx_ctrl.p_cfg     = NULL;
}

/** Ideal 8N1 frame for @p byte as 10 line levels: start(0), D0..D7 (LSB first), stop(1). */
static inline void helper_expected_frame(uint8_t byte, uint8_t out_levels[10])
{
    out_levels[0] = 0U;
    for (uint32_t i = 0U; i < 8U; i++)
    {
        out_levels[1U + i] = (uint8_t) ((byte >> i) & 0x01U);
    }
    out_levels[9] = 1U;
}

/**
 * 8N1 receiver model.  @p p_levels holds the line level (1 = high) sampled
 * once per bit period, which is exactly what the PES produces: one pin write
 * per GPT tick, one tick per bit period.
 *
 * Idle-high samples are skipped; each low sample is a start bit, followed by
 * eight data bits (LSB first) and a stop bit that must be high.
 *
 * @param[out] p_start_ticks  Optional: sample index of every start bit.
 * @return number of frames decoded, or HELPER_DECODE_FRAMING_ERROR /
 *         HELPER_DECODE_INCOMPLETE.
 */
static inline int32_t helper_decode_frames(uint8_t const * p_levels, uint32_t n,
                                           uint8_t * p_out, uint32_t out_max,
                                           uint32_t * p_start_ticks)
{
    uint32_t i     = 0U;
    uint32_t count = 0U;

    while (i < n)
    {
        if (0U != p_levels[i])
        {
            i++;                          /* idle */
            continue;
        }

        if ((i + 9U) >= n)
        {
            return HELPER_DECODE_INCOMPLETE;
        }
        if (0U == p_levels[i + 9U])
        {
            return HELPER_DECODE_FRAMING_ERROR;
        }

        uint8_t byte = 0U;
        for (uint32_t b = 0U; b < 8U; b++)
        {
            byte = (uint8_t) (byte | (uint8_t) ((p_levels[i + 1U + b] & 0x01U) << b));
        }

        if (count < out_max)
        {
            p_out[count] = byte;
            if (NULL != p_start_ticks)
            {
                p_start_ticks[count] = i;
            }
        }
        count++;
        i += 10U;
    }

    return (int32_t) count;
}

/** Result of helper_engine_run(). */
typedef struct
{
    uint32_t ticks;              ///< Ticks executed (== samples written to p_levels).
    uint32_t bytes_transmitted;  ///< Sum of advance().byte_transmitted over the run.
    bool     drained;            ///< true if the run ended because queue_now_empty was reported.
} helper_engine_run_t;

/**
 * Drive the pure TX engine exactly like rs_bb_uart_tx_hal_gpt_callback() does:
 * output_get() first (recorded as the line level for this tick), then
 * advance().  Stops when the engine reports the queue freshly empty (the tick
 * on which the real callback would stop the GPT) or after @p max_ticks.
 */
static inline helper_engine_run_t helper_engine_run(rs_bb_uart_tx_engine_state_t * p_engine,
                                                    rs_bb_uart_tx_ring_buffer_t * p_rb,
                                                    uint8_t * p_levels, uint32_t max_ticks)
{
    helper_engine_run_t r;
    memset(&r, 0, sizeof(r));

    while (r.ticks < max_ticks)
    {
        rs_bb_uart_tx_engine_output_t         out = rs_bb_uart_tx_engine_output_get(p_engine);
        p_levels[r.ticks] = (uint8_t) (out.drive_pin_high ? 1U : 0U);
        r.ticks++;

        rs_bb_uart_tx_engine_advance_result_t adv = rs_bb_uart_tx_engine_advance(p_engine, p_rb);
        if (adv.byte_transmitted)
        {
            r.bytes_transmitted++;
        }
        if (adv.queue_now_empty)
        {
            r.drained = true;
            break;
        }
    }
    return r;
}

#endif /* RS_BB_UART_TX_TEST_HELPERS_H */
