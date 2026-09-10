/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_cfg.h
 * @brief Build-time configuration constants for the Bit-Banged UART TX.
 *
 * All constants use #ifndef guards so user project overrides take precedence.
 * Override any constant by defining it before including this header, or by
 * adding -D<CONSTANT>=<VALUE> to the compiler command line.
 *
 * Traceability: values below reflect the accepted MRD clarifications in
 * "Bit-Banged UART - Software driven TX Only UART Implementation
 * (Phase-1 Common Bit-Banged UART)" Section 6.2, and the Architecture & Design v0.3
 * document Sections 5.3-5.11.
 */

#ifndef RS_BB_UART_TX_CFG_H
#define RS_BB_UART_TX_CFG_H

/***********************************************************************************************************************
 * Frame format (FR-004 — fixed for Phase 1)
 **********************************************************************************************************************/

/** Phase 1 supports only 8N1 (8 data bits, no parity, 1 stop bit). Not overridable. */
#define RS_BB_UART_TX_DATA_BITS   (8U)
#define RS_BB_UART_TX_STOP_BITS   (1U)

/***********************************************************************************************************************
 * Baud rate
 **********************************************************************************************************************/

/** Default demo baud rate (design doc Section 5.3). */
#ifndef RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE
  #define RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE     (115200U)
#endif

/** Minimum supported baud rate (design doc Section 5.4). */
#ifndef RS_BB_UART_TX_CFG_MIN_BAUD_RATE
  #define RS_BB_UART_TX_CFG_MIN_BAUD_RATE          (1200U)
#endif

/** Maximum supported baud rate (design doc Section 5.4). */
#ifndef RS_BB_UART_TX_CFG_MAX_BAUD_RATE
  #define RS_BB_UART_TX_CFG_MAX_BAUD_RATE          (115200U)
#endif

/**
 * Required baud-rate timing accuracy, expressed as parts-per-thousand of the
 * configured baud rate (30 == 3.0%). Validated by rs_bb_uart_tx_ConfigCheck()
 * against the GPT-derived actual period vs. the ideal period.
 * Design doc Section 5.5: measured baud-rate error shall not exceed +/-3%.
 */
#ifndef RS_BB_UART_TX_CFG_BAUD_ERROR_PPT_MAX
  #define RS_BB_UART_TX_CFG_BAUD_ERROR_PPT_MAX     (30U)
#endif

/***********************************************************************************************************************
 * Ring buffer sizing (FR-021, design doc Section 5.7)
 **********************************************************************************************************************/

/**
 * Static TX ring buffer size in bytes. Supported reference sizes are 64, 128,
 * 256, 512, and 1024 (design doc Section 5.7). Unlike earlier deliveries,
 * this value MUST now be a power of two — enforced by a `_Static_assert` in
 * rs_bb_uart_tx_ring_buffer.h — because the ring buffer's lock-free
 * head/tail wraparound uses a bitmask (`& (SIZE - 1)`) rather than `%`. All
 * five reference sizes already satisfy this.
 *
 * Note: usable capacity is (this value - 1) bytes, not the full value — one
 * slot is reserved so head == tail remains an unambiguous "empty" signal
 * now that there is no separate shared `count` field. See
 * rs_bb_uart_tx_ring_buffer.h for the full rationale.
 *
 * Default is 512, not the design doc's nominal 256, because the demo
 * application's own startup banner (src/rs_bb_uart_tx_demo.c) is ~304
 * bytes sent as several back-to-back WriteString() calls before the GPT
 * timer has drained any of it — confirmed via hands-on hardware testing
 * that 256 causes rs_bb_uart_tx_StatusGet()'s buffer_full_count to read
 * non-zero (2 rejected calls) during every boot. See README.md's
 * "Troubleshooting" section and docs/README.md for the full write-up.
 */
#ifndef RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES
  #define RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES     (512U)
#endif

/***********************************************************************************************************************
 * Buffer-full policy (FR-022, design doc Section 5.8)
 **********************************************************************************************************************/

/**
 * Phase 1 buffer-full policy is fixed to "return error" (RS_BB_UART_TX_STATUS_TX_BUFFER_FULL).
 * The Bit-Banged UART shall not silently overwrite, silently drop, or block. Not overridable in Phase 1.
 */
#define RS_BB_UART_TX_CFG_OVERFLOW_POLICY_RETURN_ERROR (1U)

/***********************************************************************************************************************
 * Parameter checking
 **********************************************************************************************************************/

/**
 * Enable/disable parameter checking (mirrors FSP convention).
 * When BSP_CFG_PARAM_CHECKING_ENABLE is not defined (e.g. host-based unit tests),
 * defaults to enabled (1) for maximum safety.
 */
#ifndef RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
  #ifdef BSP_CFG_PARAM_CHECKING_ENABLE
    #define RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE  (BSP_CFG_PARAM_CHECKING_ENABLE)
  #else
    #define RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE  (1U)
  #endif
#endif

/***********************************************************************************************************************
 * Configuration path
 **********************************************************************************************************************/

/**
 * Configuration path selection.
 *
 * 1 = Preset configuration path (default):
 *     Include rs_bb_uart_tx_data_fpb_<board>.h and call
 *     rs_bb_uart_tx_ConfigGen() with the board's _preset GPT instance and
 *     pin mapping. No FSP Stack Configurator re-run strictly required for
 *     the GPIO side; the GPT side still needs the r_gpt driver source
 *     vendored once (see README.md / rs_bb_uart_tx_data_fpb_ra4e2.h).
 *
 * 0 = FSP Stack Configurator path:
 *     Configure a GPT timer stack (e.g. g_timer0) in e2 studio / RASC and
 *     call rs_bb_uart_tx_ConfigGen() with the Configurator-generated
 *     instance instead of the _preset.
 */
#ifndef RS_BB_UART_TX_CFG_PRESET_CONFIG_USE
  #define RS_BB_UART_TX_CFG_PRESET_CONFIG_USE    (1U)
#endif

/***********************************************************************************************************************
 * GPT interrupt priority
 **********************************************************************************************************************/

/**
 * Recommended default GPT bit-period interrupt priority (design doc Section 14.3:
 * "Medium-high priority", example value 6). Lower numeric value = higher
 * preemption priority on Cortex-M. This ISR must remain short (design doc
 * Section 12.3) — it performs only bit-period sequencing, never blocking work.
 */
#ifndef RS_BB_UART_TX_CFG_GPT_IPL
  #define RS_BB_UART_TX_CFG_GPT_IPL                (6U)
#endif

/***********************************************************************************************************************
 * Control-block RAM budget guard
 **********************************************************************************************************************/

/** Hard cap on control block RAM — build will fail (static_assert) if exceeded. */
#ifndef RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES
  #define RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES  (2048U)
#endif

#endif /* RS_BB_UART_TX_CFG_H */
