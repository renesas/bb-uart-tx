/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_ctrl.h
 * @brief Internal control-block definition for the Bit-Banged UART TX PES.
 *
 * Internal header — NOT included by the public rs_bb_uart_tx.h and not
 * visible to application code. See rs_bb_uart_tx_types.h for the rationale
 * (this PES is a singleton: one GPIO pin, one GPT channel, one instance).
 *
 * Included by rs_bb_uart_tx.c (which owns the single static instance) and
 * rs_bb_uart_tx_hal.h/.c (which operate on a pointer to it from ISR and
 * shim-function context).
 */

#ifndef RS_BB_UART_TX_CTRL_H
#define RS_BB_UART_TX_CTRL_H

#include <rs_bb_uart_tx/rs_bb_uart_tx_types.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_engine.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bit-Banged UART TX instance control block (internal).
 *
 * Exactly one instance exists: the static g_bb_uart_tx_ctrl in
 * rs_bb_uart_tx.c. Do not declare additional instances — the GPT
 * ISR callback (rs_bb_uart_tx_hal_gpt_callback()) is wired to this specific
 * instance via p_context at Open() time.
 *
 * Version and size are the first two fields (Engineering Guidelines
 * convention). RAM budget is verified at build time via static_assert
 * against RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES (see rs_bb_uart_tx.c).
 */
typedef struct st_rs_bb_uart_tx_ctrl
{
    /* --- Versioning (must remain first two fields) ------------------------ */
    uint32_t    version;      ///< Bit-Banged UART TX module version (encoded MAJOR.MINOR.PATCH).
    uint32_t    ctrl_size;    ///< sizeof(rs_bb_uart_tx_ctrl_t) for compatibility checking.

    /* --- Lifecycle state ---------------------------------------------------*/
    rs_bb_uart_tx_lifecycle_state_t state;   ///< Current internal lifecycle state (Section 10).

    /* --- Bound configuration ------------------------------------------------*/
    rs_bb_uart_tx_cfg_t const * p_cfg;       ///< Pointer to cfg bound at (internal) Open(); NULL before then.

    /* --- Ring buffer (Section 13; static — no heap) -------------------------*/
    rs_bb_uart_tx_ring_buffer_t ring_buffer;

    /* --- TX bit-engine (Section 12) ------------------------------------------*/
    rs_bb_uart_tx_engine_state_t engine;

    /* --- Diagnostics (backs rs_bb_uart_tx_StatusGet(), Section 9.3.3) -------*/
    uint32_t    total_bytes_accepted;      ///< Total bytes accepted by Write()/WriteString().
    uint32_t    total_bytes_transmitted;   ///< Total bytes fully shifted out onto the TX pin.
    uint32_t    buffer_full_count;         ///< Number of Write()/WriteString() calls rejected as buffer-full.
    uint32_t    invalid_call_count;        ///< Number of public calls rejected for wrong lifecycle state / bad args.
} rs_bb_uart_tx_ctrl_t;

/***********************************************************************************************************************
 * Internal cross-file declarations
 *
 * g_bb_uart_tx_ctrl is defined (with external linkage) in rs_bb_uart_tx.c.
 * rs_bb_uart_tx_open_internal() is also defined there, and is called from
 * the board-specific config data file's rs_bb_uart_tx_ConfigCheck()
 * implementation on successful validation (see rs_bb_uart_tx.h top-of-file
 * note for the rationale). Neither is part of the public API.
 **********************************************************************************************************************/

extern rs_bb_uart_tx_ctrl_t g_bb_uart_tx_ctrl;

/**
 * @brief Internal Open()-equivalent helper (Architecture & Design Section 10).
 *
 * Binds p_cfg into the singleton control block, opens the GPT timer and
 * GPIO TX pin HAL resources, resets the ring buffer and TX bit-engine, and
 * transitions the internal lifecycle state UNCONFIGURED/CONFIGURED -> READY.
 *
 * @param[in] p_cfg  Validated configuration (caller — rs_bb_uart_tx_ConfigCheck()
 *                    in the board data file — has already range-checked
 *                    baud_rate and confirmed p_gpt_instance is non-NULL).
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS        Opened; state -> READY.
 * @retval RS_BB_UART_TX_STATUS_INVALID_STATE  Already READY/TRANSMITTING/ERROR (not idempotent).
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR    GPT HAL open/period-set failed; state -> ERROR.
 * @retval RS_BB_UART_TX_STATUS_GPIO_ERROR     GPIO pin configuration failed; state -> ERROR.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_open_internal(rs_bb_uart_tx_cfg_t const * p_cfg);

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_CTRL_H */
