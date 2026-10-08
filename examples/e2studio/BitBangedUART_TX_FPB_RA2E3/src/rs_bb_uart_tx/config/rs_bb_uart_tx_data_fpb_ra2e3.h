/***********************************************************************************************************************
 * Pre-set configuration header — do not edit
 *
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_bb_uart_tx_data_fpb_ra2e3.h
 * @brief Pre-set HAL instance declarations for the Bit-Banged UART TX — FPB-RA2E3.
 *
 * Hardware mapping (FPB-RA2E3, R7FA2E3073CFL, Architecture & Design v0.3
 * Section 15.1/7.2 — verified against a real e2 studio RASC-generated
 * FPB-RA2E3 bare-metal + GPT reference project; NOT assumed identical to
 * FPB-RA4E2/FPB-RA6E2, and several facts genuinely differ from both of
 * those boards — see below):
 *   TX pin : P109 (BSP_IO_PORT_01_PIN_09) — per explicit instruction for
 *            this port (board VCOM TXD path; symbolic name
 *            "ARDUINO_D1_TX" in this board's pin definitions). Default
 *            board assignment is SCI9 TXD9 (paired with P110 = SCI9
 *            RXD9, SCI9 mode "Simple I2C") — NOT SCI0 the way P411 was on
 *            FPB-RA4E2/FPB-RA6E2. Reassigned to plain GPIO output here
 *            for the same reason as the other boards: this Bit-Banged UART bit-bangs
 *            the frame in software and does not use any hardware SCI
 *            channel.
 *   Timer  : GPT channel 0, periodic interrupt mode, **undivided PCLKD**
 *            (TIMER_SOURCE_DIV_1) — deliberately different from
 *            FPB-RA4E2/FPB-RA6E2's PCLKD/4:
 *              - This board's clock tree has no PLL — HOCO (48 MHz) feeds
 *                PCLKD directly at /1, giving PCLKD = 48 MHz (not the
 *                ~100 MHz PCLKD/4=25 MHz effective rate used on the other
 *                two boards).
 *              - At 48 MHz undivided, even the lowest supported baud rate
 *                (1200) needs only 40,000 counts — comfortably within
 *                16-bit range (65,535), so no divider is required for
 *                overflow safety here at all.
 *              - R7FA2E307's GPT channel 0 is additionally a genuine
 *                32-bit channel (BSP_FEATURE_GPT_32BIT_CHANNEL_MASK ==
 *                0x01 in ra/fsp/src/bsp/mcu/ra2e3/bsp_feature.h — bit 0
 *                set), unlike FPB-RA4E2/FPB-RA6E2 (both 0x00, no 32-bit
 *                channels), reinforcing that no divider is needed.
 *              - Leaving the clock undivided also gives *better* timing
 *                resolution than the other two boards: ~417 counts at
 *                115200 baud here, versus ~217 counts on FPB-RA4E2/
 *                FPB-RA6E2's divided clock.
 *            See g_timer0_cfg_preset below and README.md "GPT Clock
 *            Divider" for the full explanation.
 *
 * GPT driver source: ra/fsp/src/r_gpt/r_gpt.c (Renesas-vendored middleware)
 * IS included in this delivery — no Smart Configurator step is required
 * to build this project. If you do open/regenerate via the Smart
 * Configurator for other reasons, see README.md "GPT Interrupt Setup" for
 * the settings that must be preserved (interrupt enabled, callback name,
 * P109 pin mode, and SCI9's Operation Mode disabled — see the "SCI9
 * pairing" note in README.md). the FPB-RA4E2/FPB-RA6E2 have the equivalent SCI0 pairing constraint.
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
 * IMPORTANT — R_BSP_PinWrite() requires PFS access enable/disable around
 * every call on this MCU/FSP combination (writes go to the same
 * write-protected PmnPFS register R_BSP_PinCfg() uses, not a separate
 * always-writable output-data register). This is handled generically in
 * rs_bb_uart_tx_hal.c (board-agnostic, unchanged from FPB-RA4E2/FPB-RA6E2)
 * — see that file's comments for the full explanation. The mechanism
 * (`bsp_io.h`, `PWPR`/write-protect) is BSP-family-level, not
 * board-specific, so it applies identically here.
 *
 * Usage:
 *   #if RS_BB_UART_TX_CFG_PRESET_CONFIG_USE
 *   #include "rs_bb_uart_tx_data_fpb_ra2e3.h"
 *   #endif
 *   ...
 *   #if RS_BB_UART_TX_CFG_PRESET_CONFIG_USE
 *     rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0_preset, BSP_IO_PORT_01_PIN_09);
 *   #else
 *     rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0, BSP_IO_PORT_01_PIN_09);
 *   #endif
 *   rs_bb_uart_tx_ConfigCheck(&g_cfg);   // validates AND commits/opens on success
 **********************************************************************************************************************/

#ifndef RS_BB_UART_TX_DATA_FPB_RA2E3_H
#define RS_BB_UART_TX_DATA_FPB_RA2E3_H

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
 * Bit-Banged UART TX configuration bridge functions — FPB-RA2E3
 *
 * rs_bb_uart_tx_ConfigGen() and rs_bb_uart_tx_ConfigCheck() are declared in
 * the public rs_bb_uart_tx.h (include that header, not this one, from
 * application code) and DEFINED in rs_bb_uart_tx_data_fpb_ra2e3.c —
 * compiled unconditionally so both the preset path and the FSP Stack
 * Configurator path can call them regardless of
 * RS_BB_UART_TX_CFG_PRESET_CONFIG_USE.
 **********************************************************************************************************************/

#endif /* RS_BB_UART_TX_DATA_FPB_RA2E3_H */
