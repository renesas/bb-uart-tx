/***********************************************************************************************************************
 * Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **********************************************************************************************************************/

/**
 * @file fault_handler.c
 * @brief Cortex-M23 fault exception handler with best-available diagnostic capture.
 *
 * Overrides the weak Default_Handler alias declared in the FSP startup
 * code for the Cortex-M23 fault exception(s). Bare-metal — no RTOS
 * dependency. Not part of the Bit-Banged UART TX itself; this is
 * general-purpose project debug infrastructure, provided so a fault during
 * Bit-Banged UART bring-up (e.g. a misconfigured GPT/GPIO pointer) is easy to diagnose
 * rather than silently falling into the FSP's default infinite-loop
 * handler with no register context captured.
 *
 * DIFFERENT FROM THE FPB-RA4E2/FPB-RA6E2 VERSION OF THIS FILE — deliberately,
 * not an oversight: FPB-RA2E3 uses a Cortex-M23 core (ARMv8-M **Baseline**
 * profile), not the Cortex-M33 (ARMv8-M **Mainline**) used on the other two
 * boards. Baseline cores implement a substantially simpler fault
 * architecture. Confirmed directly against this board's own vendored CMSIS
 * header (`ra/arm/CMSIS_6/CMSIS/Core/Include/core_cm23.h`) rather than
 * assumed: `SCB_Type` on this core has only `CPUID/ICSR/VTOR/AIRCR/SCR/
 * CCR/SHPR/SHCSR` — **no `CFSR`, `HFSR`, `MMFAR`, or `BFAR`** (all
 * Mainline-only registers). The FPB-RA4E2/FPB-RA6E2 version of this file
 * reads those four registers directly; doing the same here would not
 * compile (no such struct members exist).
 *
 * `MemManage_Handler`/`BusFault_Handler`/`UsageFault_Handler`/
 * `SecureFault_Handler` vector slots still exist in this project's shared
 * FSP startup code (`ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/startup.c`
 * declares and vectors all four as weak symbols, matching the Mainline
 * boards' startup file), so overriding them here is harmless and kept for
 * source-level consistency with the other two boards' deliveries — but on
 * real Cortex-M23 Baseline hardware, the core's simplified exception model
 * is expected to route essentially everything through `HardFault_Handler`
 * in practice, since it does not implement the fine-grained fault-type
 * decoding Mainline cores do.
 *
 * After a rebuild, confirm the linker resolved these symbols correctly by
 * checking the .map file for "HardFault_Handler" etc. and confirming they
 * resolve to fault_handler.o, not Default_Handler.
 */

#include "hal_data.h"

/** @name Fault diagnostic snapshot
 * Populated by fault_capture_and_halt() when any fault handler below
 * fires. Inspect these in the debugger (Memory view or Expressions) when
 * halted at the breakpoint in fault_capture_and_halt().
 *
 * Deliberately smaller than the FPB-RA4E2/FPB-RA6E2 version of this file:
 * `g_fault_shcsr` (System Handler Control and State Register) is the best
 * available substitute for the missing CFSR/HFSR/MMFAR/BFAR on this core —
 * it carries active/pending state for several system exceptions but not
 * the detailed sub-fault-cause breakdown Mainline's CFSR provides. The
 * process stack pointer capture is unaffected — `MRS`-based PSP read is a
 * core instruction available on every Cortex-M variant, not an
 * SCB-register read, so it is identical to the other boards' version.
 * @{
 */
volatile uint32_t   g_fault_shcsr;  /**< System Handler Control and State Register. */
volatile uint32_t * g_fault_psp;    /**< Process stack pointer at fault time. */
/** @} */

/**
 * @brief Capture available Cortex-M23 fault diagnostic state, then halt.
 *
 * Snapshots SHCSR and the process stack pointer into the globals above,
 * then executes a breakpoint instruction (`bkpt #0`). With a debugger
 * attached, execution halts there so the registers and call stack can be
 * inspected. Without a debugger attached, the `bkpt` is skipped over by
 * hardware and execution falls through to the infinite loop below, which
 * deliberately never returns — continuing after a fault with corrupted
 * CPU/peripheral state is unsafe.
 *
 * @return Never returns.
 */
static void fault_capture_and_halt(void)
{
    g_fault_shcsr = SCB->SHCSR;

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
 * @brief Cortex-M23 HardFault exception handler.
 *
 * Overrides the FSP startup code's weak default. On this Baseline-profile
 * core, this is expected to be the handler that actually fires in
 * practice for essentially all fault conditions — see this file's
 * top-of-file note. See fault_capture_and_halt() for behavior.
 *
 * @return Never returns.
 */
void HardFault_Handler(void)
{
    fault_capture_and_halt();
}

/**
 * @brief MemManage fault exception handler (vector slot present for
 *        source-level consistency with the FPB-RA4E2/FPB-RA6E2 deliveries;
 *        not expected to fire independently on this Baseline-profile core
 *        — see this file's top-of-file note).
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
 * @brief BusFault exception handler (vector slot present for source-level
 *        consistency with the FPB-RA4E2/FPB-RA6E2 deliveries; not expected
 *        to fire independently on this Baseline-profile core — see this
 *        file's top-of-file note).
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
 * @brief UsageFault exception handler (vector slot present for
 *        source-level consistency with the FPB-RA4E2/FPB-RA6E2 deliveries;
 *        not expected to fire independently on this Baseline-profile core
 *        — see this file's top-of-file note).
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
 * @brief SecureFault exception handler (TrustZone builds only; vector
 *        slot present for source-level consistency with the FPB-RA4E2/
 *        FPB-RA6E2 deliveries).
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
