# Bit-Banged UART TX – User Manual

**Short ID:** `rs_bb_uart_tx`
**Component ID:** `RS_BB_UART_TX`
**Version:** 0.1.0
**FSP Baseline:** FSP v6.5.0, LLVM Embedded Toolchain for Arm (ATfE)
**Reference Targets:** FPB-RA2E3, FPB-RA4E2, FPB-RA6E2
**Build Environment:** Visual Studio Code + CMake (Ninja)
**License:** BSD-3-Clause

---

## About This Manual

This manual covers the core use case, capabilities, and reusability model of the
`rs_bb_uart_tx` module as delivered in the **VS Code / CMake** example projects
(`BitBangedUART_TX_FPB_RA2E3`, `BitBangedUART_TX_FPB_RA4E2`,
`BitBangedUART_TX_FPB_RA6E2`). It is the starting point for evaluating and
adapting the module. Detailed, task-specific documentation has been split
into companion guides — see the [Documentation Map](#documentation-map) below.

> **e2studio users:** A parallel e2studio delivery of this module also
> exists at `examples/e2studio/BitBangedUART_TX_FPB_RA2E3/` (see the
> repository root `README.md`, which documents that project in detail —
> including its e2studio **Import** procedure, distinct from the **Open**
> procedure used for the VS Code projects documented here). This
> documentation set is written for the three VS Code / CMake projects
> under `examples/VSCode/` specifically. Where the two delivery mechanisms
> share behavior — the module core, the frame timing, the board-specific
> GPT/GPIO mapping — this set and the root `README.md` describe the same
> facts; where they differ, it is purely a build-system and project-opening
> difference (CMake + Ninja + VS Code vs. e2studio's Eclipse-based project
> model), not a functional one.

## Documentation Map

| Document | What it covers | Read this when you are... |
|---|---|---|
| **BIT_BANGED_UART_TX-USER_MANUAL.md** *(this file)* | Use case, core capabilities, design pattern, reusability model | Evaluating the module for the first time |
| [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md) | Opening/building the example projects in VS Code, expected Tera Term output per board | Running the reference demo for the first time |
| [BUILD.md](BUILD.md) | Command-line CMake/Ninja build, VS Code tool setup, flashing/debugging, troubleshooting | Building, testing, or flashing from the command line |
| [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) | Reference board assumptions, required FSP settings, adapting to a new board, dependencies | Porting to your own board or peripherals |
| [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md) | Every function — public **and** internal/private, each clearly tagged — naming conventions, calling-context rules | Writing code against, or debugging inside, `rs_bb_uart_tx` |
| [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) | Initialization order, calling-context rules, buffer-sizing considerations | Wiring the Bit-Banged UART into your application |
| [FILE_GUIDE.md](FILE_GUIDE.md) | Core vs. demo vs. FSP-generated files, naming conventions, what is safe to modify | Deciding what to copy, keep, or replace |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Lifecycle state machine, function state gating, TX bit-engine pipeline, event flow | Understanding internal design or debugging state/timing issues |
| [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md) | Demonstrated behavior, ISR timing, RAM budget, baud-rate accuracy | Validating the module meets your timing/memory budget |
| [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md) | Adapting the core logic for other outputs and use cases beyond the reference demo | Reusing the module for a different use case |
| [RELEASE_NOTES.md](RELEASE_NOTES.md) | What changes between releases, per-board differences, merge guidance | Adopting a newer release of this module |

---

## Table of Contents

1. [What Use Case Does This Module Demonstrate?](#1-what-use-case-does-this-module-demonstrate)
2. [What Does the Core Code Do?](#2-what-does-the-core-code-do)
   - [2.1 Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use)
3. [Core Code vs. Reference Application Code](#3-core-code-vs-reference-application-code)
4. [Where to Go Next](#4-where-to-go-next)

---

## 1. What Use Case Does This Module Demonstrate?

This reference code demonstrates **software-driven (bit-banged), transmit-only UART output using one GPIO pin and one GPT timer channel**. It addresses the recurring embedded development challenge of producing UART-style console or diagnostic output when a hardware SCI/UART channel is unavailable, already committed to another peripheral function, or intentionally reserved.

The core challenge the module solves is generating accurate, low-jitter UART bit timing entirely from software — using a general-purpose timer interrupt to toggle a GPIO pin at precisely the bit period for the configured baud rate — without consuming a dedicated hardware UART/SCI channel, and without blocking the calling application while a frame transmits.

### Representative Application Scenarios

| Scenario | Why bit-banged TX helps |
|---|---|
| All hardware SCI/UART channels already allocated to sensors, host protocols, or other peripherals | Recovers a diagnostic/console channel from any spare GPIO pin |
| Bring-up and bootloader diagnostics before the full peripheral configuration is loaded | No SCI stack dependency; only a GPIO pin and a GPT channel are needed |
| Low pin-count or low-cost devices where the SCI pins are needed for their primary function | Frees the SCI channel for its intended use while retaining a debug/log path |
| Reference/example projects that want a predictable, minimal-dependency console path | Self-contained; does not depend on FSP UART middleware at all |

The module encapsulates a GPT HAL driver (`r_gpt` / `timer_instance_t`) and drives the TX pin through direct BSP pin control (`R_BSP_PinCfg()` / `R_BSP_PinWrite()`), not a full `ioport_instance_t`. Developers do not need to manually derive bit-period timer counts, manage frame bit sequencing, or hand-roll a ring buffer to get non-blocking, byte-oriented UART-style output from a spare GPIO pin.

**Phase 1 scope** (this delivery): TX-only, 8N1 frame format, LSB-first, configurable baud rate from 1200 to 115200 baud, non-blocking writes. There is no receive path and no interactive CLI input — see [Known Limitations](#known-limitations-phase-1-by-design) below.

---

## 2. What Does the Core Code Do?

The core module provides the following capabilities:

**Bit-Period Timing via GPT**
`rs_bb_uart_tx_ConfigCheck()` opens the configured GPT timer instance, queries its *actual* applied clock frequency at runtime (`timer_info_t.clock_frequency`, via the generic `timer_api_t::infoGet()` call — not a hard-coded clock-tree assumption), and computes the bit-period count for the configured baud rate using round-to-nearest integer arithmetic: `period_counts = round(clock_frequency / baud_rate) - 1`. This makes the calculation itself driver- and board-agnostic; only the board's GPT clock-divider choice (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)) differs per target.

**Non-Blocking Enqueue via Ring Buffer**
`rs_bb_uart_tx_Write()`/`rs_bb_uart_tx_WriteString()` copy bytes into a static, single-producer/single-consumer (SPSC) ring buffer using an **all-or-nothing** policy: either the entire request fits, or nothing is written and `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` is returned. Both calls return immediately once the data is queued — they do not wait for transmission to complete. See [ARCHITECTURE.md](ARCHITECTURE.md#3-tx-bit-engine-pipeline-and-data-flow) for why the ring buffer needs no critical section.

**GPT-ISR-Driven Bit Engine**
A GPT compare/overflow interrupt fires once per bit period. On every tick, the interrupt callback writes the GPIO pin to the level already decided by the *previous* tick (an O(1) operation with no ring-buffer access), then prepares the *next* tick's output and advances the internal TX state machine — dequeuing the next byte only when needed. This pipelined design keeps every tick's GPIO write off the ring buffer's access path. See [ARCHITECTURE.md](ARCHITECTURE.md) for the full state machine and pipeline rationale.

**Fixed 8N1 Frame, LSB-First**
Every byte is framed as: 1 start bit (low) → 8 data bits (LSB first) → 1 stop bit (high), idling high between frames. Frame format is fixed in Phase 1 — no parity, no alternate stop-bit counts.

**Runtime Status and Diagnostics**
`rs_bb_uart_tx_StatusGet()` returns a snapshot of the current lifecycle state, configured baud rate, ring-buffer occupancy (queued/free bytes), and four cumulative counters: total bytes accepted, total bytes transmitted, buffer-full rejection count, and invalid-call rejection count.

**Two Configuration Paths, One Set of Bridge Functions**
`rs_bb_uart_tx_ConfigGen()` and `rs_bb_uart_tx_ConfigCheck()` are declared unconditionally in each board's configuration header, so the same two-call sequence works whether the caller binds the board's standalone `*_preset` GPT instance (default, `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 1`) or an FSP Stack Configurator-generated instance (`RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 0`). Both paths are fully wired in every board project in this delivery — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md).

---

## 2.1 Design Pattern: Single-Call, Simple-Use

Unlike modules that expose a full `Open()`/`Start()`/`Stop()`/`Close()` public lifecycle, `rs_bb_uart_tx` follows the Engineering Guidelines' **"single-call/simple-use"** pattern: `rs_bb_uart_tx_ConfigCheck()` performs double duty by design. On success, it *also* performs the internal Open()-equivalent step — binding the configuration, opening the GPT and GPIO HAL resources, resetting the ring buffer and TX bit-engine, and transitioning the internal lifecycle state to READY — so the public call sequence is exactly:

```c
rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0_preset, BSP_IO_PORT_01_PIN_09);
rs_bb_uart_tx_ConfigCheck(&g_cfg);            // validates AND opens on success
rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\r\n");
```

Internal `Open()`/`Start()`/`Stop()`/`Close()`-equivalent helpers exist only as file-local `static` functions (or, for the shared `open` helper, an internal-linkage function called from the board configuration file) inside the core source — they are **never** exposed as public functions. This is a deliberate, documented design choice for this module (see `rs_bb_uart_tx_types.h`'s top-of-file note): the module is a **singleton** — one GPIO pin, one GPT channel, one instance per application — so there is no caller-managed control block to declare or pass around, unlike a multi-instance-capable module. Every public function operates on the module's own internal static instance.

This also means there is no public runtime-reconfiguration path in Phase 1: baud rate, TX pin, and GPT channel are fixed once `ConfigCheck()` succeeds. See [Known Limitations](#known-limitations-phase-1-by-design).

---

## 3. Core Code vs. Reference Application Code

The following table identifies which files are reusable core code and which belong only to the reference/demo application layer, at a glance. The full file-by-file breakdown — including which files are safe to modify — is in [FILE_GUIDE.md](FILE_GUIDE.md).

| Layer | Location | Reuse Expectation |
|---|---|---|
| **Core** | `src/rs_bb_uart_tx/` (`rs_bb_uart_tx.c/.h`, `_types.h`, `_ctrl.h`, `_cfg.h`, `_ring_buffer.c/.h`, `_engine.c/.h`, `_hal.c/.h`) | Copy into your project; hardware-agnostic orchestration, TX bit-engine, and ring buffer; only `_hal.c/.h` touches FSP GPT/GPIO types directly |
| **Preset Config** | `src/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_<board>.c/.h` | Board GPT preset instance + the `ConfigGen()`/`ConfigCheck()` bridge function definitions. Reference only — do not edit; for a different board, add a new preset file or use the FSP Stack Configurator path (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)) |
| **Reference App** | `src/rs_bb_uart_tx_demo.c`, `src/hal_entry.c`, `src/hal_warmstart.c`, `src/fault_handler.c` | Adapt or replace entirely for your application |
| **FSP Generated / Vendored** | `ra/`, `ra_gen/` directories | GPT (`r_gpt`) and IOPORT (`r_ioport`) driver source is genuinely vendored in this delivery — no additional FSP Smart Configurator step is required to build. Do not edit manually; regenerate via the FSP Smart Configurator (or the CMake pre-configure RASC step — see [BUILD.md](BUILD.md)) if you change `configuration.xml` |

**Key separation:** the demo's startup banner, heartbeat cadence, and status-print formatting in `rs_bb_uart_tx_demo.c` are entirely reference-application code — not part of the reusable module. The module core (`src/rs_bb_uart_tx/`) has no knowledge of what an application writes to it.

---

## Known Limitations (Phase 1, by design)

- **TX only.** No receive, no interactive CLI input, not full-duplex.
- **Not ISR-safe for `Write()`/`WriteString()`.** These must be called from startup or main-loop/task context, never from an interrupt handler (Phase 1 restriction — see [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)).
- **No dynamic reconfiguration while transmitting.** Baud rate, TX pin, and GPT channel are fixed once `ConfigCheck()` succeeds; `ConfigCheck()` is not idempotent.
- **Buffer-full policy is "return error" only.** No overwrite-oldest, drop-newest, or blocking option in Phase 1.
- **Fixed 8N1 frame format.** No parity or alternate stop-bit counts in Phase 1.

---

## 4. Where to Go Next

- **Running the demo for the first time?** → [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md)
- **Building from the command line?** → [BUILD.md](BUILD.md)
- **Wiring the Bit-Banged UART into your application?** → [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md)
- **Reusing this for a different output or use case?** → [REUSABILITY_GUIDE.md](REUSABILITY_GUIDE.md)
- **Porting to your own board?** → [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md)
- **Deciding what to copy vs. replace?** → [FILE_GUIDE.md](FILE_GUIDE.md)
- **Writing code against the module?** → [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md)
- **Understanding internal design?** → [ARCHITECTURE.md](ARCHITECTURE.md)
- **Checking timing/RAM budget?** → [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md)
- **Upgrading from an older release?** → [RELEASE_NOTES.md](RELEASE_NOTES.md)

---

*End of User Manual — `rs_bb_uart_tx` v0.1.0*
