/***********************************************************************************************************************
 * Pre-set configuration header — do not edit
 *
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_bb_uart_tx_data_fpb_ra4e2.h
 * @brief Pre-set HAL instance declarations for the Bit-Banged UART TX — FPB-RA4E2.
 *
 * Hardware mapping (FPB-RA4E2, Architecture & Design v0.3 Section 15.1/7.2):
 *   TX pin : P411 (board VCOM TXD0 path) — on-board J-Link VCOM performs
 *            the TTL-to-USB conversion; no external adapter needed for the
 *            default demo.
 *   Timer  : GPT channel 0, periodic interrupt mode, PCLKD/4 clock divider.
 *            R7FA4E2B9 (RA4E2) has NO 32-bit GPT channels
 *            (BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0 — every channel is
 *            16-bit), so the /4 divider is required to keep period_counts
 *            within 16-bit range across the full 1200-115200 baud range
 *            (undivided ~100 MHz PCLKD would need ~83,333 counts at 1200
 *            baud, which overflows a 16-bit counter). See
 *            g_timer0_cfg_preset below and README.md "GPT Clock Divider".
 *
 * GPT driver source: ra/fsp/src/r_gpt/r_gpt.c (Renesas-vendored middleware)
 * IS included in this delivery, verified field-for-field against a real
 * RASC-generated FPB-RA4E2 bare-metal + GPT project — see the comment block
 * above g_timer0_extend_preset below. No Smart Configurator step is
 * required to build this project. If you do open/regenerate via the Smart
 * Configurator for other reasons, see README.md "GPT Interrupt Setup" for
 * the settings that must be preserved (interrupt enabled, callback name,
 * P411 pin mode).
 *
 * g_timer0_preset (compiled only when RS_BB_UART_TX_CFG_PRESET_CONFIG_USE == 1,
 * default) is a standalone r_gpt instance — NOT an alias for ra_gen's
 * Configurator-style g_timer0 (ra_gen/hal_data.c, also included and fully
 * wired in this delivery as the alternative FSP Stack Configurator path,
 * selected via RS_BB_UART_TX_CFG_PRESET_CONFIG_USE == 0). Its period_counts
 * placeholder is overwritten at runtime by rs_bb_uart_tx_hal_gpt_open()
 * based on the configured baud rate (Architecture & Design Section 14.2),
 * so the value baked into g_timer0_cfg_preset below is never actually used
 * as-is.
 *
 * The gpt_extended_cfg_t fields below configure GPT for interrupt-only
 * periodic operation with both GTIOC pins left unused (this Bit-Banged UART drives the
 * TX pin as a plain GPIO output via rs_bb_uart_tx_hal_gpio_*(), not through
 * GPT's own PWM/compare-match pin hardware).
 *
 * Usage:
 *   #if RS_BB_UART_TX_CFG_PRESET_CONFIG_USE
 *   #include "rs_bb_uart_tx_data_fpb_ra4e2.h"
 *   #endif
 *   ...
 *   #if RS_BB_UART_TX_CFG_PRESET_CONFIG_USE
 *     rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0_preset, BSP_IO_PORT_04_PIN_11);
 *   #else
 *     rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0, BSP_IO_PORT_04_PIN_11);
 *   #endif
 *   rs_bb_uart_tx_ConfigCheck(&g_cfg);   // validates AND commits/opens on success
 **********************************************************************************************************************/

#ifndef RS_BB_UART_TX_DATA_FPB_RA4E2_H
#define RS_BB_UART_TX_DATA_FPB_RA4E2_H

#include <stddef.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_cfg.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_types.h>  /* rs_bb_uart_tx_cfg_t, rs_bb_uart_tx_status_t,
                                                   * timer_instance_t, bsp_io_port_pin_t */

#if RS_BB_UART_TX_CFG_PRESET_CONFIG_USE

#include "r_gpt.h"
#include "r_timer_api.h"

FSP_HEADER

/***********************************************************************************************************************
 * GPT preset instance — standalone, no ra_gen dependency for its cfg
 * contents. The vectored ISR entry (VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW)
 * comes from vector_data.h, which IS correctly populated in this delivery
 * (ra_gen/vector_data.h/.c) — see README.md "GPT Interrupt Setup".
 **********************************************************************************************************************/
extern gpt_instance_ctrl_t g_timer0_ctrl_preset;
extern const timer_instance_t g_timer0_preset;

FSP_FOOTER

#endif /* RS_BB_UART_TX_CFG_PRESET_CONFIG_USE */

/***********************************************************************************************************************
 * Bit-Banged UART TX configuration bridge functions — FPB-RA4E2
 *
 * rs_bb_uart_tx_ConfigGen() and rs_bb_uart_tx_ConfigCheck() are declared in
 * the public rs_bb_uart_tx.h (include that header, not this one, from
 * application code) and DEFINED in rs_bb_uart_tx_data_fpb_ra4e2.c —
 * compiled unconditionally so both the preset path and the FSP Stack
 * Configurator path can call them regardless of
 * RS_BB_UART_TX_CFG_PRESET_CONFIG_USE.
 **********************************************************************************************************************/

#endif /* RS_BB_UART_TX_DATA_FPB_RA4E2_H */
