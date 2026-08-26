/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file hal_entry.c
 * @brief Bare-metal FSP application entry point.
 *
 * Standard FSP bare-metal convention: main() (ra_gen/main.c) calls
 * hal_entry() directly, which never returns for this example. Kept
 * intentionally thin — all demo logic lives in rs_bb_uart_tx_demo.c
 * (Engineering Guidelines Section 5: demo/reference application code
 * should be clearly separated from the reusable PES core, and this file is
 * simply the FSP-mandated entry point shim).
 */

#include "bsp_api.h"

extern void rs_bb_uart_tx_demo_run(void);

/**
 * @brief FSP bare-metal user entry point.
 *
 * Called once from main() (ra_gen/main.c) after the C runtime and BSP
 * common initialization complete. Delegates immediately to
 * rs_bb_uart_tx_demo_run() (src/rs_bb_uart_tx_demo.c), which never
 * returns.
 *
 * @return Never returns.
 */
void hal_entry(void)
{
    rs_bb_uart_tx_demo_run();

    /* rs_bb_uart_tx_demo_run() runs forever; unreachable. */
}
