/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file fault_handler.c
 * @brief Cortex-M33 fault exception handlers with register-snapshot capture.
 *
 * Overrides the weak Default_Handler aliases declared in the FSP startup
 * code for the Cortex-M33 fault exceptions. Bare-metal — no RTOS
 * dependency. Not part of the Bit-Banged UART TX itself; this is
 * general-purpose project debug infrastructure, provided so a fault during
 * Bit-Banged UART bring-up (e.g. a misconfigured GPT/GPIO pointer) is easy to diagnose
 * rather than silently falling into the FSP's default infinite-loop
 * handler with no register context captured.
 *
 * After a rebuild, confirm the linker resolved these symbols correctly by
 * checking the .map file for "HardFault_Handler" / "UsageFault_Handler"
 * etc. and confirming they resolve to fault_handler.o, not Default_Handler.
 */

#include "hal_data.h"

/** @name Fault register snapshot
 * Populated by fault_capture_and_halt() when any fault handler below
 * fires. Inspect these in the debugger (Memory view or Expressions) when
 * halted at the breakpoint in fault_capture_and_halt().
 * @{
 */
volatile uint32_t   g_fault_cfsr;   /**< Configurable Fault Status Register. */
volatile uint32_t   g_fault_hfsr;   /**< HardFault Status Register. */
volatile uint32_t   g_fault_mmfar;  /**< MemManage Fault Address Register. */
volatile uint32_t   g_fault_bfar;   /**< BusFault Address Register. */
volatile uint32_t * g_fault_psp;    /**< Process stack pointer at fault time. */
/** @} */

/**
 * @brief Capture Cortex-M33 fault status registers, then halt.
 *
 * Snapshots CFSR/HFSR/MMFAR/BFAR and the process stack pointer into the
 * globals above, then executes a breakpoint instruction (`bkpt #0`). With a
 * debugger attached, execution halts there so the registers and call stack
 * can be inspected. Without a debugger attached, the `bkpt` is skipped over
 * by hardware and execution falls through to the infinite loop below,
 * which deliberately never returns — continuing after a fault with
 * corrupted CPU/peripheral state is unsafe.
 *
 * @return Never returns.
 */
static void fault_capture_and_halt(void)
{
    g_fault_cfsr  = SCB->CFSR;
    g_fault_hfsr  = SCB->HFSR;
    g_fault_mmfar = SCB->MMFAR;
    g_fault_bfar  = SCB->BFAR;

    __asm volatile ("MRS %0, psp" : "=r" (g_fault_psp));

    /* Halts here with a debugger attached. Without one, this falls through
     * to the infinite loop below instead of re-faulting. */
    __asm volatile ("bkpt #0");

    for (;;)
    {
        /* Stay here for inspection — do not let execution continue
         * past a fault with corrupted state. */
    }
}

/**
 * @brief Cortex-M33 HardFault exception handler.
 *
 * Overrides the FSP startup code's weak default. See
 * fault_capture_and_halt() for behavior.
 *
 * @return Never returns.
 */
void HardFault_Handler(void)
{
    fault_capture_and_halt();
}

/**
 * @brief Cortex-M33 MemManage (memory protection) fault exception handler.
 *
 * Overrides the FSP startup code's weak default. See
 * fault_capture_and_halt() for behavior.
 *
 * @return Never returns.
 */
void MemManage_Handler(void)
{
    fault_capture_and_halt();
}

/**
 * @brief Cortex-M33 BusFault exception handler.
 *
 * Overrides the FSP startup code's weak default. See
 * fault_capture_and_halt() for behavior.
 *
 * @return Never returns.
 */
void BusFault_Handler(void)
{
    fault_capture_and_halt();
}

/**
 * @brief Cortex-M33 UsageFault exception handler.
 *
 * Overrides the FSP startup code's weak default. See
 * fault_capture_and_halt() for behavior.
 *
 * @return Never returns.
 */
void UsageFault_Handler(void)
{
    fault_capture_and_halt();
}

/**
 * @brief Cortex-M33 SecureFault exception handler (TrustZone builds only).
 *
 * Overrides the FSP startup code's weak default. See
 * fault_capture_and_halt() for behavior.
 *
 * @return Never returns.
 */
void SecureFault_Handler(void)
{
    fault_capture_and_halt();
}
