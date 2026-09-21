/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_bb_uart_tx_test_types.h
 * @brief Host-build type stubs for the Bit-Banged UART TX unit tests.
 *
 * Replaces bsp_api.h and r_timer_api.h when RS_BB_UART_TX_TEST_HOST_BUILD is
 * defined (see rs_bb_uart_tx_types.h).  Every type below mirrors the layout
 * and member names of the real FSP v6.x declaration (r_timer_api.h,
 * bsp_io.h, fsp_common_api.h) so the PES compiles unchanged on the host.
 *
 * Deliberate differences from real FSP:
 *   - timer_api_t only lists the members the PES calls (open/start/stop/
 *     reset/periodSet/infoGet/close).  Fakes must use designated
 *     initialisers so the same fake source also compiles against the full
 *     FSP struct.
 *   - R_BSP_PinAccessEnable/Disable/PinCfg/PinWrite are __STATIC_INLINE in
 *     the real BSP.  Here they are ordinary external functions, implemented
 *     by tests/stubs/rs_bb_uart_tx_fake_fsp.c so tests can record the pin.
 *
 * Do NOT include FSP headers here — this file is intentionally self-contained.
 **********************************************************************************************************************/

#ifndef RS_BB_UART_TX_TEST_TYPES_H
#define RS_BB_UART_TX_TEST_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── FSP_HEADER / FSP_FOOTER macros (no-ops on host) ───────────────────── */

#ifndef FSP_HEADER
  #define FSP_HEADER
#endif
#ifndef FSP_FOOTER
  #define FSP_FOOTER
#endif

/* ── FSP error codes (values copied from fsp_common_api.h) ──────────────── */

typedef enum e_fsp_err
{
    FSP_SUCCESS                  = 0,
    FSP_ERR_ASSERTION            = 1,
    FSP_ERR_INVALID_ARGUMENT     = 3,
    FSP_ERR_NOT_OPEN             = 7,
    FSP_ERR_IN_USE               = 8,
    FSP_ERR_INVALID_HW_CONDITION = 27,
} fsp_err_t;

typedef int32_t IRQn_Type;

/* ── Timer API (subset of r_timer_api.h) ─────────────────────────────────── */

typedef enum e_timer_event
{
    TIMER_EVENT_CYCLE_END = 0,
} timer_event_t;

typedef enum e_timer_mode
{
    TIMER_MODE_PERIODIC = 0,
} timer_mode_t;

typedef enum e_timer_source_div
{
    TIMER_SOURCE_DIV_1 = 0,
    TIMER_SOURCE_DIV_4 = 2,
} timer_source_div_t;

typedef enum e_timer_direction
{
    TIMER_DIRECTION_DOWN = 0,
    TIMER_DIRECTION_UP   = 1,
} timer_direction_t;

typedef void timer_ctrl_t;

/** Callback parameter data — member order matches FSP (p_context first). */
typedef struct st_timer_callback_args
{
    void        * p_context;
    timer_event_t event;
    uint32_t      capture;
} timer_callback_args_t;

typedef struct st_timer_info
{
    timer_direction_t count_direction;
    uint32_t          clock_frequency;   ///< Clock frequency of the timer counter.
    uint32_t          period_counts;
} timer_info_t;

typedef struct st_timer_cfg
{
    timer_mode_t       mode;
    uint32_t           period_counts;
    timer_source_div_t source_div;
    uint32_t           duty_cycle_counts;
    uint8_t            channel;
    uint8_t            cycle_end_ipl;
    IRQn_Type          cycle_end_irq;
    void (* p_callback)(timer_callback_args_t * p_args);
    void       * p_context;
    void const * p_extend;
} timer_cfg_t;

typedef struct st_timer_api
{
    fsp_err_t (* open)(timer_ctrl_t * const p_ctrl, timer_cfg_t const * const p_cfg);
    fsp_err_t (* start)(timer_ctrl_t * const p_ctrl);
    fsp_err_t (* stop)(timer_ctrl_t * const p_ctrl);
    fsp_err_t (* reset)(timer_ctrl_t * const p_ctrl);
    fsp_err_t (* periodSet)(timer_ctrl_t * const p_ctrl, uint32_t const period);
    fsp_err_t (* infoGet)(timer_ctrl_t * const p_ctrl, timer_info_t * const p_info);
    fsp_err_t (* close)(timer_ctrl_t * const p_ctrl);
} timer_api_t;

typedef struct st_timer_instance
{
    timer_ctrl_t      * p_ctrl;
    timer_cfg_t const * p_cfg;
    timer_api_t const * p_api;
} timer_instance_t;

/* ── BSP pin types (subset of bsp_io.h) ──────────────────────────────────── */

typedef enum e_bsp_io_level
{
    BSP_IO_LEVEL_LOW = 0,
    BSP_IO_LEVEL_HIGH
} bsp_io_level_t;

typedef enum e_bsp_io_port_pin_t
{
    BSP_IO_PORT_00_PIN_00 = 0x0000,
    BSP_IO_PORT_01_PIN_09 = 0x0109,   ///< FPB-RA2E3 VCOM TXD
    BSP_IO_PORT_04_PIN_11 = 0x040B,   ///< FPB-RA4E2 / FPB-RA6E2 VCOM TXD
} bsp_io_port_pin_t;

void R_BSP_PinAccessEnable(void);
void R_BSP_PinAccessDisable(void);
void R_BSP_PinCfg(bsp_io_port_pin_t pin, uint32_t cfg);
void R_BSP_PinWrite(bsp_io_port_pin_t pin, bsp_io_level_t level);

#endif /* RS_BB_UART_TX_TEST_TYPES_H */
