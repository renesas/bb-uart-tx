/* generated HAL header file - do not edit
 *
 * Base structure verified against a real RASC-generated FPB-RA4E2 bare-metal
 * + GPT project (VSCode_FPB_RA4E2_Baremetal reference). Adapted here only in
 * that g_timer0's callback is wired to rs_bb_uart_tx_hal_gpt_callback and its
 * interrupt is enabled — see hal_data.c and README.md "GPT Interrupt Setup". */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_gpt.h"
#include "r_timer_api.h"
FSP_HEADER
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer0;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer0_ctrl;
extern const timer_cfg_t g_timer0_cfg;

void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */
