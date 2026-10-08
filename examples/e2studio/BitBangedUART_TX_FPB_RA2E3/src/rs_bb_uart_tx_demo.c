/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file rs_bb_uart_tx_demo.c
 * @brief Reference application for the Bit-Banged UART TX — FPB-RA2E3.
 *
 * Bare-metal example project (Architecture & Design Section 19). Demonstrates:
 *   - Generated configuration path (ConfigGen -> ConfigCheck).
 *   - A single public call to output a startup banner and status messages
 *     (rs_bb_uart_tx_WriteString()).
 *   - Periodic heartbeat output via a bare-metal main loop (no RTOS).
 *   - Reading back runtime status via rs_bb_uart_tx_StatusGet().
 *
 * Expected VCOM output (Architecture & Design Section 8.5), Tera Term at
 * 115200 8N1, on-board J-Link VCOM, no external adapter required:
 *
 *   ================================
 *   Renesas Bit-Banged UART Demo
 *   ================================
 *
 *   Board        : FPB-RA2E3
 *   Project      : VS Code bare-metal example
 *   UART Mode    : Software TX only
 *   Baud Rate    : 115200
 *   Frame Format : 8N1
 *   Transport    : On-board J-Link VCOM
 *
 *   System Ready
 *
 *   [000001] Heartbeat OK
 *   [000002] Status       : Running
 *   [000003] Heartbeat OK
 *   ...
 *
 * This file is demo/application-only code (Engineering Guidelines Section
 * 5): the demo strings, banner, and heartbeat cadence below are NOT part of
 * the reusable Bit-Banged UART core and are expected to be replaced by customer
 * application logic. The Bit-Banged UART core the customer actually reuses lives
 * entirely under src/rs_bb_uart_tx/.
 */

#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/rs_bb_uart_tx_cfg.h> /* RS_BB_UART_TX_CFG_PRESET_CONFIG_USE, default baud rate */

/* rs_bb_uart_tx_ConfigGen()/ConfigCheck() live in the board config header
 * (see rs_bb_uart_tx.h top-of-file note) and are declared there
 * UNCONDITIONALLY, so this header is included regardless of
 * RS_BB_UART_TX_CFG_PRESET_CONFIG_USE — only the *_preset instance externs
 * inside it are conditionally compiled on that flag. */
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.h>

#include "bsp_api.h"    /* R_BSP_SoftwareDelay() */
#include <stdio.h>      /* snprintf() */

/***********************************************************************************************************************
 * Demo configuration
 **********************************************************************************************************************/

/** Interval between heartbeat messages. */
#define DEMO_HEARTBEAT_PERIOD_MS        (1000U)

/** Print a StatusGet() snapshot every Nth heartbeat instead of "Heartbeat OK". */
#define DEMO_STATUS_EVERY_N_HEARTBEATS  (5U)

/** Bit-Banged UART TX configuration structure — must remain valid (static
 *  storage) for the life of the application; the singleton retains a
 *  pointer to it after rs_bb_uart_tx_ConfigCheck() succeeds (see that
 *  function's documentation in rs_bb_uart_tx.h). */
static rs_bb_uart_tx_cfg_t g_bb_uart_tx_cfg;

/***********************************************************************************************************************
 * Local helpers
 **********************************************************************************************************************/

/**
 * @brief Print the demo startup banner, including the configured baud rate.
 *
 * Demo/application-only code — not part of the reusable Bit-Banged UART
 * core (see the file-level note above). Called once from
 * rs_bb_uart_tx_demo_run() after rs_bb_uart_tx_ConfigCheck() succeeds.
 *
 * Built and sent as a SINGLE rs_bb_uart_tx_WriteString() call rather than
 * several separate back-to-back calls: the first WriteString() call in a
 * multi-call sequence starts the GPT timer, so every call after it
 * enqueues more data while that timer is already running, which is
 * unnecessary risk for no benefit here. A single combined call enqueues
 * everything up front, before the timer starts, when masking interrupts
 * during the copy is harmless.
 *
 * @return None.
 */
static void demo_print_banner(void)
{
    char banner[400];
    int  len = 0;

    len += snprintf(&banner[len], sizeof(banner) - (size_t) len,
                     "\r\n================================\r\n"
                     "Renesas Bit-Banged UART Demo\r\n"
                     "================================\r\n\r\n"
                     "Board        : FPB-RA2E3\r\n"
                     "Project      : bare-metal example\r\n"
                     "UART Mode    : Software TX only\r\n"
                     "Baud Rate    : %lu\r\n"
                     "Frame Format : 8N1\r\n"
                     "Transport    : On-board J-Link VCOM\r\n\r\n"
                     "System Ready\r\n\r\n",
                     (unsigned long) g_bb_uart_tx_cfg.baud_rate);

    if (len > 0)
    {
        (void) rs_bb_uart_tx_WriteString(banner);
    }
}

/**
 * @brief Print one "Heartbeat OK" status line with an incrementing counter.
 *
 * Demo/application-only code. Called from the main loop in
 * rs_bb_uart_tx_demo_run() on heartbeats that are not a StatusGet()
 * heartbeat (see DEMO_STATUS_EVERY_N_HEARTBEATS).
 *
 * @param[in] count  Current heartbeat counter value, printed zero-padded to 6 digits.
 *
 * @return None.
 */
static void demo_print_heartbeat(uint32_t count)
{
    char line[48];
    (void) snprintf(line, sizeof(line), "[%06lu] Heartbeat OK\r\n", (unsigned long) count);
    (void) rs_bb_uart_tx_WriteString(line);
}

/**
 * @brief Print one status line summarizing rs_bb_uart_tx_StatusGet() output.
 *
 * Demo/application-only code. Demonstrates the diagnostic use case for
 * rs_bb_uart_tx_StatusGet() — printed every DEMO_STATUS_EVERY_N_HEARTBEATS
 * heartbeats instead of the plain "Heartbeat OK" line. Silently does
 * nothing if StatusGet() fails (defensive; cannot actually fail once the
 * Bit-Banged UART is READY/TRANSMITTING, since @p state is always a valid caller-owned
 * pointer here).
 *
 * @param[in] count  Current heartbeat counter value, printed zero-padded to 6 digits.
 *
 * @return None.
 */
static void demo_print_status(uint32_t count)
{
    rs_bb_uart_tx_state_t state;
    char                  line[80];

    if (RS_BB_UART_TX_STATUS_SUCCESS != rs_bb_uart_tx_StatusGet(&state))
    {
        return;
    }

    (void) snprintf(line, sizeof(line),
                     "[%06lu] Status       : queued=%lu free=%lu tx=%lu ok=%lu full=%lu\r\n",
                     (unsigned long) count,
                     (unsigned long) state.queued_bytes,
                     (unsigned long) state.free_bytes,
                     (unsigned long) state.total_bytes_transmitted,
                     (unsigned long) state.total_bytes_accepted,
                     (unsigned long) state.buffer_full_count);
    (void) rs_bb_uart_tx_WriteString(line);
}

/***********************************************************************************************************************
 * Entry point — called once from hal_entry() (src/hal_entry.c)
 **********************************************************************************************************************/

/**
 * @brief Bit-Banged UART TX demo application entry point.
 *
 * Demonstrates the full Phase 1 usage sequence (Architecture & Design
 * Section 19): generated configuration (rs_bb_uart_tx_ConfigGen() ->
 * rs_bb_uart_tx_ConfigCheck()), a startup banner printed via a single
 * rs_bb_uart_tx_WriteString() call, and a bare-metal main loop that prints
 * a heartbeat once per second — printing a full rs_bb_uart_tx_StatusGet()
 * summary every DEMO_STATUS_EVERY_N_HEARTBEATS heartbeats instead of the
 * plain heartbeat line.
 *
 * If ConfigGen() or ConfigCheck() fails, halts in an infinite loop rather
 * than proceeding with an unconfigured/unopened Bit-Banged UART — see the inline
 * comments at each call site for the most common failure causes.
 *
 * @return Never returns (infinite loop).
 */
void rs_bb_uart_tx_demo_run(void)
{
    rs_bb_uart_tx_status_t status;
    uint32_t               heartbeat_count = 0U;

    /* --- Generated configuration path (Architecture & Design Section 19.2) --- */
#if RS_BB_UART_TX_CFG_PRESET_CONFIG_USE
    status = rs_bb_uart_tx_ConfigGen(&g_bb_uart_tx_cfg, &g_timer0_preset, BSP_IO_PORT_01_PIN_09);
#else
    status = rs_bb_uart_tx_ConfigGen(&g_bb_uart_tx_cfg, &g_timer0, BSP_IO_PORT_01_PIN_09);
#endif
    if (RS_BB_UART_TX_STATUS_SUCCESS != status)
    {
        /* ConfigGen only fails on a NULL pointer — a build-time programming
         * error, not a field condition. Halt for visibility during bring-up. */
        while (1)
        {
            /* ConfigGen() failed. */
        }
    }

    /* Application may override any ConfigGen() default here before
     * ConfigCheck(), e.g.: g_bb_uart_tx_cfg.baud_rate = 9600U; */

    status = rs_bb_uart_tx_ConfigCheck(&g_bb_uart_tx_cfg);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status)
    {
        /* Configuration rejected, or GPT/GPIO HAL open failed. Common causes:
         *   - RS_BB_UART_TX_STATUS_TIMER_ERROR: the r_gpt driver source has
         *     not been vendored yet (no GPT Timer stack added via the Smart
         *     Configurator) — see README.md "Before You Build".
         *   - RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE: baud_rate outside
         *     RS_BB_UART_TX_CFG_MIN/MAX_BAUD_RATE. */
        while (1)
        {
            /* ConfigCheck() failed. */
        }
    }

    /* --- Bit-Banged UART is now READY. Single public call demonstrates the primary
     *     use case: UART-style console output with no hardware SCI/UART
     *     channel required (Architecture & Design Section 1). --- */
    demo_print_banner();

    while (1)
    {
        R_BSP_SoftwareDelay(DEMO_HEARTBEAT_PERIOD_MS, BSP_DELAY_UNITS_MILLISECONDS);

        heartbeat_count++;

        if (0U == (heartbeat_count % DEMO_STATUS_EVERY_N_HEARTBEATS))
        {
            demo_print_status(heartbeat_count);
        }
        else
        {
            demo_print_heartbeat(heartbeat_count);
        }
    }
}
