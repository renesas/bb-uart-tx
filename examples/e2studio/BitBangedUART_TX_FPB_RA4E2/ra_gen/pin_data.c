/* hand-authored pin source file, modeled on RASC generated-file conventions —
 * regenerate/verify via the RA Smart Configurator before building on hardware
 * (see README.md "Before You Build"). */
#include "bsp_api.h"
#include "r_ioport.h"

/***********************************************************************************************************************
 * Minimal pin table for the Bit-Banged UART TX demo project.
 *
 * Unlike a full board-bring-up project, this example only requires one pin
 * to be explicitly configured: P411, the FPB-RA4E2 board's default VCOM
 * TXD path (Architecture & Design Section 7.2 / 15.1), driven here as a
 * plain GPIO output (NOT the SCI0 peripheral function — this PES bit-bangs
 * the UART frame in software, it does not use the hardware SCI channel).
 *
 * All other pins are left at their hardware reset defaults (unconfigured).
 * If your application also needs LEDs, buttons, debug pins, or other board
 * peripherals configured, regenerate this file via the RA Smart
 * Configurator, which will add those entries while preserving this P411
 * GPIO-output configuration (or re-add it manually — see the pin_cfg value
 * below).
 **********************************************************************************************************************/

const ioport_pin_cfg_t g_bsp_pin_cfg_data[] = {

    {
        /* P411 — FPB-RA4E2 board VCOM TXD path (default Bit-Banged UART TX pin).
         * Configured as a digital output, idling high (Architecture & Design
         * Section 16.1: "The TX pin shall idle high"). This initial state is
         * applied by R_IOPORT_Open() during R_BSP_WarmStart(); the PES's own
         * rs_bb_uart_tx_hal_gpio_open() re-asserts the same configuration
         * later during rs_bb_uart_tx_ConfigCheck(), so the pin is correctly
         * configured even if this table is regenerated without this entry. */
        .pin = BSP_IO_PORT_04_PIN_11,
        .pin_cfg = ((uint32_t) IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t) IOPORT_CFG_PORT_OUTPUT_HIGH)
    },
};

const ioport_cfg_t g_bsp_pin_cfg = {
    .number_of_pins = sizeof(g_bsp_pin_cfg_data) / sizeof(ioport_pin_cfg_t),
    .p_pin_cfg_data = &g_bsp_pin_cfg_data[0],
};

#if BSP_TZ_SECURE_BUILD

void R_BSP_PinCfgSecurityInit(void);

/* Initialize SAR registers for secure pins. */
void R_BSP_PinCfgSecurityInit(void)
{
 #if (2U == BSP_FEATURE_IOPORT_VERSION)
    uint32_t pmsar[BSP_FEATURE_BSP_NUM_PMSAR];
 #else
    uint16_t pmsar[BSP_FEATURE_BSP_NUM_PMSAR];
 #endif
    memset(pmsar, 0xFF, BSP_FEATURE_BSP_NUM_PMSAR * sizeof(R_PMISC->PMSAR[0]));

    for (uint32_t i = 0; i < g_bsp_pin_cfg.number_of_pins; i++)
    {
        uint32_t port_pin = g_bsp_pin_cfg.p_pin_cfg_data[i].pin;
        uint32_t port = port_pin >> 8U;
        uint32_t pin = port_pin & 0xFFU;
        pmsar[port] &= (uint16_t) ~(1U << pin);
    }

    for (uint32_t i = 0; i < BSP_FEATURE_BSP_NUM_PMSAR; i++)
    {
 #if (2U == BSP_FEATURE_IOPORT_VERSION)
    #if BSP_SECONDARY_CORE_BUILD
        R_PMISC->PMSAR[i].PMSAR &= (uint16_t) pmsar[i];
    #else
        R_PMISC->PMSAR[i].PMSAR = (uint16_t) pmsar[i];
    #endif
 #else
        R_PMISC->PMSAR[i].PMSAR = pmsar[i];
 #endif
    }
}
#endif
