/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_types.h
 * @brief Public type definitions and enumerations for the Bit-Banged UART TX.
 *
 * This header is included by rs_bb_uart_tx.h and may be included directly by
 * application code that needs to reference Bit-Banged UART TX types without
 * including the full public interface header.
 *
 * Structure layout follows Architecture & Design v0.3 Section 11
 * ("Configuration Structure Recommendations"), with the two placeholder
 * `void *` HAL-instance fields from that draft resolved to concrete FSP
 * types here (`timer_instance_t const * p_gpt_instance`,
 * `bsp_io_port_pin_t tx_pin`) as anticipated by the doc's own note:
 * "void pointers may be replaced by concrete FSP instance types in
 * implementation."
 *
 * @note Singleton design decision (resolves Section 26 Open Item #1/#3):
 * the Architecture & Design Section 19.4 example pseudocode calls
 * rs_bb_uart_tx_Write()/WriteString()/ConfigGen()/ConfigCheck() with no
 * control-block argument anywhere — only one Bit-Banged UART TX instance
 * (one GPIO pin, one GPT channel) is ever needed per application. This
 * implementation therefore owns a single static control block internally
 * (see rs_bb_uart_tx_ctrl.h, an internal-only header) rather than exposing
 * an rs_bb_uart_tx_ctrl_t the application must declare and pass around, as
 * rs_rtc_logger's multi-instance-capable API does. This is a deliberate,
 * documented deviation from that reference pattern — see PhasePlan.md.
 */

#ifndef RS_BB_UART_TX_TYPES_H
#define RS_BB_UART_TX_TYPES_H

#include <rs_bb_uart_tx/rs_bb_uart_tx_cfg.h>

/* Pull in FSP types when building on-target; stub types defined in test shim
 * for host-based unit tests (RS_BB_UART_TX_TEST_HOST_BUILD). */
#ifndef RS_BB_UART_TX_TEST_HOST_BUILD
 #include "bsp_api.h"
 #include "r_timer_api.h"      /* timer_instance_t — implemented by r_gpt for this Bit-Banged UART */
#else
 #include "rs_bb_uart_tx_test_types.h"  /* host-build type stubs */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * Enumerations
 **********************************************************************************************************************/

/**
 * Bit-Banged UART TX internal lifecycle states — Architecture & Design
 * Section 10. Not a formal FSP Runtime-Control lifecycle; Open/Start/Stop/
 * Close are internal file-local helpers only (see rs_bb_uart_tx.c), never
 * exposed as public functions, per the doc's explicit instruction:
 * "They should not be exposed as mandatory public functions unless a later
 * architecture review determines that the public lifecycle model is needed."
 */
typedef enum e_rs_bb_uart_tx_lifecycle_state
{
    RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED = 0, ///< No valid configuration bound yet.
    RS_BB_UART_TX_LIFECYCLE_CONFIGURED   = 1, ///< ConfigGen() succeeded; ConfigCheck() not yet run (or re-run pending).
    RS_BB_UART_TX_LIFECYCLE_READY        = 2, ///< Configuration validated and HAL resources opened; idle, ready to transmit.
    RS_BB_UART_TX_LIFECYCLE_TRANSMITTING = 3, ///< GPT ISR actively driving bits; ring buffer non-empty or mid-frame.
    RS_BB_UART_TX_LIFECYCLE_ERROR        = 4, ///< Deterministic error state; requires re-init / corrective action.
} rs_bb_uart_tx_lifecycle_state_t;

/**
 * @brief Bit-Banged UART TX status/error codes.
 *
 * Bit-Banged UART-specific status enum per Engineering Guidelines Software Development
 * Rule 2 (fsp_err_t is not used here because most Bit-Banged UART TX
 * failures — bad baud rate, buffer full, unconfigured state — are use-case
 * level failures, not FSP HAL failures). Success is zero; all non-success
 * values are documented below and at each function's declaration.
 */
typedef enum e_rs_bb_uart_tx_status
{
    RS_BB_UART_TX_STATUS_SUCCESS = 0,             ///< Operation completed successfully.
    RS_BB_UART_TX_STATUS_INVALID_POINTER,         ///< A required pointer argument was NULL.
    RS_BB_UART_TX_STATUS_INVALID_ARGUMENT,        ///< An argument was outside its valid range.
    RS_BB_UART_TX_STATUS_INVALID_STATE,           ///< Call is not valid in the current lifecycle state.
    RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE,   ///< baud_rate outside RS_BB_UART_TX_CFG_MIN/MAX_BAUD_RATE.
    RS_BB_UART_TX_STATUS_UNSUPPORTED_FRAME_FORMAT,///< Reserved for future frame-format options (Phase 1: 8N1 only).
    RS_BB_UART_TX_STATUS_INVALID_BUFFER_SIZE,     ///< buffer_size field inconsistent with compiled buffer size.
    RS_BB_UART_TX_STATUS_TX_BUFFER_FULL,          ///< Not enough free space for an all-or-nothing enqueue.
    RS_BB_UART_TX_STATUS_RESOURCE_CONFLICT,       ///< GPT/GPIO resource unavailable or already owned elsewhere.
    RS_BB_UART_TX_STATUS_TIMER_ERROR,             ///< Propagated failure from the GPT HAL driver.
    RS_BB_UART_TX_STATUS_GPIO_ERROR,              ///< Propagated failure from GPIO pin configuration.
} rs_bb_uart_tx_status_t;

/***********************************************************************************************************************
 * Structures
 **********************************************************************************************************************/

/**
 * @brief Bit-Banged UART TX instance configuration.
 *
 * Populated by rs_bb_uart_tx_ConfigGen() (board-specific config data file).
 * Must remain valid (const-correct, static storage) for the full lifetime
 * of the Bit-Banged UART TX instance.
 *
 * Field layout mirrors Architecture & Design v0.3 Section 11.1.
 */
typedef struct st_rs_bb_uart_tx_cfg
{
    uint32_t                 version;          ///< Config structure version (compatibility check).
    uint32_t                 size;              ///< sizeof(rs_bb_uart_tx_cfg_t) at ConfigGen() time.

    uint32_t                 baud_rate;         ///< Requested baud rate, bits/second.
    uint32_t                 buffer_size;       ///< Informational copy of RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES.

    uint8_t                  data_bits;         ///< Fixed to RS_BB_UART_TX_DATA_BITS (8) in Phase 1.
    uint8_t                  parity;            ///< Reserved; fixed to 0 (none) in Phase 1.
    uint8_t                  stop_bits;         ///< Fixed to RS_BB_UART_TX_STOP_BITS (1) in Phase 1.
    uint8_t                  overflow_policy;   ///< Reserved; fixed to "return error" in Phase 1.
    uint8_t                  irq_priority;      ///< GPT bit-period interrupt priority (IPL).

    /** GPT HAL instance pointer (r_gpt, implementing the generic timer_instance_t
     *  interface) — bound at configuration time. Preset or FSP Stack
     *  Configurator-generated; see rs_bb_uart_tx_data_fpb_ra4e2.h. */
    timer_instance_t const * p_gpt_instance;

    /** Reserved for a future ioport_instance_t if a board ever needs
     *  Configurator-managed pin state instead of direct R_BSP_Pin* control.
     *  NULL in Phase 1 — the GPIO adapter drives tx_pin directly via
     *  R_BSP_PinAccessEnable()/R_BSP_PinWrite(). */
    void                    * p_gpio_instance;

    /** TX GPIO output pin (board VCOM TXD by default — see board data file). */
    bsp_io_port_pin_t         tx_pin;
} rs_bb_uart_tx_cfg_t;

/**
 * @brief Returned by rs_bb_uart_tx_StatusGet() — read-only runtime snapshot.
 *
 * Field list mirrors Architecture & Design v0.3 Section 9.3.3 "Recommended
 * status contents".
 */
typedef struct st_rs_bb_uart_tx_state
{
    uint32_t                        version;                  ///< Bit-Banged UART TX module version.
    uint32_t                        size;                     ///< sizeof(rs_bb_uart_tx_state_t).
    rs_bb_uart_tx_lifecycle_state_t state;                    ///< Current internal lifecycle state.
    uint32_t                        baud_rate;                ///< Configured baud rate.
    uint32_t                        queued_bytes;              ///< Bytes currently pending in the ring buffer.
    uint32_t                        free_bytes;                ///< Bytes of ring buffer space currently free.
    uint32_t                        total_bytes_accepted;      ///< Total bytes accepted since Open().
    uint32_t                        total_bytes_transmitted;   ///< Total bytes fully transmitted since Open().
    uint32_t                        buffer_full_count;         ///< Count of buffer-full rejections since Open().
    uint32_t                        invalid_call_count;        ///< Count of invalid-state/argument rejections since Open().
} rs_bb_uart_tx_state_t;

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_TYPES_H */
