# Reusability and Adaptation for Similar Use Cases

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).
> This guide covers **adapting the core logic for a different target, pin, or output need**. For **wiring the module into your application as-is** (calling-context rules, buffer sizing), see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md).

## Table of Contents

1. [Core Function Reusability](#1-core-function-reusability)
2. [Portions Applicable Without Modification](#2-portions-applicable-without-modification)
3. [Portions Requiring Modification](#3-portions-requiring-modification)
4. [Example: Porting to a Fourth Board](#4-example-porting-to-a-fourth-board)
5. [Example: Redirecting `printf()` Through the Module](#5-example-redirecting-printf-through-the-module)
6. [What This Module Is Not Designed to Do](#6-what-this-module-is-not-designed-to-do)

---

## 1. Core Function Reusability

`rs_bb_uart_tx_Write()`/`WriteString()` are the universal entry points and are, by design, **agnostic to what the transmitted bytes represent**. The core module has no knowledge of whether the caller is sending a diagnostic banner, a binary protocol frame, or `printf()`-redirected text — it only knows how to move bytes onto a GPIO pin at the configured baud rate, non-blocking, all-or-nothing. No changes to the core source are required to change *what* you send.

The TX bit-engine (`rs_bb_uart_tx_engine.c`) and ring buffer (`rs_bb_uart_tx_ring_buffer.c`) are also fully hardware-agnostic — neither file includes any FSP header. Only `rs_bb_uart_tx_hal.c` (and the board-specific `rs_bb_uart_tx_data_fpb_<board>.c/.h` preset) touch FSP/GPT/GPIO types directly. This isolation is what makes porting to a new board, or a new timer peripheral, a contained change — see [ARCHITECTURE.md](ARCHITECTURE.md#4-module-file-responsibilities).

---

## 2. Portions Applicable Without Modification

- All of `src/rs_bb_uart_tx/rs_bb_uart_tx.c/.h`, `_types.h`, `_ctrl.h`, `_cfg.h`, `_ring_buffer.c/.h`, and `_engine.c/.h` can be included directly in any FSP v6.5.0 bare-metal project with an available GPT channel and a spare GPIO pin, on any RA MCU, without modification — provided the configuration is supplied via `rs_bb_uart_tx_ConfigGen()` with a valid `timer_instance_t` and pin.
- `rs_bb_uart_tx_hal.c/.h` is also reusable **unmodified** on any RA target whose GPT driver implements the standard `timer_api_t` interface (`open`/`infoGet`/`periodSet`/`reset`/`start`/`stop`/`close`) and whose pin-write path matches this module's `R_BSP_PinAccessEnable()`/`R_BSP_PinCfg()`/`R_BSP_PinWrite()`/`R_BSP_PinAccessDisable()` pattern — true for every RA family MCU using the standard FSP BSP.
- The ring buffer's lock-free SPSC enqueue/dequeue pattern (`rs_bb_uart_tx_ring_buffer.c`) represents a portable, heap-free, single-producer/single-consumer byte-queue pattern reusable in similar producer/ISR-consumer designs beyond UART TX — see [ARCHITECTURE.md](ARCHITECTURE.md#5-ring-buffer-concurrency-model) for the concurrency argument that makes it safe without a critical section.
- The two-phase ISR pipeline pattern (`output_get()` before `advance()`, see [ARCHITECTURE.md](ARCHITECTURE.md#3-tx-bit-engine-pipeline-and-data-flow)) is a reusable technique for any fixed-period ISR that needs to keep its time-critical output action off a variable-cost data-structure access path — applicable well beyond UART bit-banging.

---

## 3. Portions Requiring Modification

- **Board-specific preset config files** (`config/rs_bb_uart_tx_data_fpb_<board>.c/.h`): create a new preset file for other target boards, or switch to the FSP Stack Configurator path (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board)). This is where the GPT clock divider, TX pin, and interrupt priority for a given board live.
- **`rs_bb_uart_tx_hal.c`'s GPT clock divider choice** is *not* hardcoded in that file — it lives entirely in each board's `timer_cfg_t.source_div` in the preset config file, so `rs_bb_uart_tx_hal.c` itself needs no change when porting; only the preset config does.
- **Reference application** (`rs_bb_uart_tx_demo.c`, `hal_entry.c`, `hal_warmstart.c`, `fault_handler.c`): replace the banner, heartbeat cadence, and status-print logic with application-specific code. `fault_handler.c` specifically must be re-derived (not copy-pasted) if you port to a different Cortex-M core generation — see [FILE_GUIDE.md](FILE_GUIDE.md) for the Cortex-M23 vs. Cortex-M33 register-set difference already handled across the three delivered boards.
- **Frame format** (8N1, LSB-first): fixed in Phase 1 inside `rs_bb_uart_tx_engine.c`'s state machine and `rs_bb_uart_tx_cfg.h`'s `RS_BB_UART_TX_DATA_BITS`/`RS_BB_UART_TX_STOP_BITS` macros. Adding parity or a different stop-bit count requires extending the bit-engine state machine — not a configuration-only change.
- **Buffer-full policy** (return error only): fixed in Phase 1 (`RS_BB_UART_TX_CFG_OVERFLOW_POLICY_RETURN_ERROR` is not `#ifndef`-guarded — it cannot be overridden by a project define). Adding an overwrite-oldest or blocking policy requires modifying `rs_bb_uart_tx_ring_buffer.c` and the public `Write()`/`WriteString()` contract in `rs_bb_uart_tx.c`/`rs_bb_uart_tx.h`.

---

## 4. Example: Porting to a Fourth Board

```c
// New file: config/rs_bb_uart_tx_data_fpb_ra6m5.h  (illustrative — not delivered)

// 1. Determine PCLKD (or equivalent) and GPT channel width for the new MCU,
//    then choose a source_div that keeps period_counts in range at 1200 baud
//    — see CONFIGURATION_GUIDE.md Section 4 for the worked reasoning.
// 2. Determine the NVIC priority-bit count for the new core and pick a
//    valid cycle_end_ipl (0-3 on ARMv8-M Baseline cores; typically 0-15 on
//    ARMv8-M Mainline cores).
// 3. Confirm the chosen TX pin's default peripheral pairing (if any) and
//    disable that peripheral's Operation Mode entirely, not just its pin.
// 4. Copy rs_bb_uart_tx_data_fpb_ra4e2.c/.h as your starting template
//    (already PCLKD/4-divided, 16-bit-channel-safe) and adjust the values
//    above for the new board.
```

No changes to `rs_bb_uart_tx.c`, `rs_bb_uart_tx_engine.c`, `rs_bb_uart_tx_ring_buffer.c`, or `rs_bb_uart_tx_hal.c` are required — only a new preset config file (or an FSP Stack Configurator-generated GPT stack) and, if the new core's fault-exception register set differs, a re-derived `fault_handler.c`.

---

## 5. Example: Redirecting `printf()` Through the Module

The module itself does not provide a `printf()`/stdio redirection layer (unlike some UART reference modules that bridge to picolibc's stdio streams) — `Write()`/`WriteString()` are the only transmit entry points. If your application wants `printf()`-style output over the bit-banged TX pin, add a thin adapter in your own application code:

```c
// Application-owned adapter — not part of the reusable core.
int _write(int fd, char const * p_buf, int len)
{
    (void) fd;
    rs_bb_uart_tx_status_t status = rs_bb_uart_tx_Write((uint8_t const *) p_buf, (uint32_t) len);
    return (RS_BB_UART_TX_STATUS_SUCCESS == status) ? len : 0;
}
```

Remember the calling-context restriction still applies: `printf()` calls redirected this way must themselves only be made from main-loop/task context, never from an ISR — see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md#2-calling-context-rules).

---

## 6. What This Module Is Not Designed to Do

- **Not a receive path.** There is no RX pin, no RX ring buffer, and no interrupt-driven receive state machine anywhere in this module. Adding receive is a substantial architectural addition (a second GPIO pin sampled at the bit rate, typically via a separate timer or edge-triggered interrupt scheme), not a configuration change.
- **Not multi-instance.** See [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md#6-single-instance-constraint) — the singleton design is a deliberate deviation from a multi-instance-capable reference pattern, made because the target use case never needs more than one GPIO pin's worth of software UART output per application.
- **Not a drop-in replacement for a hardware SCI/UART driver's full feature set.** No parity, no configurable stop-bit count, no hardware flow control, no DMA/DTC offload — this module exists specifically for the case where no hardware UART channel is available at all.

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md). For wiring the module into your application as-is, see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md).*
