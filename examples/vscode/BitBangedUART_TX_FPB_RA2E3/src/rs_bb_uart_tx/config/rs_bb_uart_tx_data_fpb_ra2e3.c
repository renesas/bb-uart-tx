/***********************************************************************************************************************
 * Pre-set configuration source — do not edit
 *
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file rs_bb_uart_tx_data_fpb_ra2e3.c
 * @brief Pre-set HAL instance definitions for the Bit-Banged UART TX — FPB-RA2E3.
 *
 * See rs_bb_uart_tx_data_fpb_ra2e3.h for the hardware mapping, the GPT
 * driver source dependency note, and usage.
 **********************************************************************************************************************/

#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_hal.h>
#include <string.h>

#if RS_BB_UART_TX_CFG_PRESET_CONFIG_USE

/***********************************************************************************************************************
 * GPT preset instance — FPB-RA2E3 (standalone, no ra_gen dependency)
 *
 * Field-for-field verified against a real RASC-generated FPB-RA2E3
 * bare-metal + GPT project (e2 studio FPB-RA2E3 blinky+GPT reference,
 * identical field layout to the verified FPB-RA4E2 delivery) — every
 * gpt_extended_cfg_t field (including compare_match_c/d/e/f_ipl/irq,
 * compare_match_value[], compare_match_status, gtior_setting,
 * gtioca/gtiocb_polarity) is present below, not hand-guessed.
 *
 * Interrupt-only periodic timer: both GTIOC pins left unused/not output
 * (this Bit-Banged UART drives the TX line via direct GPIO control, not GPT's own pin
 * hardware). period_counts here is a placeholder — rs_bb_uart_tx_hal_gpt_open()
 * overwrites it at runtime via R_GPT_PeriodSet() based on the configured
 * baud rate (Architecture & Design Section 14.2).
 *
 * source_div: DIFFERENT from FPB-RA4E2/FPB-RA6E2 — this board's clock tree
 * has no PLL (HOCO 48 MHz feeds PCLKD directly at /1, so PCLKD = 48 MHz).
 * At 48 MHz undivided, even the lowest supported baud rate (1200) needs
 * only 40,000 counts, comfortably within 16-bit range (65,535) with no
 * divider at all — unlike FPB-RA4E2/FPB-RA6E2, which needed PCLKD/4 to
 * avoid overflowing their 16-bit-only GPT channels at a much higher
 * undivided PCLKD (~100 MHz). R7FA2E307's GPT channel 0 is additionally a
 * genuine 32-bit channel (BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0x01 in
 * ra/fsp/src/bsp/mcu/ra2e3/bsp_feature.h), reinforcing that no divider is
 * needed here. TIMER_SOURCE_DIV_1 (undivided) also gives *better*
 * resolution than the other two boards: ~417 counts at 115200 baud here,
 * versus ~217 counts on FPB-RA4E2/FPB-RA6E2's divided clock. See
 * README.md "GPT Clock Divider".
 **********************************************************************************************************************/

gpt_instance_ctrl_t g_timer0_ctrl_preset;

static gpt_extended_cfg_t const g_timer0_extend_preset =
{
    .gtioca = { .output_enabled = false,
                .stop_level     = GPT_PIN_LEVEL_LOW
              },
    .gtiocb = { .output_enabled = false,
                .stop_level     = GPT_PIN_LEVEL_LOW
              },
    .start_source        = (gpt_source_t) ( GPT_SOURCE_NONE),
    .stop_source         = (gpt_source_t) ( GPT_SOURCE_NONE),
    .clear_source        = (gpt_source_t) ( GPT_SOURCE_NONE),
    .count_up_source     = (gpt_source_t) ( GPT_SOURCE_NONE),
    .count_down_source   = (gpt_source_t) ( GPT_SOURCE_NONE),
    .capture_a_source    = (gpt_source_t) ( GPT_SOURCE_NONE),
    .capture_b_source    = (gpt_source_t) ( GPT_SOURCE_NONE),
    .capture_a_ipl       = (BSP_IRQ_DISABLED),
    .capture_b_ipl       = (BSP_IRQ_DISABLED),
    .compare_match_c_ipl = (BSP_IRQ_DISABLED),
    .compare_match_d_ipl = (BSP_IRQ_DISABLED),
    .compare_match_e_ipl = (BSP_IRQ_DISABLED),
    .compare_match_f_ipl = (BSP_IRQ_DISABLED),
#if defined(VECTOR_NUMBER_GPT0_CAPTURE_COMPARE_A)
    .capture_a_irq         = VECTOR_NUMBER_GPT0_CAPTURE_COMPARE_A,
#else
    .capture_a_irq         = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT0_CAPTURE_COMPARE_B)
    .capture_b_irq         = VECTOR_NUMBER_GPT0_CAPTURE_COMPARE_B,
#else
    .capture_b_irq         = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT0_COMPARE_C)
    .compare_match_c_irq   = VECTOR_NUMBER_GPT0_COMPARE_C,
#else
    .compare_match_c_irq   = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT0_COMPARE_D)
    .compare_match_d_irq   = VECTOR_NUMBER_GPT0_COMPARE_D,
#else
    .compare_match_d_irq   = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT0_COMPARE_E)
    .compare_match_e_irq   = VECTOR_NUMBER_GPT0_COMPARE_E,
#else
    .compare_match_e_irq   = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_GPT0_COMPARE_F)
    .compare_match_f_irq   = VECTOR_NUMBER_GPT0_COMPARE_F,
#else
    .compare_match_f_irq   = FSP_INVALID_VECTOR,
#endif
     .compare_match_value = { (uint32_t) 0x0, /* CMP_A */ (uint32_t) 0x0, /* CMP_B */ (uint32_t) 0x0, /* CMP_C */
                              (uint32_t) 0x0, /* CMP_D */ (uint32_t) 0x0, /* CMP_E */ (uint32_t) 0x0  /* CMP_F */ },
     .compare_match_status = ((0U << 5U) | (0U << 4U) | (0U << 3U) | (0U << 2U) | (0U << 1U) | 0U),
    .capture_filter_gtioca = GPT_CAPTURE_FILTER_NONE,
    .capture_filter_gtiocb = GPT_CAPTURE_FILTER_NONE,
    .p_pwm_cfg             = NULL,
    .gtior_setting.gtior   = 0U,

    .gtioca_polarity = GPT_GTIOC_POLARITY_NORMAL,
    .gtiocb_polarity = GPT_GTIOC_POLARITY_NORMAL,
};

static timer_cfg_t const g_timer0_cfg_preset =
{
    .mode               = TIMER_MODE_PERIODIC,
    /* Placeholder — actual period is computed and applied at runtime by
     * rs_bb_uart_tx_hal_gpt_open() from the configured baud rate. */
    .period_counts      = (uint32_t) 0x10000,
    .duty_cycle_counts  = 0x8000,
    .source_div         = TIMER_SOURCE_DIV_1, /* undivided PCLKD (48 MHz) — see note above */
    .channel            = 0U,
    .p_callback         = rs_bb_uart_tx_hal_gpt_callback,
    .p_context          = &g_bb_uart_tx_ctrl,
    .p_extend           = &g_timer0_extend_preset,
    .cycle_end_ipl       = (1U), /* NOT RS_BB_UART_TX_CFG_GPT_IPL (6) — invalid on this board's
                                   * Cortex-M23 core (only 2 NVIC priority bits implemented, valid
                                   * range 0-3). See the field's twin in ra_gen/hal_data.c and this
                                   * file's top-of-file comment for the full explanation. */
#if defined(VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW)
    .cycle_end_irq      = VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW,
#else
    .cycle_end_irq      = FSP_INVALID_VECTOR,
#endif
};

const timer_instance_t g_timer0_preset =
{
    .p_ctrl = &g_timer0_ctrl_preset,
    .p_cfg  = &g_timer0_cfg_preset,
    .p_api  = &g_timer_on_gpt,
};

#endif /* RS_BB_UART_TX_CFG_PRESET_CONFIG_USE */

/***********************************************************************************************************************
 * Param-checking macro (mirrors rs_bb_uart_tx.c convention; scoped to this file)
 **********************************************************************************************************************/

#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
 #define RS_BB_UART_TX_DATA_ASSERT_RETURN(expr, err)  do { if (!(expr)) { return (err); } } while (0)
#else
 #define RS_BB_UART_TX_DATA_ASSERT_RETURN(expr, err)  ((void) 0)
#endif

/***********************************************************************************************************************
 * Bit-Banged UART TX configuration bridge functions — FPB-RA2E3
 *
 * Compiled unconditionally so both the preset path and the FSP Stack
 * Configurator path can call these functions.
 **********************************************************************************************************************/

/*****************************************************************************************************************
 * @brief       Assign the GPT HAL instance and TX pin to a Bit-Banged UART TX configuration structure.
 * @param[out]  p_cfg           Configuration structure to populate.
 * @param[in]   p_gpt_instance  GPT HAL instance pointer (preset or FSP-generated).
 * @param[in]   tx_pin          GPIO pin to use as the TX output.
 * @retval      RS_BB_UART_TX_STATUS_SUCCESS         Configuration populated.
 * @retval      RS_BB_UART_TX_STATUS_INVALID_POINTER p_cfg or p_gpt_instance is NULL.
 ****************************************************************************************************************/
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGen(rs_bb_uart_tx_cfg_t     * p_cfg,
                                                timer_instance_t const * p_gpt_instance,
                                                bsp_io_port_pin_t         tx_pin)
{
#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
    RS_BB_UART_TX_DATA_ASSERT_RETURN(NULL != p_cfg,          RS_BB_UART_TX_STATUS_INVALID_POINTER);
    RS_BB_UART_TX_DATA_ASSERT_RETURN(NULL != p_gpt_instance, RS_BB_UART_TX_STATUS_INVALID_POINTER);
#endif

    memset(p_cfg, 0, sizeof(*p_cfg));

    p_cfg->version     = RS_BB_UART_TX_VERSION;
    p_cfg->size        = sizeof(*p_cfg);

    p_cfg->baud_rate    = RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE;
    p_cfg->buffer_size  = RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES;
    p_cfg->data_bits    = RS_BB_UART_TX_DATA_BITS;
    p_cfg->parity       = 0U;                       /* None — fixed in Phase 1. */
    p_cfg->stop_bits    = RS_BB_UART_TX_STOP_BITS;
    p_cfg->overflow_policy = RS_BB_UART_TX_CFG_OVERFLOW_POLICY_RETURN_ERROR;
    /* NOT RS_BB_UART_TX_CFG_GPT_IPL (6) — this is purely informational/status
     * metadata (rs_bb_uart_tx_cfg_t.irq_priority is never read back to
     * configure hardware), but it must still match the ACTUAL priority
     * programmed into g_timer0_cfg_preset/g_timer0_cfg's cycle_end_ipl
     * below/in ra_gen/hal_data.c, or StatusGet()-style diagnostics would
     * report a misleading value. This board's Cortex-M23 core only
     * implements 2 NVIC priority bits (valid range 0-3) — see this file's
     * top-of-file comment. */
    p_cfg->irq_priority = (uint8_t) 1U;

    p_cfg->p_gpt_instance = p_gpt_instance;
    p_cfg->p_gpio_instance = NULL; /* Reserved; direct pin control path (Phase 1) needs no instance. */
    p_cfg->tx_pin          = tx_pin;

    return RS_BB_UART_TX_STATUS_SUCCESS;
}

/*****************************************************************************************************************
 * @brief       Validate a Bit-Banged UART TX configuration, and on success commit/open it.
 * @param[in]   p_cfg  Configuration structure populated by rs_bb_uart_tx_ConfigGen().
 * @retval      RS_BB_UART_TX_STATUS_SUCCESS                Configuration valid; Bit-Banged UART is now READY.
 * @retval      RS_BB_UART_TX_STATUS_INVALID_POINTER        p_cfg or p_cfg->p_gpt_instance is NULL.
 * @retval      RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE  baud_rate outside the supported range.
 * @retval      RS_BB_UART_TX_STATUS_INVALID_STATE          Bit-Banged UART is already READY/TRANSMITTING/ERROR.
 * @retval      RS_BB_UART_TX_STATUS_TIMER_ERROR            GPT HAL open/period-set failed.
 * @retval      RS_BB_UART_TX_STATUS_GPIO_ERROR              GPIO pin configuration failed.
 ****************************************************************************************************************/
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigCheck(rs_bb_uart_tx_cfg_t const * p_cfg)
{
#if RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE
    RS_BB_UART_TX_DATA_ASSERT_RETURN(NULL != p_cfg,                  RS_BB_UART_TX_STATUS_INVALID_POINTER);
    RS_BB_UART_TX_DATA_ASSERT_RETURN(NULL != p_cfg->p_gpt_instance,  RS_BB_UART_TX_STATUS_INVALID_POINTER);
#endif

    if ((p_cfg->baud_rate < RS_BB_UART_TX_CFG_MIN_BAUD_RATE)
        || (p_cfg->baud_rate > RS_BB_UART_TX_CFG_MAX_BAUD_RATE))
    {
        return RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE;
    }

    if (p_cfg->buffer_size != RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES)
    {
        return RS_BB_UART_TX_STATUS_INVALID_BUFFER_SIZE;
    }

    /* Validation passed — commit and open (see rs_bb_uart_tx.h top-of-file
     * note for why ConfigCheck() also performs this step). */
    return rs_bb_uart_tx_open_internal(p_cfg);
}
