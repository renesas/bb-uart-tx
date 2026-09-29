/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * @file r_ioport.h
 * @brief Host-build shim for FSP r_ioport.h.
 *
 * rs_bb_uart_tx_hal.c includes "r_ioport.h" unconditionally (for the
 * IOPORT_CFG_PORT_* pin-configuration flags).  On the host that include
 * resolves here.  Values are copied from the real FSP r_ioport.h.
 **********************************************************************************************************************/

#ifndef R_IOPORT_H
#define R_IOPORT_H

#include "rs_bb_uart_tx_test_types.h"

typedef enum e_ioport_cfg_options
{
    IOPORT_CFG_PORT_DIRECTION_OUTPUT = 0x00000004,
    IOPORT_CFG_PORT_OUTPUT_HIGH      = 0x00000001,
} ioport_cfg_options_t;

#endif /* R_IOPORT_H */
