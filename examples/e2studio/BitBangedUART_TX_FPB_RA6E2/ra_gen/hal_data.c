/* generated HAL source file - do not edit
 *
 * g_timer0_extend / g_timer0_cfg below are copied field-for-field from a
 * real RASC-generated FPB-RA6E2 bare-metal + GPT project
 * (VSCode_FPB_RA4E2_Baremetal reference) — every gpt_extended_cfg_t field
 * (including compare_match_c/d/e/f_ipl/irq, compare_match_value[],
 * compare_match_status, gtior_setting, gtioca/gtiocb_polarity) is present
 * and verified-correct, not hand-guessed. Three things are adapted from
 * that reference for this PES's use:
 *   - cycle_end_ipl: BSP_IRQ_DISABLED -> RS_BB_UART_TX_CFG_GPT_IPL (interrupt enabled)
 *   - p_callback:    NULL -> rs_bb_uart_tx_hal_gpt_callback
 *   - p_context:     NULL -> &g_bb_uart_tx_ctrl
 * period_counts/duty_cycle_counts below are placeholders (as in the
 * reference) — rs_bb_uart_tx_hal_gpt_open() overwrites period_counts at
 * runtime via R_GPT_PeriodSet() based on the configured baud rate
 * (Architecture & Design Section 14.2); duty_cycle_counts is irrelevant
 * here since GTIOCA/GTIOCB output is disabled (this PES drives the TX pin
 * via direct GPIO control, not GPT's own pin hardware).
 *
 * See README.md "GPT Interrupt Setup" for the corresponding
 * configuration.xml / Smart Configurator settings if you regenerate this
 * project via RASC. */
#include "hal_data.h"
#include <rs_bb_uart_tx/rs_bb_uart_tx_cfg.h>   /* RS_BB_UART_TX_CFG_GPT_IPL */
#include <rs_bb_uart_tx/rs_bb_uart_tx_hal.h>   /* rs_bb_uart_tx_hal_gpt_callback */
#include <rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h>  /* g_bb_uart_tx_ctrl */

gpt_instance_ctrl_t g_timer0_ctrl;

const gpt_extended_cfg_t g_timer0_extend =
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
     .compare_match_value = { (uint32_t)0x0, /* CMP_A */(uint32_t)0x0, /* CMP_B */(uint32_t)0x0, /* CMP_C */(uint32_t)0x0, /* CMP_D */(uint32_t)0x0, /* CMP_E */(uint32_t)0x0, /* CMP_F */ }, .compare_match_status = ((0U << 5U) | (0U << 4U) | (0U << 3U) | (0U << 2U) | (0U << 1U) | 0U),
    .capture_filter_gtioca = GPT_CAPTURE_FILTER_NONE,
    .capture_filter_gtiocb = GPT_CAPTURE_FILTER_NONE,
    .p_pwm_cfg             = NULL,
    .gtior_setting.gtior   = 0U,

    .gtioca_polarity = GPT_GTIOC_POLARITY_NORMAL,
    .gtiocb_polarity = GPT_GTIOC_POLARITY_NORMAL,
};

const timer_cfg_t g_timer0_cfg =
{
    .mode                = TIMER_MODE_PERIODIC,
    /* Placeholder — actual period is computed and applied at runtime by
     * rs_bb_uart_tx_hal_gpt_open() from the configured baud rate. */
    .period_counts       = (uint32_t) 0x10000,
    .duty_cycle_counts   = 0x8000,
    /* PCLKD/4 (see README.md "GPT Clock Divider") — the RA4E2 has no
     * 32-bit GPT channels (BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0), so an
     * undivided ~100 MHz PCLKD would overflow the 16-bit counter at low
     * baud rates (100 MHz / 1200 baud ~= 83,333 counts > 65,535). /4 keeps
     * period_counts within 16-bit range across the full 1200-115200 baud
     * range while retaining adequate resolution at 115200 baud
     * (~217 counts). */
    .source_div          = TIMER_SOURCE_DIV_4,
    .channel             = 0,
    .p_callback          = rs_bb_uart_tx_hal_gpt_callback,
    .p_context           = &g_bb_uart_tx_ctrl,
    .p_extend            = &g_timer0_extend,
    .cycle_end_ipl       = (RS_BB_UART_TX_CFG_GPT_IPL),
#if defined(VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW)
    .cycle_end_irq       = VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW,
#else
    .cycle_end_irq       = FSP_INVALID_VECTOR,
#endif
};
/* Instance structure to use this module. */
const timer_instance_t g_timer0 =
{
    .p_ctrl        = &g_timer0_ctrl,
    .p_cfg         = &g_timer0_cfg,
    .p_api         = &g_timer_on_gpt
};

void g_hal_init(void)
{
    g_common_init();
}
