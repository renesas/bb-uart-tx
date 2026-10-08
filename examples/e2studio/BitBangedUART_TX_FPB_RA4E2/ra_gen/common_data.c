/* hand-authored common source file, modeled on RASC generated-file conventions —
 * regenerate/verify via the RA Smart Configurator before building on hardware. */
#include "common_data.h"

ioport_instance_ctrl_t g_ioport_ctrl;
const ioport_instance_t g_ioport =
{
    .p_api = &g_ioport_on_ioport,
    .p_ctrl = &g_ioport_ctrl,
    .p_cfg = &g_bsp_pin_cfg,
};

void g_common_init(void)
{
    /* No Configurator-managed auto-init instances in this project — the
     * Bit-Banged UART TX PES opens its own GPT/GPIO resources explicitly
     * from rs_bb_uart_tx_ConfigCheck() (see rs_bb_uart_tx_data_fpb_ra4e2.c). */
}
