/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx.h
 * @brief Public interface for the Bit-Banged UART TX (rs_bb_uart_tx).
 *
 * Software-driven, transmit-only UART implementation using a GPIO output
 * pin and GPT timer-based bit-period control. Enables UART-style console or
 * diagnostic output when a hardware SCI/UART channel is unavailable or
 * intentionally reserved for another function.
 *
 * Phase 1 scope: TX-only, 8N1, LSB-first, configurable baud rate
 * (1200-115200), non-blocking writes. No receive. No interactive CLI input.
 * See PES_Bit-BangedUART_Acceptance_ArchDesign_v03 for the full
 * requirements and architecture.
 *
 * This Bit-Banged UART is a **singleton**: one instance per application, corresponding
 * to one GPIO TX pin and one GPT timer channel. There is no caller-managed
 * control block — the module owns its own static instance internally (see
 * rs_bb_uart_tx_types.h for the rationale).
 *
 * Usage (FPB-RA4E2, preset configuration path — see
 * rs_bb_uart_tx_data_fpb_ra4e2.h for details):
 * @code
 *   #include <rs_bb_uart_tx/rs_bb_uart_tx.h>
 *   #include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>
 *
 *   rs_bb_uart_tx_cfg_t    g_cfg;
 *   rs_bb_uart_tx_status_t status;
 *
 *   status = rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0_preset, BSP_IO_PORT_04_PIN_11);
 *   status = rs_bb_uart_tx_ConfigCheck(&g_cfg);   // validates AND commits/opens on success
 *   if (RS_BB_UART_TX_STATUS_SUCCESS == status)
 *   {
 *       (void) rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\r\n");
 *   }
 * @endcode
 *
 * @note rs_bb_uart_tx_ConfigCheck() performs double duty by design
 * (Engineering Guidelines Section 7, "Single-call/simple-use Bit-Banged UART" pattern
 * and Section 6.2, "initialization... may be hidden inside a higher-level
 * public function"): on success it also performs the internal
 * Open()-equivalent step (binds the configuration, opens the GPT and GPIO
 * HAL resources, resets the ring buffer and TX engine, and transitions the
 * internal lifecycle state to READY). This keeps the public sequence to
 * exactly the three calls shown above, matching Architecture & Design
 * Section 19.4's example pseudocode with no additional hidden steps.
 * Internal Open()/Start()/Stop()/Close() helpers exist only as static
 * functions inside rs_bb_uart_tx.c (Architecture & Design Section 10) and
 * are never part of this public header.
 */

#ifndef RS_BB_UART_TX_H
#define RS_BB_UART_TX_H

#include <rs_bb_uart_tx/rs_bb_uart_tx_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************************************************************
 * Module version
 **********************************************************************************************************************/

/** Bit-Banged UART TX module version — encoded as (MAJOR << 16) | (MINOR << 8) | PATCH */
#define RS_BB_UART_TX_VERSION_MAJOR    (0U)
#define RS_BB_UART_TX_VERSION_MINOR    (1U)
#define RS_BB_UART_TX_VERSION_PATCH    (0U)
#define RS_BB_UART_TX_VERSION          ((RS_BB_UART_TX_VERSION_MAJOR << 16U) | \
                                        (RS_BB_UART_TX_VERSION_MINOR <<  8U) | \
                                        (RS_BB_UART_TX_VERSION_PATCH))

/***********************************************************************************************************************
 * Public function prototypes
 **********************************************************************************************************************/

/* ── Configuration bridge ─────────────────────────────────────────────────── */

/**
 * rs_bb_uart_tx_ConfigGen() and rs_bb_uart_tx_ConfigCheck() are declared in
 * this header but DEFINED in the board-specific pre-set configuration
 * header/source, alongside the *_preset GPT instance data they are most
 * commonly paired with:
 *
 *   #include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>   (FPB-RA4E2)
 *
 * They are defined unconditionally (regardless of
 * RS_BB_UART_TX_CFG_PRESET_CONFIG_USE), so the same functions remain
 * available whether the caller binds *_preset instances or FSP Stack
 * Configurator-generated instances (e.g. g_timer0).
 */

/**
 * @brief Assign the GPT HAL instance and TX pin to a Bit-Banged UART TX configuration structure.
 *
 * Also fills in Phase 1 defaults for the remaining fields
 * (RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE, RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES,
 * 8N1, RS_BB_UART_TX_CFG_GPT_IPL) — override any of these on *p_cfg after
 * this call returns, before calling rs_bb_uart_tx_ConfigCheck().
 *
 * @param[out] p_cfg           Configuration structure to populate (any prior contents overwritten).
 * @param[in]  p_gpt_instance  GPT HAL instance pointer (preset or FSP Stack Configurator-generated).
 * @param[in]  tx_pin          GPIO pin to use as the TX output (board VCOM TXD by default).
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS           Configuration populated.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER   p_cfg or p_gpt_instance is NULL.
 *
 * @note Callable from any context before rs_bb_uart_tx_ConfigCheck(). Not ISR-safe.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGen(rs_bb_uart_tx_cfg_t     * p_cfg,
                                                timer_instance_t const * p_gpt_instance,
                                                bsp_io_port_pin_t         tx_pin);

/**
 * @brief Validate a configuration, and — on success — commit it and open HAL resources.
 *
 * Validates: baud rate is within the supported range, the GPT instance
 * pointer is present, and the TX pin value is set. On success, this
 * function also performs the equivalent of an internal Open(): binds *p_cfg
 * into the singleton control block, opens the GPT timer (computing and
 * applying the bit-period count for the configured baud rate) and the GPIO
 * TX pin (idling high), resets the ring buffer and TX bit-engine, and
 * transitions the internal lifecycle state to READY. See this header's
 * top-of-file note for the rationale.
 *
 * @param[in] p_cfg  Configuration structure populated by rs_bb_uart_tx_ConfigGen().
 *                    Must remain valid (static/global storage) for the life
 *                    of the application — the singleton retains a pointer
 *                    to it, it is not copied.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS                 Configuration valid; Bit-Banged UART is now READY.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER          p_cfg or p_cfg->p_gpt_instance is NULL.
 * @retval RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE    baud_rate outside the supported range.
 * @retval RS_BB_UART_TX_STATUS_INVALID_STATE            Bit-Banged UART is already READY/TRANSMITTING
 *                                                        (call is not idempotent; Phase 1 does
 *                                                        not support runtime reconfiguration).
 * @retval RS_BB_UART_TX_STATUS_TIMER_ERROR              GPT HAL open/period-set failed.
 * @retval RS_BB_UART_TX_STATUS_GPIO_ERROR               GPIO pin configuration failed.
 *
 * @note Valid once, before the first rs_bb_uart_tx_Write()/WriteString() call. Not ISR-safe.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigCheck(rs_bb_uart_tx_cfg_t const * p_cfg);

/**
 * @brief Get the size, in bytes, of the internal Bit-Banged UART TX control block.
 *
 * Useful for confirming control-block sizing (e.g. against
 * RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES) without requiring
 * rs_bb_uart_tx_ConfigCheck() to have been called first. Since this Bit-Banged UART is a
 * singleton, no control-block pointer argument is needed — unlike a
 * multi-instance component, there is only ever one instance to size.
 *
 * @param[out] p_size  Populated with the internal control block size in bytes.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS          Size returned in *p_size.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER  p_size is NULL.
 *
 * @note Valid in any lifecycle state, including before ConfigGen(). Not ISR-safe.
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGetSize(size_t * p_size);

/* ── Operational functions ────────────────────────────────────────────────── */

/**
 * @brief Enqueue raw bytes for transmission.
 *
 * Non-blocking. Returns immediately. Copies bytes into the TX ring buffer
 * using an all-or-nothing policy (Architecture & Design Section 13.3): if
 * the full request does not fit, nothing is written and
 * RS_BB_UART_TX_STATUS_TX_BUFFER_FULL is returned. Starts (or continues)
 * asynchronous transmission via the GPT timer if data is queued.
 *
 * Success means the data was accepted into the transmit queue — NOT that it
 * has completed transmission on the TX pin.
 *
 * @param[in] p_data  Bytes to enqueue.
 * @param[in] length  Number of bytes to enqueue.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS          All bytes accepted into the queue.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER  p_data is NULL (and length > 0).
 * @retval RS_BB_UART_TX_STATUS_INVALID_STATE    Bit-Banged UART is not READY/TRANSMITTING
 *                                               (ConfigCheck() has not succeeded yet, or Bit-Banged UART is in ERROR).
 * @retval RS_BB_UART_TX_STATUS_TX_BUFFER_FULL   Insufficient free space for the whole request.
 *
 * @note Valid from startup context (after ConfigCheck() succeeds) and
 *       bare-metal main-loop/task context. NOT valid from ISR context in
 *       Phase 1 (Architecture & Design Section 17.1).
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_Write(uint8_t const * p_data, uint32_t length);

/**
 * @brief Enqueue a NUL-terminated ASCII string for transmission.
 *
 * Non-blocking. Calculates the string length up to (but not including) the
 * terminating NUL byte, then applies the same all-or-nothing buffer policy
 * as rs_bb_uart_tx_Write() (Architecture & Design Section 13.4). The
 * terminating NUL is never transmitted.
 *
 * @param[in] p_str  NUL-terminated ASCII string to enqueue.
 *
 * @retval RS_BB_UART_TX_STATUS_SUCCESS          All characters accepted into the queue.
 * @retval RS_BB_UART_TX_STATUS_INVALID_POINTER  p_str is NULL.
 * @retval RS_BB_UART_TX_STATUS_INVALID_STATE    Bit-Banged UART is not READY/TRANSMITTING.
 * @retval RS_BB_UART_TX_STATUS_TX_BUFFER_FULL   Insufficient free space for the whole string.
 *
 * @note Same calling-context restriction as rs_bb_uart_tx_Write().
 */
rs_bb_uart_tx_status_t rs_bb_uart_tx_WriteString(char const * p_str);

/**
 * @brief Get a snapshot of Bit-Banged UART TX runtime status and diagnostics.
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
rs_bb_uart_tx_status_t rs_bb_uart_tx_StatusGet(rs_bb_uart_tx_state_t * p_state);

#ifdef __cplusplus
}
#endif

#endif /* RS_BB_UART_TX_H */
