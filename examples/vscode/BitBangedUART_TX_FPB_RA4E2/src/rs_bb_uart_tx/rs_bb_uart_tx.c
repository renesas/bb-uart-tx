/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx.c
 * @brief Core orchestration for the Bit-Banged UART TX: public function
 *        implementations and the internal lifecycle state machine.
 *
 * See rs_bb_uart_tx.h for the public API contract and the singleton
 * rationale, and Architecture & Design v0.3 Section 10 for the internal
 * lifecycle model this file implements:
 *
 *   UNCONFIGURED -> CONFIGURED -> READY <-> TRANSMITTING
 *        \_____________\____________\____________/
 *                        \-> ERROR (any state, on unrecoverable failure)
 *
 * This file never includes GPT/GPIO-specific FSP headers directly — all
 * hardware access goes through rs_bb_uart_tx_hal.h (Engineering Guidelines
 * Section 9.2, target-specific isolation).
 */

#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_engine.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_hal.h>
#include <string.h>

/***********************************************************************************************************************
 * The one and only Bit-Banged UART TX instance (see rs_bb_uart_tx.h / rs_bb_uart_tx_types.h
 * for why this Bit-Banged UART is a singleton rather than a caller-allocated control block).
 **********************************************************************************************************************/

rs_bb_uart_tx_ctrl_t g_bb_uart_tx_ctrl =
{
    .version              = RS_BB_UART_TX_VERSION,
    .ctrl_size            = sizeof(rs_bb_uart_tx_ctrl_t),
    .state                = RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED,
    .p_cfg                = NULL,
    .total_bytes_accepted = 0U,
    .total_bytes_transmitted = 0U,
    .buffer_full_count    = 0U,
    .invalid_call_count   = 0U,
    /* .ring_buffer and .engine are zero-initialised by static storage rules;
     * rs_bb_uart_tx_open_internal() explicitly resets them regardless. */
};

/** RAM budget guard (Engineering Guidelines Software Development Rule; design
 *  doc Section 5.7/18.3 default budget expectations). */
_Static_assert(sizeof(rs_bb_uart_tx_ctrl_t) <= RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES,
               "rs_bb_uart_tx_ctrl_t exceeds RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES - "
               "reduce RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES or raise the limit explicitly.");

/***********************************************************************************************************************
 * Parameter-checking macro (mirrors FSP convention; scoped to this file)
 **********************************************************************************************************************/

#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
 #define RS_BB_UART_TX_ASSERT_RETURN(expr, err)  do { if (!(expr)) { return (err); } } while (0)
#else
 #define RS_BB_UART_TX_ASSERT_RETURN(expr, err)  ((void) 0)
#endif

/***********************************************************************************************************************
 * Internal (file-local) helpers
 **********************************************************************************************************************/

/**
 * @brief Start (or continue) asynchronous transmission.
 *
 * Called from rs_bb_uart_tx_Write()/WriteString() after a successful
 * enqueue. If the Bit-Banged UART was READY (timer stopped, queue was empty before this
 * enqueue), starts the GPT timer and transitions to TRANSMITTING. If the
 * Bit-Banged UART was already TRANSMITTING, the timer is already running and this is a
 * no-op — the GPT ISR will naturally pick up the newly-queued bytes.
 *
 * @param[in,out] p_ctrl  Bit-Banged UART TX control block. Must be in the
 *                        READY or TRANSMITTING lifecycle state (checked by
 *                        the caller before this is invoked).
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS      Timer already running, or was
 *                                           started successfully; state is
 *                                           now TRANSMITTING.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR  GPT start failed; state ->
 *                                           ERROR (propagated from
 *                                           rs_bb_uart_tx_hal_gpt_start()).
 */
static rs_bb_uart_tx_status_t rs_bb_uart_tx_start_internal(rs_bb_uart_tx_ctrl_t * p_ctrl)
{
    rs_bb_uart_tx_status_t status;

    if (RS_BB_UART_TX_LIFECYCLE_TRANSMITTING == p_ctrl->state)
    {
        return RS_BB_UART_TX_STATUS_SUCCESS; /* Timer already running; nothing to do. */
    }

    status = rs_bb_uart_tx_hal_gpt_start(p_ctrl);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status)
    {
        p_ctrl->state = RS_BB_UART_TX_LIFECYCLE_ERROR;
        return status;
    }

    p_ctrl->state = RS_BB_UART_TX_LIFECYCLE_TRANSMITTING;
    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/**
 * @brief Internal Open()-equivalent helper (Architecture & Design Section 10).
 *
 * Binds @p p_cfg into the singleton control block (g_bb_uart_tx_ctrl),
 * resets the ring buffer and TX bit-engine, opens the GPIO TX pin and GPT
 * timer HAL resources, and transitions the internal lifecycle state
 * UNCONFIGURED/CONFIGURED -> READY. Full contract documented in
 * rs_bb_uart_tx_ctrl.h (the internal header that declares this function so
 * the board config data file's rs_bb_uart_tx_ConfigCheck() can call it).
 *
 * @param[in] p_cfg  Validated configuration (the caller —
 *                    rs_bb_uart_tx_ConfigCheck() in the board data file —
 *                    has already range-checked baud_rate and confirmed
 *                    p_gpt_instance is non-NULL). Must remain valid
 *                    (static/global storage) for the life of the
 *                    application; the singleton retains a pointer to it.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS        Opened; state -> READY.
 * @retval RS_BB_UART_TX_STATUS_INVALID_STATE  Already READY/TRANSMITTING/
 *                                             ERROR (not idempotent — see
 *                                             Architecture & Design Section 5).
 * @retval RS_BB_UART_TX_STATUS_GPIO_ERROR     GPIO pin configuration failed;
 *                                             state -> ERROR.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR    GPT HAL open/period-set failed;
 *                                             state -> ERROR.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_open_internal(rs_bb_uart_tx_cfg_t const * p_cfg)
{
    rs_bb_uart_tx_status_t status;

    if (RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED != g_bb_uart_tx_ctrl.state
        && RS_BB_UART_TX_LIFECYCLE_CONFIGURED != g_bb_uart_tx_ctrl.state)
    {
        /* Already READY/TRANSMITTING/ERROR — not idempotent in Phase 1
         * (Architecture & Design Section 5: "Runtime remapping of GPIO or
         * timer while transmission is active" is explicitly out of scope). */
        g_bb_uart_tx_ctrl.invalid_call_count++;
        return RS_BB_UART_TX_STATUS_INVALID_STATE;
    }

    g_bb_uart_tx_ctrl.p_cfg = p_cfg;

    rs_bb_uart_tx_ring_buffer_reset(&g_bb_uart_tx_ctrl.ring_buffer);
    rs_bb_uart_tx_engine_reset(&g_bb_uart_tx_ctrl.engine);

    status = rs_bb_uart_tx_hal_gpio_open(&g_bb_uart_tx_ctrl);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status)
    {
        g_bb_uart_tx_ctrl.state = RS_BB_UART_TX_LIFECYCLE_ERROR;
        return status;
    }

    status = rs_bb_uart_tx_hal_gpt_open(&g_bb_uart_tx_ctrl);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status)
    {
        g_bb_uart_tx_ctrl.state = RS_BB_UART_TX_LIFECYCLE_ERROR;
        return status;
    }

    g_bb_uart_tx_ctrl.state = RS_BB_UART_TX_LIFECYCLE_READY;
    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/***********************************************************************************************************************
 * Public function implementations
 **********************************************************************************************************************/

/**
 * @brief Get the size, in bytes, of the internal Bit-Banged UART TX control block.
 *
 * Full contract documented in rs_bb_uart_tx.h. Useful for confirming
 * control-block sizing (e.g. against RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES)
 * without requiring rs_bb_uart_tx_ConfigCheck() to have been called first.
 * Since this Bit-Banged UART is a singleton, no control-block pointer argument is
 * needed.
 *
 * @param[out] p_size  Populated with the internal control block size in bytes.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS          Size returned in *p_size.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER  p_size is NULL.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGetSize(size_t * p_size)
{
    RS_BB_UART_TX_ASSERT_RETURN(NULL != p_size, RS_BB_UART_TX_STATUS_INVALID_POINTER);

    *p_size = sizeof(rs_bb_uart_tx_ctrl_t);
    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/**
 * @brief Enqueue raw bytes for transmission.
 *
 * Full contract documented in rs_bb_uart_tx.h. Non-blocking: copies bytes
 * into the TX ring buffer using an all-or-nothing policy (Architecture &
 * Design Section 13.3), then starts asynchronous transmission via
 * rs_bb_uart_tx_start_internal() if the Bit-Banged UART was idle.
 *
 * @param[in] p_data  Bytes to enqueue. Ignored (may be NULL) when @p length is 0.
 * @param[in] length  Number of bytes to enqueue. 0 is a no-op success.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS          All bytes accepted into the queue
 *                                               (or @p length was 0).
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER  p_data is NULL and length > 0.
 * @retval RS_BB_UART_TX_STATUS_INVALID_STATE    Bit-Banged UART is not READY/TRANSMITTING.
 * @retval RS_BB_UART_TX_STATUS_TX_BUFFER_FULL   Insufficient free space for
 *                                               the whole request.
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR      Propagated from
 *                                               rs_bb_uart_tx_start_internal()
 *                                               if the GPT timer failed to start.
 *
 * @note Valid from startup context (after ConfigCheck() succeeds) and
 *       bare-metal main-loop/task context. NOT valid from ISR context in
 *       Phase 1 (Architecture & Design Section 17.1).
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_Write(uint8_t const * p_data, uint32_t length)
{
    rs_bb_uart_tx_status_t status;

    if (0U == length)
    {
        return RS_BB_UART_TX_STATUS_SUCCESS; /* Nothing to do. */
    }

    RS_BB_UART_TX_ASSERT_RETURN(NULL != p_data, RS_BB_UART_TX_STATUS_INVALID_POINTER);

    if ((RS_BB_UART_TX_LIFECYCLE_READY != g_bb_uart_tx_ctrl.state)
        && (RS_BB_UART_TX_LIFECYCLE_TRANSMITTING != g_bb_uart_tx_ctrl.state))
    {
        g_bb_uart_tx_ctrl.invalid_call_count++;
        return RS_BB_UART_TX_STATUS_INVALID_STATE;
    }

    if (!rs_bb_uart_tx_ring_buffer_enqueue(&g_bb_uart_tx_ctrl.ring_buffer, p_data, length))
    {
        g_bb_uart_tx_ctrl.buffer_full_count++;
        return RS_BB_UART_TX_STATUS_TX_BUFFER_FULL;
    }

    g_bb_uart_tx_ctrl.total_bytes_accepted += length;

    status = rs_bb_uart_tx_start_internal(&g_bb_uart_tx_ctrl);
    return status;
}

/**
 * @brief Enqueue a NUL-terminated ASCII string for transmission.
 *
 * Full contract documented in rs_bb_uart_tx.h. Calculates the string
 * length up to (but not including) the terminating NUL byte via
 * `strlen()`, then delegates to rs_bb_uart_tx_Write() for the actual
 * enqueue (same all-or-nothing buffer policy, Architecture & Design
 * Section 13.4). The terminating NUL is never transmitted.
 *
 * @param[in] p_str  NUL-terminated ASCII string to enqueue.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS          All characters accepted into the queue.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER  p_str is NULL.
 * @retval RS_BB_UART_TX_STATUS_INVALID_STATE    Bit-Banged UART is not READY/TRANSMITTING
 *                                               (propagated from rs_bb_uart_tx_Write()).
 * @retval RS_BB_UART_TX_STATUS_TX_BUFFER_FULL   Insufficient free space for
 *                                               the whole string (propagated
 *                                               from rs_bb_uart_tx_Write()).
 *
 * @note Same calling-context restriction as rs_bb_uart_tx_Write().
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_WriteString(char const * p_str)
{
    RS_BB_UART_TX_ASSERT_RETURN(NULL != p_str, RS_BB_UART_TX_STATUS_INVALID_POINTER);

    return rs_bb_uart_tx_Write((uint8_t const *) p_str, (uint32_t) strlen(p_str));
}

/**
 * @brief Get a snapshot of Bit-Banged UART TX runtime status and diagnostics.
 *
 * Full contract documented in rs_bb_uart_tx.h. Populates every field of
 * @p p_state from the current singleton control-block state — lifecycle
 * state, configured baud rate, ring-buffer occupancy, and the four
 * cumulative diagnostic counters (Architecture & Design Section 9.3.3
 * "Recommended status contents").
 *
 * @param[out] p_state  Caller-allocated status structure, populated on return.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS          Status populated.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER  p_state is NULL.
 *
 * @note Valid in any lifecycle state, including before ConfigCheck()
 *       (fields will read as zero/UNCONFIGURED). Callable from main-loop/
 *       task context; not guaranteed ISR-safe (reads multiple fields
 *       without a critical section — acceptable for diagnostics snapshots,
 *       not for control-flow decisions).
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_StatusGet(rs_bb_uart_tx_state_t * p_state)
{
    RS_BB_UART_TX_ASSERT_RETURN(NULL != p_state, RS_BB_UART_TX_STATUS_INVALID_POINTER);

    p_state->version   = RS_BB_UART_TX_VERSION;
    p_state->size      = sizeof(rs_bb_uart_tx_state_t);
    p_state->state     = g_bb_uart_tx_ctrl.state;
    p_state->baud_rate = (NULL != g_bb_uart_tx_ctrl.p_cfg) ? g_bb_uart_tx_ctrl.p_cfg->baud_rate : 0U;

    p_state->queued_bytes = rs_bb_uart_tx_ring_buffer_count_get(&g_bb_uart_tx_ctrl.ring_buffer);
    p_state->free_bytes   = rs_bb_uart_tx_ring_buffer_free_get(&g_bb_uart_tx_ctrl.ring_buffer);

    p_state->total_bytes_accepted    = g_bb_uart_tx_ctrl.total_bytes_accepted;
    p_state->total_bytes_transmitted = g_bb_uart_tx_ctrl.total_bytes_transmitted;
    p_state->buffer_full_count       = g_bb_uart_tx_ctrl.buffer_full_count;
    p_state->invalid_call_count      = g_bb_uart_tx_ctrl.invalid_call_count;

    return RS_BB_UART_TX_STATUS_SUCCESS;
}
