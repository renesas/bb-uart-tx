# Bit-Banged UART TX  –  User Manual

**Short ID:** `rs_bb_uart_tx`
**Component ID:** `RS_BB_UART_TX`
**Version:** 1.0.0
**FSP Baseline:** FSP v6.5.0, LLVM Embedded Toolchain for Arm (ATfE)
**Reference Targets:** FPB-RA2E3, FPB-RA4E2, FPB-RA6E2
**Build Environment:** Visual Studio Code + CMake (Ninja)


---

## About This Manual

This manual covers the core use case, capabilities, and reusability model of the
`rs_bb_uart_tx` module as delivered in the **VS Code / CMake** example projects
(`BitBangedUART_TX_FPB_RA2E3`, `BitBangedUART_TX_FPB_RA4E2`,
`BitBangedUART_TX_FPB_RA6E2`). It is the starting point for evaluating and
adapting the module, and it also contains the complete task-specific documentation (running the demo,
configuration, function reference, integration, file layout, architecture,
performance, reusability, and release notes) in a single file. Use the
[Table of Contents](#table-of-contents) to jump to the section you need.

---

## Table of Contents

### Find What You Need by Goal

| If you are... | Go to |
|---|---|
| Evaluating the module for the first time | [1. What Use Case Does This Module Demonstrate?](#1-what-use-case-does-this-module-demonstrate), [2. What Does the Core Code Do?](#2-what-does-the-core-code-do) and [2.1 Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use) |
| Running the reference demo for the first time | [Running the Example Projects and Expected Output](#running-the-example-projects-and-expected-output) |
| Wiring the Bit-Banged UART into your application | [Integration Guide](#integration-guide) |
| Reusing the module for a different output or use case | [Reusability and Adaptation for Similar Use Cases](#reusability-and-adaptation-for-similar-use-cases) |
| Porting to your own board or peripherals | [Configuration Guide](#configuration-guide) |
| Deciding what to copy, keep, or replace | [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify) |
| Writing code against, or debugging inside, `rs_bb_uart_tx` | [Functions Guide](#functions-guide--public-and-internal-functions) |
| Understanding internal design or debugging state/timing issues | [Design Flow and Architecture](#design-flow-and-architecture) |
| Validating the module meets your timing/memory budget | [Demonstrated Behavior and Performance](#demonstrated-behavior-and-performance) |
| Checking Phase 1 restrictions | [Known Limitations (Phase 1, by design)](#known-limitations-phase-1-by-design) |
| Adopting a newer release of this module | [Release Notes and Adopting a Newer Release](#release-notes-and-adopting-a-newer-release) |

### Full Table of Contents

- [1. What Use Case Does This Module Demonstrate?](#1-what-use-case-does-this-module-demonstrate)
  - [Representative Application Scenarios](#representative-application-scenarios)
- [2. What Does the Core Code Do?](#2-what-does-the-core-code-do)
  - [2.1 Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use)
- [3. Core Code vs. Reference Application Code](#3-core-code-vs-reference-application-code)
- [Known Limitations (Phase 1, by design)](#known-limitations-phase-1-by-design)
- [4. Where to Go Next](#4-where-to-go-next)
- [Running the Example Projects and Expected Output](#running-the-example-projects-and-expected-output)
  - [1. Prerequisites](#1-prerequisites)
  - [2. Opening and Building in VS Code](#2-opening-and-building-in-vs-code)
  - [3. Flashing and Debugging](#3-flashing-and-debugging)
  - [4. Expected Output — FPB-RA2E3](#4-expected-output--fpb-ra2e3)
  - [5. Expected Output — FPB-RA4E2](#5-expected-output--fpb-ra4e2)
  - [6. Expected Output — FPB-RA6E2](#6-expected-output--fpb-ra6e2)
  - [7. Interaction Notes](#7-interaction-notes)
- [Configuration Guide](#configuration-guide)
  - [1. Configuration Overview](#1-configuration-overview)
  - [2. Configuration Assumptions Per Reference Board](#2-configuration-assumptions-per-reference-board)
  - [3. Required Non-Default FSP Settings](#3-required-non-default-fsp-settings)
  - [4. GPT Clock Divider — Why It Differs Per Board](#4-gpt-clock-divider--why-it-differs-per-board)
  - [5. Adapting to Your Board](#5-adapting-to-your-board)
  - [6. Using an External TTL-to-USB Adapter](#6-using-an-external-ttl-to-usb-adapter)
  - [7. Key Adaptation Points](#7-key-adaptation-points)
  - [8. Dependencies](#8-dependencies)
    - [Hardware Dependencies](#hardware-dependencies)
    - [Software Dependencies](#software-dependencies)
    - [Driver Dependency Details](#driver-dependency-details)
- [Functions Guide — Public and Internal Functions](#functions-guide--public-and-internal-functions)
  - [Public Functions](#public-functions)
    - [1. Start Here: The Minimal Call Sequence](#1-start-here-the-minimal-call-sequence)
    - [2. Configuration Bridge](#2-configuration-bridge)
      - [🟢🔒 `rs_bb_uart_tx_ConfigGen()` — PUBLIC BRIDGE](#-rs_bb_uart_tx_configgen--public-bridge)
      - [🟢🔒 `rs_bb_uart_tx_ConfigCheck()` — PUBLIC BRIDGE](#-rs_bb_uart_tx_configcheck--public-bridge)
      - [🟢 `rs_bb_uart_tx_ConfigGetSize()` — PUBLIC](#-rs_bb_uart_tx_configgetsize--public)
    - [3. Operational Functions](#3-operational-functions)
      - [🟢 `rs_bb_uart_tx_Write()` — PUBLIC](#-rs_bb_uart_tx_write--public)
      - [🟢 `rs_bb_uart_tx_WriteString()` — PUBLIC](#-rs_bb_uart_tx_writestring--public)
    - [4. Query Functions](#4-query-functions)
      - [🟢 `rs_bb_uart_tx_StatusGet()` — PUBLIC](#-rs_bb_uart_tx_statusget--public)
    - [5. Complete Public Function Reference Table](#5-complete-public-function-reference-table)
  - [Internal Functions](#internal-functions)
    - [6. Internal Functions — `rs_bb_uart_tx.c`](#6-internal-functions--rs_bb_uart_txc)
      - [🔒 `rs_bb_uart_tx_open_internal()` — INTERNAL](#-rs_bb_uart_tx_open_internal--internal)
      - [⚫ `rs_bb_uart_tx_start_internal()` — PRIVATE](#-rs_bb_uart_tx_start_internal--private)
    - [7. Internal Functions — `rs_bb_uart_tx_ring_buffer.c` / `.h`](#7-internal-functions--rs_bb_uart_tx_ring_bufferc--h)
    - [8. Internal Functions — `rs_bb_uart_tx_engine.c` / `.h`](#8-internal-functions--rs_bb_uart_tx_enginec--h)
    - [9. Internal Functions — `rs_bb_uart_tx_hal.c` / `.h`](#9-internal-functions--rs_bb_uart_tx_halc--h)
      - [🔒 GPT timer adapter — INTERNAL](#-gpt-timer-adapter--internal)
      - [🔒 GPIO adapter — INTERNAL](#-gpio-adapter--internal)
      - [🔒 GPT ISR-context callback — INTERNAL](#-gpt-isr-context-callback--internal)
    - [10. Complete Internal Function Reference Table](#10-complete-internal-function-reference-table)
- [Integration Guide](#integration-guide)
  - [1. Minimal Integration](#1-minimal-integration)
  - [2. Calling-Context Rules](#2-calling-context-rules)
  - [3. Sizing the Ring Buffer for Your Traffic Pattern](#3-sizing-the-ring-buffer-for-your-traffic-pattern)
  - [4. Handling `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`](#4-handling-rs_bb_uart_tx_status_tx_buffer_full)
  - [5. Bare-Metal Main-Loop Integration Pattern (Reference)](#5-bare-metal-main-loop-integration-pattern-reference)
  - [6. Single-Instance Constraint](#6-single-instance-constraint)
- [File Guide — Naming Conventions and What Is Safe to Modify](#file-guide--naming-conventions-and-what-is-safe-to-modify)
  - [1. Core Code vs. Reference Application Code](#1-core-code-vs-reference-application-code)
  - [2. Naming Conventions Used in This Module](#2-naming-conventions-used-in-this-module)
  - [3. Core Files — Copy and Keep; Modify with Care](#3-core-files--copy-and-keep-modify-with-care)
  - [4. Preset Configuration Files — Reference Only; Do Not Edit](#4-preset-configuration-files--reference-only-do-not-edit)
  - [5. Reference Application — Adapt Freely](#5-reference-application--adapt-freely)
  - [6. FSP-Generated / Vendored Files — Do Not Edit Manually](#6-fsp-generated--vendored-files--do-not-edit-manually)
  - [7. VS Code / CMake Project Files](#7-vs-code--cmake-project-files)
- [Design Flow and Architecture](#design-flow-and-architecture)
  - [1. Lifecycle State Machine](#1-lifecycle-state-machine)
  - [2. Public Function State Gating](#2-public-function-state-gating)
  - [3. TX Bit-Engine Pipeline and Data Flow](#3-tx-bit-engine-pipeline-and-data-flow)
    - [3.1 Why the Callback Is Split Into Two Halves](#31-why-the-callback-is-split-into-two-halves)
    - [3.2 Bit-Level State Machine](#32-bit-level-state-machine)
  - [4. Module File Responsibilities](#4-module-file-responsibilities)
  - [5. Ring Buffer Concurrency Model](#5-ring-buffer-concurrency-model)
- [Demonstrated Behavior and Performance](#demonstrated-behavior-and-performance)
  - [1. Functional Behavior Demonstrated](#1-functional-behavior-demonstrated)
  - [2. Bit-Period Timing Reference](#2-bit-period-timing-reference)
  - [3. Baud-Rate Accuracy and Jitter](#3-baud-rate-accuracy-and-jitter)
  - [4. ISR and Enqueue Performance](#4-isr-and-enqueue-performance)
  - [5. RAM Consumption](#5-ram-consumption)
  - [6. No Dynamic Allocation, No Floating Point](#6-no-dynamic-allocation-no-floating-point)
  - [7. On-Board VCOM Baud-Range Caveat](#7-on-board-vcom-baud-range-caveat)
- [Reusability and Adaptation for Similar Use Cases](#reusability-and-adaptation-for-similar-use-cases)
  - [1. Core Function Reusability](#1-core-function-reusability)
  - [2. Portions Applicable Without Modification](#2-portions-applicable-without-modification)
  - [3. Portions Requiring Modification](#3-portions-requiring-modification)
  - [4. Example: Porting to a Fourth Board](#4-example-porting-to-a-fourth-board)
  - [5. Example: Redirecting `printf()` Through the Module](#5-example-redirecting-printf-through-the-module)
  - [6. What This Module Is Not Designed to Do](#6-what-this-module-is-not-designed-to-do)
- [Release Notes and Adopting a Newer Release](#release-notes-and-adopting-a-newer-release)
  - [1. Current Version](#1-current-version)
  - [2. Delivery Scope](#2-delivery-scope)
  - [3. Per-Board Notes](#3-per-board-notes)
  - [4. Known Issues](#4-known-issues)
  - [5. Adopting a Newer Release](#5-adopting-a-newer-release)
  - [6. What Typically Changes Between Releases](#6-what-typically-changes-between-releases)
  - [7. Recommended Merge Process](#7-recommended-merge-process)

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
`rs_bb_uart_tx_ConfigCheck()` opens the configured GPT timer instance, queries its *actual* applied clock frequency at runtime (`timer_info_t.clock_frequency`, via the generic `timer_api_t::infoGet()` call — not a hard-coded clock-tree assumption), and computes the bit-period count for the configured baud rate using round-to-nearest integer arithmetic: `period_counts = round(clock_frequency / baud_rate) - 1`. This makes the calculation itself driver- and board-agnostic; only the board's GPT clock-divider choice (see [Configuration Guide](#configuration-guide)) differs per target.

**Non-Blocking Enqueue via Ring Buffer**
`rs_bb_uart_tx_Write()`/`rs_bb_uart_tx_WriteString()` copy bytes into a static, single-producer/single-consumer (SPSC) ring buffer using an **all-or-nothing** policy: either the entire request fits, or nothing is written and `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` is returned. Both calls return immediately once the data is queued — they do not wait for transmission to complete. See [Architecture](#3-tx-bit-engine-pipeline-and-data-flow) for why the ring buffer needs no critical section.

**GPT-ISR-Driven Bit Engine**
A GPT compare/overflow interrupt fires once per bit period. On every tick, the interrupt callback writes the GPIO pin to the level already decided by the *previous* tick (an O(1) operation with no ring-buffer access), then prepares the *next* tick's output and advances the internal TX state machine — dequeuing the next byte only when needed. This pipelined design keeps every tick's GPIO write off the ring buffer's access path. See [Architecture](#design-flow-and-architecture) for the full state machine and pipeline rationale.

**Fixed 8N1 Frame, LSB-First**
Every byte is framed as: 1 start bit (low) → 8 data bits (LSB first) → 1 stop bit (high), idling high between frames. Frame format is fixed in Phase 1 — no parity, no alternate stop-bit counts.

**Runtime Status and Diagnostics**
`rs_bb_uart_tx_StatusGet()` returns a snapshot of the current lifecycle state, configured baud rate, ring-buffer occupancy (queued/free bytes), and four cumulative counters: total bytes accepted, total bytes transmitted, buffer-full rejection count, and invalid-call rejection count.

**Two Configuration Paths, One Set of Bridge Functions**
`rs_bb_uart_tx_ConfigGen()` and `rs_bb_uart_tx_ConfigCheck()` are declared unconditionally in each board's configuration header, so the same two-call sequence works whether the caller binds the board's standalone `*_preset` GPT instance (default, `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 1`) or an FSP Stack Configurator-generated instance (`RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 0`). Both paths are fully wired in every board project in this delivery — see [Configuration Guide](#configuration-guide).

---

### 2.1 Design Pattern: Single-Call, Simple-Use

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

The following table identifies which files are reusable core code and which belong only to the reference/demo application layer, at a glance. The full file-by-file breakdown — including which files are safe to modify — is in [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify).

| Layer | Location | Reuse Expectation |
|---|---|---|
| **Core** | `src/rs_bb_uart_tx/` (`rs_bb_uart_tx.c/.h`, `_types.h`, `_ctrl.h`, `_cfg.h`, `_ring_buffer.c/.h`, `_engine.c/.h`, `_hal.c/.h`) | Copy into your project; hardware-agnostic orchestration, TX bit-engine, and ring buffer; only `_hal.c/.h` touches FSP GPT/GPIO types directly |
| **Preset Config** | `src/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_<board>.c/.h` | Board GPT preset instance + the `ConfigGen()`/`ConfigCheck()` bridge function definitions. Reference only — do not edit; for a different board, add a new preset file or use the FSP Stack Configurator path (see [Configuration Guide](#configuration-guide)) |
| **Reference App** | `src/rs_bb_uart_tx_demo.c`, `src/hal_entry.c`, `src/hal_warmstart.c`, `src/fault_handler.c` | Adapt or replace entirely for your application |
| **FSP Generated / Vendored** | `ra/`, `ra_gen/` directories | GPT (`r_gpt`) and IOPORT (`r_ioport`) driver source is genuinely vendored in this delivery — no additional FSP Smart Configurator step is required to build. Do not edit manually; regenerate via the FSP Smart Configurator, the CMake pre-configure RASC step, or the Meson build's `rasc_generate` run target if you change `configuration.xml` |

**Key separation:** the demo's startup banner, heartbeat cadence, and status-print formatting in `rs_bb_uart_tx_demo.c` are entirely reference-application code — not part of the reusable module. The module core (`src/rs_bb_uart_tx/`) has no knowledge of what an application writes to it.

---

## Known Limitations (Phase 1, by design)

- **TX only.** No receive, no interactive CLI input, not full-duplex.
- **Not ISR-safe for `Write()`/`WriteString()`.** These must be called from startup or main-loop/task context, never from an interrupt handler (Phase 1 restriction — see [Integration Guide](#integration-guide)).
- **No dynamic reconfiguration while transmitting.** Baud rate, TX pin, and GPT channel are fixed once `ConfigCheck()` succeeds; `ConfigCheck()` is not idempotent.
- **Buffer-full policy is "return error" only.** No overwrite-oldest, drop-newest, or blocking option in Phase 1.
- **Fixed 8N1 frame format.** No parity or alternate stop-bit counts in Phase 1.

---

## 4. Where to Go Next

- **Running the demo for the first time?** → [Running the Example Projects and Expected Output](#running-the-example-projects-and-expected-output)
- **Wiring the Bit-Banged UART into your application?** → [Integration Guide](#integration-guide)
- **Reusing this for a different output or use case?** → [Reusability and Adaptation for Similar Use Cases](#reusability-and-adaptation-for-similar-use-cases)
- **Porting to your own board?** → [Configuration Guide](#configuration-guide)
- **Deciding what to copy vs. replace?** → [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify)
- **Writing code against the module?** → [Functions Guide](#functions-guide--public-and-internal-functions)
- **Understanding internal design?** → [Design Flow and Architecture](#design-flow-and-architecture)
- **Checking timing/RAM budget?** → [Demonstrated Behavior and Performance](#demonstrated-behavior-and-performance)
- **Upgrading from an older release?** → [Release Notes and Adopting a Newer Release](#release-notes-and-adopting-a-newer-release)

---

## Running the Example Projects and Expected Output

*Read this when you are: Running the reference demo for the first time.*

### 1. Prerequisites

- One of: FPB-RA2E3, FPB-RA4E2, or FPB-RA6E2, connected via USB (provides both power and the on-board J-Link virtual COM port)
- Visual Studio Code with the CMake Tools extension and the Renesas RA extensions (device family support, `renesas-hardware` debug type)
- The LLVM Embedded Toolchain for Arm (ATfE), with `ARM_LLVM_TOOLCHAIN_PATH` (or `ARM_TOOLCHAIN_PATH`) set
- A serial terminal (Tera Term or equivalent) connected to the on-board J-Link virtual COM port at **115200 baud, 8N1, no flow control**

### 2. Opening and Building in VS Code

Each board's project lives under `examples/VSCode/BitBangedUART_TX_FPB_<board>/` (`RA2E3`, `RA4E2`, or `RA6E2`). **There is no e2studio-style "Import" step for VS Code projects** — you open the project directly by opening its workspace file, the same way you'd open any other VS Code project:

1. **Open:** double-click the board's `.code-workspace` file — `BitBangedUART_FPB_RA2E3.code-workspace`, `BitBangedUART_FPB_RA4E2.code-workspace`, or `BitBangedUART_FPB_RA6E2.code-workspace` — or in VS Code use **File → Open Workspace from File...** and select it. Opening the board's folder directly (**File → Open Folder...**) also works, since each board's project is fully self-contained. Each board is a separate project; there is no multi-board workspace in this delivery. VS Code's CMake Tools extension detects `CMakeLists.txt`/`CMakePresets.json` automatically on open — no import or project-recognition step is required.
2. Ensure `ARM_LLVM_TOOLCHAIN_PATH` is set in your environment before starting VS Code (or set `ARM_TOOLCHAIN_PATH` directly in `.vscode/cmake-kits.json`).
3. In the VS Code status bar, select the **"ARM LLVM - Ninja"** CMake kit (or **"ARM LLVM - MinGW Makefiles"** on Windows without Ninja).
4. Select a configure preset — **Debug** or **Release** (`CMakePresets.json`).
5. Build via the CMake Tools status-bar **Build** button, or run the **Build Project** task (**Terminal → Run Task → Build Project**). This also runs **Configure Project** first, which invokes RASC as a pre-build step if `configuration.xml` has changed (or if `RASC_EXE_PATH` resolves to something other than the `Config.cmake` default of `echo`).
6. The project is functional as delivered — `configuration.xml` and the checked-in `ra_gen`/`ra_cfg` content are pre-configured for the preset configuration path (`RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 1`, the default). No FSP Smart Configurator re-run is required for a first build.

> To edit the FSP configuration interactively instead, run the **"FSP Smart Configurator"** task (**Terminal → Run Task → FSP Smart Configurator**), which opens `configuration.xml` in the Smart Configurator GUI. See [Configuration Guide](#3-required-non-default-fsp-settings) for the settings that must be preserved per board.

### 3. Flashing and Debugging

Each board project includes a `.vscode/launch.json` configured for the Renesas hardware debug extension, targeting the on-board SEGGER J-Link:

| Board | `device` |
|---|---|
| FPB-RA2E3 | `R7FA2E307` |
| FPB-RA4E2 | `R7FA4E2B9` |
| FPB-RA6E2 | `R7FA6E2BB` |

Start debugging via **Run and Debug → Renesas GDB Debugging** (or **F5**). This flashes the built image and starts a debug session. Once running, open your serial terminal on the board's VCOM port at 115200 8N1.

### 4. Expected Output — FPB-RA2E3

```
================================
Renesas Bit-Banged UART Demo
================================

Board        : FPB-RA2E3
Project      : bare-metal example
UART Mode    : Software TX only
Baud Rate    : 115200
Frame Format : 8N1
Transport    : On-board J-Link VCOM

System Ready

[000001] Heartbeat OK
[000002] Heartbeat OK
[000003] Heartbeat OK
[000004] Heartbeat OK
[000005] Status       : queued=0 free=511 tx=304 ok=304 full=0
[000006] Heartbeat OK
...
```

### 5. Expected Output — FPB-RA4E2

```
================================
Renesas Bit-Banged UART Demo
================================

Board        : FPB-RA4E2
Project      : VS Code bare-metal example
UART Mode    : Software TX only
Baud Rate    : 115200
Frame Format : 8N1
Transport    : On-board J-Link VCOM

System Ready

[000001] Heartbeat OK
[000002] Heartbeat OK
[000003] Heartbeat OK
[000004] Heartbeat OK
[000005] Status       : queued=0 free=511 tx=304 ok=304 full=0
[000006] Heartbeat OK
...
```

### 6. Expected Output — FPB-RA6E2

Identical to FPB-RA4E2 above, with `Board        : FPB-RA6E2` in the banner.

### 7. Interaction Notes

- **Heartbeat cadence:** one line per second (`DEMO_HEARTBEAT_PERIOD_MS = 1000`), driven by `R_BSP_SoftwareDelay()` in a bare-metal main loop — no RTOS is used anywhere in this module or its reference demo.
- **Status line cadence:** every 5th heartbeat (`DEMO_STATUS_EVERY_N_HEARTBEATS = 5`) prints a `StatusGet()` snapshot instead of the plain "Heartbeat OK" line: `queued` (bytes waiting in the ring buffer), `free` (bytes of ring-buffer space remaining), `tx` (total bytes fully transmitted since `ConfigCheck()`), `ok` (total bytes accepted into the queue), `full` (count of rejected buffer-full enqueue attempts). In steady-state operation with no application backlog, `queued` reads 0 and `full` reads 0.
- **Exact `tx`/`ok` byte counts** in the examples above depend on the precise banner text and line lengths in your build and are shown as representative values, not a guaranteed exact count — read your own terminal output for the authoritative numbers.
- **Reconnecting the terminal:** if you open your terminal application after the board has already booted, you will only see output starting from that point — heartbeats already sent are not replayed. Power-cycle or reset the board after connecting your terminal to see the banner.

---

## Configuration Guide

*Read this when you are: Porting to your own board or peripherals.*

### 1. Configuration Overview

`rs_bb_uart_tx` relies on a combination of:

- A GPT HAL instance (`timer_instance_t`), either a standalone preset or FSP Stack Configurator-generated
- Build-time configuration constants in `rs_bb_uart_tx_cfg.h`
- A runtime configuration structure (`rs_bb_uart_tx_cfg_t`) bound via `rs_bb_uart_tx_ConfigGen()`

The reference implementation targets **FPB-RA2E3**, **FPB-RA4E2**, and **FPB-RA6E2** — all three via VS Code / CMake example projects in this delivery.

---

### 2. Configuration Assumptions Per Reference Board

These are reference assumptions for the demo hardware, not requirements for your application.

| Resource | FPB-RA2E3 | FPB-RA4E2 | FPB-RA6E2 |
|---|---|---|---|
| MCU | R7FA2E3073CFL (Cortex-M23, ARMv8-M Baseline) | R7FA4E2B9 (Cortex-M33, ARMv8-M Mainline) | R7FA6E2BB (Cortex-M33, ARMv8-M Mainline, TrustZone) |
| Package | 48-pin LQFP | 64-pin QFN | — |
| ROM / RAM (device) | 64 KB / 16 KB | — | — |
| TX GPIO pin | P109 (`BSP_IO_PORT_01_PIN_09`) | P411 (`BSP_IO_PORT_04_PIN_11`) | P411 (`BSP_IO_PORT_04_PIN_11`) |
| Default board VCOM RX pin (unused by this module) | P110 | P410 | P410 |
| Default competing peripheral on the TX pin | SCI9 TXD9 (paired with P110 as SCI9 RXD9, Simple I2C mode) | SCI0 TXD0 | SCI0 TXD0 |
| GPT channel | GPT0, periodic interrupt mode | GPT0, periodic interrupt mode | GPT0, periodic interrupt mode |
| GPT clock divider | `TIMER_SOURCE_DIV_1` (undivided) | `TIMER_SOURCE_DIV_4` | `TIMER_SOURCE_DIV_4` |
| GPT channel width | 32-bit (`BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0x01`) | 16-bit only | 16-bit only |
| PCLKD (effective, after divider) | 48 MHz (undivided) | ~25 MHz (100 MHz ÷ 4) | ~25 MHz (100 MHz ÷ 4) |
| GPT bit-period interrupt priority | 1 (hardcoded — see [Section 3](#3-required-non-default-fsp-settings)) | `RS_BB_UART_TX_CFG_GPT_IPL` (default 6) | `RS_BB_UART_TX_CFG_GPT_IPL` (default 6) |
| NVIC priority bits implemented | 2 (valid range 0–3) | 4 (valid range 0–15) | 4 (valid range 0–15) |
| Transport | On-board J-Link VCOM USB | On-board J-Link VCOM USB | On-board J-Link VCOM USB |
| Fault handler register set | `SHCSR` only (no `CFSR`/`HFSR`/`MMFAR`/`BFAR`) | Full: `CFSR`/`HFSR`/`MMFAR`/`BFAR` | Full: `CFSR`/`HFSR`/`MMFAR`/`BFAR` |

FPB-RA2E3's default VCOM transmit/receive pin assignment (P109/P110) reflects the physical J-Link OB VCOM routing on that specific board, defined by board-level connectivity — not by an SCI alternate-function naming convention the way P411/P410 (`TXD0`/`RXD0`) are on the other two boards.

---

### 3. Required Non-Default FSP Settings

These are the settings that must be preserved (or reproduced, if porting via the FSP Stack Configurator path) for the module to operate. If you ever reopen the FSP Smart Configurator perspective for one of these projects, verify these before clicking **Generate Project Content**.

| Board | Setting | Required Value | Reason |
|---|---|---|---|
| All | GPT0 counter-overflow interrupt | Enabled | If disabled, or the callback reverts to `NULL`, the bit engine will not run |
| All | GPT0 callback | `rs_bb_uart_tx_hal_gpt_callback` | The ISR the entire TX bit engine depends on |
| FPB-RA2E3 | GPT0 counter-overflow interrupt priority | **0–3 only** | Cortex-M23 (ARMv8-M Baseline) implements only 2 NVIC priority bits. The board config source hardcodes `cycle_end_ipl = 1U`, **not** `RS_BB_UART_TX_CFG_GPT_IPL` (default 6) — using the default here would be an invalid priority value on this board |
| FPB-RA4E2 / FPB-RA6E2 | GPT0 counter-overflow interrupt priority | 0–15, default `RS_BB_UART_TX_CFG_GPT_IPL` (6) | Cortex-M33 implements the full 4 NVIC priority bits |
| FPB-RA2E3 | P109 pin mode | GPIO Output (high) | Default board assignment is "Peripheral: SCI9 TXD9" — this module bit-bangs the frame in software and does not use the SCI9 hardware channel |
| FPB-RA2E3 | SCI9 Operation Mode | **Disabled entirely** (no active mode, no leftover pairing property) | SCI9's default Simple I2C mode claims P109 (TXD9) *and* P110 (RXD9) as a 2-pin pairing regardless of either pin's own individual assignment. The Smart Configurator's conflict checker will flag P109 as contested with this module's GPIO reassignment if SCI9's mode is re-enabled. P110 reverts to a plain unconfigured input as a result |
| FPB-RA4E2 / FPB-RA6E2 | P411 pin mode | GPIO Output (high) | Default board assignment is "Peripheral: SCI0 TXD0" |
| FPB-RA4E2 / FPB-RA6E2 | SCI0 pairing property | Inert/disabled pairing line preserved (`sci0.pairing.b`) | Same underlying SCI-pairing mechanism as FPB-RA2E3's SCI9 note above, different channel |

If any of these settings show as reverted, re-apply them and re-run **Generate Project Content** before rebuilding.

---

### 4. GPT Clock Divider — Why It Differs Per Board

FPB-RA2E3 uses an **undivided** PCLKD (`TIMER_SOURCE_DIV_1`); FPB-RA4E2 and FPB-RA6E2 both need a **PCLKD/4** divider (`TIMER_SOURCE_DIV_4`). This is a genuine per-board engineering decision, not a copy-paste inconsistency:

- **FPB-RA2E3's clock tree has no PLL** — HOCO (48 MHz) feeds PCLKD directly at ÷1, giving PCLKD = 48 MHz. At 48 MHz undivided, even the lowest supported baud rate (1200) needs only ~40,000 counts — comfortably within 16-bit range (65,535) with no divider needed at all.
- **FPB-RA2E3's GPT channel 0 is additionally a genuine 32-bit channel** (`BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0x01`), unlike FPB-RA4E2/FPB-RA6E2 (`0x00` — every GPT channel is 16-bit only), reinforcing that overflow is not a concern on FPB-RA2E3 even without a divider.
- **FPB-RA4E2/FPB-RA6E2's PCLKD runs at roughly 100 MHz undivided.** At 1200 baud, an undivided ~100 MHz clock would need ~83,333 counts — this overflows their 16-bit-only GPT counters. The PCLKD/4 divider brings the effective rate down to ~25 MHz, keeping period_counts within 16-bit range across the full 1200–115200 baud range.
- **Leaving FPB-RA2E3's clock undivided also gives better timing resolution** than the other two boards: ~417 counts at 115200 baud vs. ~217 counts on the PCLKD/4-divided boards.

FPB-RA6E2's clock tree (HOCO 20 MHz × PLL 10.0 = 200 MHz system clock, PCLKD ÷2 ≈ 100 MHz) produces effectively the same PCLKD as FPB-RA4E2, so the same ÷4 divider value applies unchanged on both boards.

---

### 5. Adapting to Your Board

Two configuration paths are available, selected by `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE` in `rs_bb_uart_tx_cfg.h` (default `1`):

- **Preset path (default):** bind the board's standalone `g_timer0_preset` instance (`src/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_<board>.c`) — has no dependency on `ra_gen` matching any particular Smart Configurator state.
- **FSP Stack Configurator path:** configure a GPT timer stack (`g_timer0`) via the FSP Smart Configurator and set `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 0`. Both paths call the same `rs_bb_uart_tx_ConfigGen()`/`ConfigCheck()` bridge functions — only the `timer_instance_t const *` argument changes.

To port to a board not already delivered here:

1. Determine the board's actual PCLKD (or equivalent GPT clock source) frequency and whether its GPT channels are 16-bit or 32-bit (`BSP_FEATURE_GPT_32BIT_CHANNEL_MASK` in that MCU's `bsp_feature.h`).
2. Choose a divider that keeps `period_counts` within the channel's counter width across the full 1200–115200 baud range — see [Section 4](#4-gpt-clock-divider--why-it-differs-per-board) for the worked reasoning.
3. Choose a valid GPT interrupt priority for that core — check the NVIC priority-bit count for the target Cortex-M core (2 bits = 0–3 on ARMv8-M Baseline cores like Cortex-M23; typically 4 bits = 0–15 on ARMv8-M Mainline cores like Cortex-M33).
4. Add a new `rs_bb_uart_tx_data_fpb_<board>.c/.h` pair following the pattern of the three delivered board files, or configure a GPT stack via the FSP Stack Configurator and use the Configurator path instead.
5. Confirm the chosen TX pin is not claimed by a default peripheral pairing (as SCI9/SCI0 claim P109/P411 by default on the reference boards) — disable that peripheral's Operation Mode entirely if it pairs with your chosen pin, not just its own pin assignment.

---

### 6. Using an External TTL-to-USB Adapter

To bypass the on-board VCOM bridge entirely (see [Behavior and Performance](#7-on-board-vcom-baud-range-caveat)), change the `tx_pin` argument passed to `rs_bb_uart_tx_ConfigGen()` in `rs_bb_uart_tx_demo.c` to your chosen GPIO, and wire an external TTL-level USB-to-UART adapter:

| Board signal | Adapter signal | Required | Notes |
|---|---|---|---|
| Configured GPIO TX | RXD | Yes | Board transmit output → adapter receive input |
| GND | GND | Yes | Common reference |
| — | TXD | No | Not used — Phase 1 is TX-only |
| — | VCC | Usually no | Only if the adapter needs a target reference voltage; confirm voltage compatibility first |

**Use a TTL-level adapter, not a true RS-232 level adapter** — never connect the target GPIO directly to an RS-232 DB9 interface.

---

### 7. Key Adaptation Points

| Item | Location | Effect |
|---|---|---|
| TX pin | `tx_pin` argument to `rs_bb_uart_tx_ConfigGen()` | Change to any available GPIO; confirm it isn't claimed by a default peripheral pairing first |
| GPT instance | `p_gpt_instance` argument to `rs_bb_uart_tx_ConfigGen()` | Preset (`g_timer0_preset`) or FSP Stack Configurator (`g_timer0`); switch via `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE` |
| Baud rate | `rs_bb_uart_tx_cfg_t.baud_rate`, set after `ConfigGen()` and before `ConfigCheck()` | Any value in `[RS_BB_UART_TX_CFG_MIN_BAUD_RATE, RS_BB_UART_TX_CFG_MAX_BAUD_RATE]` (default 1200–115200) |
| Default baud rate | `RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE` in `rs_bb_uart_tx_cfg.h` | Default 115200 |
| Ring buffer size | `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` in `rs_bb_uart_tx_cfg.h` | Must remain a power of two (64/128/256/512/1024 documented references); default 512 — see [Behavior and Performance](#5-ram-consumption) for why |
| GPT bit-period interrupt priority | `RS_BB_UART_TX_CFG_GPT_IPL` in `rs_bb_uart_tx_cfg.h` | Default 6; **not applicable on FPB-RA2E3**, whose board config file hardcodes 1 instead (Cortex-M23 priority range is 0–3) |
| Control-block RAM budget guard | `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` in `rs_bb_uart_tx_cfg.h` | Default 2048 bytes; build fails via `_Static_assert` if exceeded |
| Parameter checking | `RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE` in `rs_bb_uart_tx_cfg.h` | Mirrors `BSP_CFG_PARAM_CHECKING_ENABLE` when defined; defaults to enabled (1) otherwise |
| Configuration path | `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE` in `rs_bb_uart_tx_cfg.h` | `1` = preset (default), `0` = FSP Stack Configurator |
| Fault handler | `src/fault_handler.c` | Replace with your production fault handling; remember the Cortex-M23 vs. Cortex-M33 register-set difference if copying across boards — see [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify) |

---

### 8. Dependencies

#### Hardware Dependencies

| Resource | Usage | Owned By |
|---|---|---|
| GPT channel 0 (`r_gpt`) | Bit-period interrupt generation | This module |
| TX GPIO pin (P109 on FPB-RA2E3; P411 on FPB-RA4E2/FPB-RA6E2) | Software-driven UART TX output, direct BSP pin control | This module |
| On-board J-Link VCOM (USB) | Default demonstration transport (TTL-to-USB conversion) | Board hardware |

#### Software Dependencies

| Component | Version | Notes |
|---|---|---|
| Renesas FSP | v6.5.0 | Provides `r_gpt`, `r_ioport` types, BSP, startup |
| LLVM Embedded Toolchain for Arm (ATfE) | `clang`/`clang++` targeting `arm-none-eabi`, referenced by `cmake/llvm.cmake` |
| RASC (standalone) | v6.5.0-compatible | Required for headless project-content regeneration — from the CMake configure step or the Meson build's `rasc_generate` target |
| SEGGER J-Link (on-board) | — | Debug/flash on all three FPB boards |

#### Driver Dependency Details

This module references the GPT HAL driver exclusively through the FSP abstract interface type `timer_instance_t *`. This means the GPT-facing core source (`rs_bb_uart_tx_hal.c`) is written once and used unchanged on all three boards — board-specific differences are confined entirely to each board's `rs_bb_uart_tx_data_fpb_<board>.c/.h` preset file (or `configuration.xml`, on the FSP Stack Configurator path).

This module has **no dependency on any other `rs_` module.** It does not open, own, or depend on a UART/SCI HAL instance at all — the whole point of the module is to synthesize UART-style output without one.

---

## Functions Guide — Public and Internal Functions

*Read this when you are: Writing code against, or debugging inside, `rs_bb_uart_tx`.*

> For call sequences and how these functions fit together in an application, see [Integration Guide](#integration-guide).

This guide documents every function in `rs_bb_uart_tx` — both the **public** functions an application calls directly, and the **internal/private** functions used only inside the module. Each function is explicitly tagged:

| Tag | Meaning |
|---|---|
| 🟢 **PUBLIC** | Declared in `rs_bb_uart_tx.h`. Part of the supported application-facing interface. Naming: `rs_bb_uart_tx_OperationName()`. |
| 🔒 **INTERNAL** | Non-`static` linkage but declared in an internal header (`rs_bb_uart_tx_ctrl.h`, `rs_bb_uart_tx_hal.h`, `rs_bb_uart_tx_engine.h`, `rs_bb_uart_tx_ring_buffer.h`) explicitly marked "not part of the public API." Callable from other files inside the module, but not intended for application use. |
| ⚫ **PRIVATE** | `static` — file-scoped, callable only from within the `.c` file that defines it. |
| 🟢🔒 **PUBLIC BRIDGE** | Declared in `rs_bb_uart_tx.h` (so it is application-facing) but **defined** in the board-specific configuration file, alongside the board's preset GPT instance data. |

Only 🟢 **PUBLIC** and 🟢🔒 **PUBLIC BRIDGE** functions are safe to call from application code. 🔒 **INTERNAL** and ⚫ **PRIVATE** functions are documented here for engineers reading, debugging, or extending the module's source — calling them directly from application code voids the state-gating and calling-context guarantees described for the public functions.

---

### Public Functions

#### 1. Start Here: The Minimal Call Sequence

```c
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>   // pick your board

rs_bb_uart_tx_cfg_t    g_cfg;
rs_bb_uart_tx_status_t status;

status = rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0_preset, BSP_IO_PORT_04_PIN_11);
status = rs_bb_uart_tx_ConfigCheck(&g_cfg);   // validates AND opens on success
if (RS_BB_UART_TX_STATUS_SUCCESS == status)
{
    (void) rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\r\n");
}
```

There is no separate `Open()`/`Start()` call — see [Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use) for why `ConfigCheck()` performs double duty.

---

#### 2. Configuration Bridge

`rs_bb_uart_tx_ConfigGen()` and `rs_bb_uart_tx_ConfigCheck()` are declared in `rs_bb_uart_tx.h` but **defined** in the board-specific pre-set configuration source file, alongside the `*_preset` GPT instance data they are most commonly paired with:

```c
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.h>   // FPB-RA2E3
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>   // FPB-RA4E2
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra6e2.h>   // FPB-RA6E2
```

They are defined **unconditionally**, regardless of `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE`, so the same functions remain available whether the caller binds the board's `*_preset` instance or an FSP Stack Configurator-generated instance (e.g. `g_timer0`).

##### 🟢🔒 `rs_bb_uart_tx_ConfigGen()` — PUBLIC BRIDGE

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGen(rs_bb_uart_tx_cfg_t     * p_cfg,
                                                timer_instance_t const * p_gpt_instance,
                                                bsp_io_port_pin_t         tx_pin);
```

Assigns the GPT HAL instance and TX pin to a configuration structure, and fills in Phase 1 defaults for every other field: `RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE` (115200), `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` (512), 8N1, and `RS_BB_UART_TX_CFG_GPT_IPL`. Override any of these on `*p_cfg` after this call returns, before calling `ConfigCheck()`.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Configuration populated |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_cfg` or `p_gpt_instance` is `NULL` |

*Callable from any context before `ConfigCheck()`. Not ISR-safe.*

##### 🟢🔒 `rs_bb_uart_tx_ConfigCheck()` — PUBLIC BRIDGE

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigCheck(rs_bb_uart_tx_cfg_t const * p_cfg);
```

Validates that the baud rate is within the supported range, the GPT instance pointer is present, and the buffer-size field matches the compiled buffer size. On success, **also** commits the configuration and opens the GPT and GPIO HAL resources (see [Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use)).

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Configuration valid; module is now READY |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_cfg` or `p_cfg->p_gpt_instance` is `NULL` |
| `RS_BB_UART_TX_STATUS_UNSUPPORTED_BAUD_RATE` | `baud_rate` outside `RS_BB_UART_TX_CFG_MIN/MAX_BAUD_RATE` |
| `RS_BB_UART_TX_STATUS_INVALID_BUFFER_SIZE` | `p_cfg->buffer_size` does not match the compiled `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Module is already READY/TRANSMITTING/ERROR (not idempotent) |
| `RS_BB_UART_TX_STATUS_TIMER_ERROR` | GPT HAL open/period-set failed |
| `RS_BB_UART_TX_STATUS_GPIO_ERROR` | GPIO pin configuration failed |

*Valid once, before the first `Write()`/`WriteString()` call. Not ISR-safe.*

##### 🟢 `rs_bb_uart_tx_ConfigGetSize()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_ConfigGetSize(size_t * p_size);
```

Returns the size, in bytes, of the internal control block. Useful for confirming control-block sizing (e.g. against `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES`) without requiring `ConfigCheck()` to have run first. Since the module is a singleton, no control-block pointer argument is needed.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Size returned in `*p_size` |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_size` is `NULL` |

*Valid in any lifecycle state, including before `ConfigGen()`. Not ISR-safe.*

---

#### 3. Operational Functions

##### 🟢 `rs_bb_uart_tx_Write()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_Write(uint8_t const * p_data, uint32_t length);
```

Enqueues raw bytes for transmission. Non-blocking; returns immediately. Copies bytes into the ring buffer using an all-or-nothing policy — if the full request does not fit, nothing is written and `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` is returned. Starts (or continues) asynchronous transmission via the GPT timer if data is queued. `length == 0` is a no-op success regardless of `p_data`.

**Success means the data was accepted into the transmit queue — not that it has finished transmitting on the TX pin.**

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | All bytes accepted into the queue (or `length` was 0) |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_data` is `NULL` and `length > 0` |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Module is not READY/TRANSMITTING |
| `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` | Insufficient free space for the whole request |
| `RS_BB_UART_TX_STATUS_TIMER_ERROR` | Propagated if the GPT timer failed to start |

*Valid from startup context (after `ConfigCheck()` succeeds) and bare-metal main-loop/task context. **NOT** valid from ISR context in Phase 1.*

##### 🟢 `rs_bb_uart_tx_WriteString()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_WriteString(char const * p_str);
```

Enqueues a `NUL`-terminated ASCII string. Calculates the length via `strlen()`, then delegates to `rs_bb_uart_tx_Write()` — same all-or-nothing policy, same calling-context restriction. The terminating `NUL` is never transmitted.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | All characters accepted into the queue |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_str` is `NULL` |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Propagated from `Write()` |
| `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL` | Propagated from `Write()` |

*Same calling-context restriction as `rs_bb_uart_tx_Write()`.*

---

#### 4. Query Functions

##### 🟢 `rs_bb_uart_tx_StatusGet()` — PUBLIC

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_StatusGet(rs_bb_uart_tx_state_t * p_state);
```

Populates a caller-allocated snapshot of runtime status: lifecycle state, configured baud rate, ring-buffer occupancy (`queued_bytes`/`free_bytes`), and four cumulative diagnostic counters (`total_bytes_accepted`, `total_bytes_transmitted`, `buffer_full_count`, `invalid_call_count`).

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Status populated |
| `RS_BB_UART_TX_STATUS_INVALID_POINTER` | `p_state` is `NULL` |

*Valid in any lifecycle state, including before `ConfigCheck()` (fields read as zero/UNCONFIGURED). Callable from main-loop/task context; not guaranteed ISR-safe for control-flow decisions — it reads multiple fields without a critical section, which is acceptable for a diagnostics snapshot but not for a decision that must be atomic with respect to the GPT ISR.*

---

#### 5. Complete Public Function Reference Table

| Function | Category | Blocking? | Possible Return Values | Primary User |
|---|---|---|---|---|
| `rs_bb_uart_tx_ConfigGen()` | Configuration | No | `SUCCESS`, `INVALID_POINTER` | Application/example setup code |
| `rs_bb_uart_tx_ConfigCheck()` | Configuration | No | `SUCCESS`, `INVALID_POINTER`, `UNSUPPORTED_BAUD_RATE`, `INVALID_BUFFER_SIZE`, `INVALID_STATE`, `TIMER_ERROR`, `GPIO_ERROR` | Application/example setup code |
| `rs_bb_uart_tx_ConfigGetSize()` | Configuration | No | `SUCCESS`, `INVALID_POINTER` | Tooling, diagnostics |
| `rs_bb_uart_tx_Write()` | Operation | No | `SUCCESS`, `INVALID_POINTER`, `INVALID_STATE`, `TX_BUFFER_FULL`, `TIMER_ERROR` | Application |
| `rs_bb_uart_tx_WriteString()` | Operation | No | `SUCCESS`, `INVALID_POINTER`, `INVALID_STATE`, `TX_BUFFER_FULL` | Application |
| `rs_bb_uart_tx_StatusGet()` | Query | No | `SUCCESS`, `INVALID_POINTER` | Application, diagnostics |

The names above omit the common `RS_BB_UART_TX_STATUS_` prefix for readability.

---

### Internal Functions

#### 6. Internal Functions — `rs_bb_uart_tx.c`

##### 🔒 `rs_bb_uart_tx_open_internal()` — INTERNAL

```c
rs_bb_uart_tx_status_t rs_bb_uart_tx_open_internal(rs_bb_uart_tx_cfg_t const * p_cfg);
```

Declared in `rs_bb_uart_tx_ctrl.h`, defined in `rs_bb_uart_tx.c`. Binds `p_cfg` into the singleton control block, resets the ring buffer and TX bit-engine, opens the GPIO TX pin and GPT timer HAL resources, and transitions the lifecycle state UNCONFIGURED/CONFIGURED → READY. Called exclusively from the board configuration file's `rs_bb_uart_tx_ConfigCheck()` implementation on successful validation.

| Return Value | Meaning |
|---|---|
| `RS_BB_UART_TX_STATUS_SUCCESS` | Opened; state → READY |
| `RS_BB_UART_TX_STATUS_INVALID_STATE` | Already READY/TRANSMITTING/ERROR |
| `RS_BB_UART_TX_STATUS_GPIO_ERROR` | GPIO pin configuration failed; state → ERROR |
| `RS_BB_UART_TX_STATUS_TIMER_ERROR` | GPT HAL open/period-set failed; state → ERROR |

##### ⚫ `rs_bb_uart_tx_start_internal()` — PRIVATE

```c
static rs_bb_uart_tx_status_t rs_bb_uart_tx_start_internal(rs_bb_uart_tx_ctrl_t * p_ctrl);
```

Starts (or continues) asynchronous transmission. Called from `Write()`/`WriteString()` after a successful enqueue. A no-op returning `SUCCESS` if already TRANSMITTING (the GPT ISR will naturally pick up newly-queued bytes); otherwise starts the GPT timer and transitions to TRANSMITTING.

---

#### 7. Internal Functions — `rs_bb_uart_tx_ring_buffer.c` / `.h`

`rs_bb_uart_tx_ring_buffer.h` is explicitly marked *"Internal header — not part of the public API. Not included by `rs_bb_uart_tx.h`."* All five functions are 🔒 **INTERNAL**.

| Function | Purpose | Calling Context |
|---|---|---|
| `rs_bb_uart_tx_ring_buffer_reset(p_rb)` | Resets a ring buffer to the empty state. Called once from `rs_bb_uart_tx_open_internal()` | Before the GPT ISR is enabled only — not ISR-safe by itself |
| `rs_bb_uart_tx_ring_buffer_enqueue(p_rb, p_data, length)` | All-or-nothing enqueue; lock-free (no critical section — see [Architecture](#5-ring-buffer-concurrency-model)) | Main-loop/task context only — matches `Write()`'s restriction |
| `rs_bb_uart_tx_ring_buffer_dequeue(p_rb, p_byte)` | Dequeue a single byte; lock-free, O(1), no division | GPT ISR context only, via the TX engine |
| `rs_bb_uart_tx_ring_buffer_count_get(p_rb)` | Bytes currently queued (single volatile read) | Any context (diagnostics snapshot) |
| `rs_bb_uart_tx_ring_buffer_free_get(p_rb)` | Bytes of free space remaining | Any context (diagnostics snapshot) |

---

#### 8. Internal Functions — `rs_bb_uart_tx_engine.c` / `.h`

`rs_bb_uart_tx_engine.h` is explicitly marked *"Internal header — not part of the public API."* All three functions are 🔒 **INTERNAL**. This file never includes FSP headers — it calls back into the HAL shim to actually drive the GPIO pin, so it is unit-testable on the host.

| Function | Purpose | Calling Context |
|---|---|---|
| `rs_bb_uart_tx_engine_reset(p_engine)` | Resets the TX bit-engine to `TX_IDLE`, idle-high output pending. Called once from `rs_bb_uart_tx_open_internal()` | Before the GPT ISR is enabled |
| `rs_bb_uart_tx_engine_output_get(p_engine)` | Returns the pin level for the tick about to run. O(1), no ring-buffer access — call FIRST in the ISR | GPT ISR context only, while TRANSMITTING |
| `rs_bb_uart_tx_engine_advance(p_engine, p_rb)` | Prepares the *next* tick's output and advances the FSM; dequeues from the ring buffer when a new byte is needed. Call AFTER the GPIO write | GPT ISR context only, while TRANSMITTING |

See [Architecture](#3-tx-bit-engine-pipeline-and-data-flow) for the full pipeline design and the bit-level state machine these two functions implement.

---

#### 9. Internal Functions — `rs_bb_uart_tx_hal.c` / `.h`

`rs_bb_uart_tx_hal.h` is explicitly marked *"Internal header — not part of the public API."* This is the **only** file that includes GPT-specific FSP headers and drives the TX pin via direct register/BSP pin control. Seven 🔒 **INTERNAL** functions plus the ISR-context callback.

##### 🔒 GPT timer adapter — INTERNAL

| Function | Purpose |
|---|---|
| `rs_bb_uart_tx_hal_gpt_open(p_ctrl)` | Opens the GPT instance, queries its actual applied clock frequency via `infoGet()`, computes and applies the bit-period count for the configured baud rate via `periodSet()` (round-to-nearest — see [Behavior and Performance](#demonstrated-behavior-and-performance)). Does **not** start the timer. |
| `rs_bb_uart_tx_hal_gpt_start(p_ctrl)` | Resets the counter to 0, then starts the timer — every frame's first bit period is a full, consistent duration regardless of when `Start()` is called relative to any prior count state. |
| `rs_bb_uart_tx_hal_gpt_stop(p_ctrl)` | Stops the timer. Called from GPT ISR context once the TX engine returns to `TX_IDLE`. ISR-safe — a simple register write, not blocking. |
| `rs_bb_uart_tx_hal_gpt_close(p_ctrl)` | Closes the GPT instance. Not currently called by any Phase 1 code path (no public `Close()`-equivalent exists); provided for completeness. |

##### 🔒 GPIO adapter — INTERNAL

| Function | Purpose |
|---|---|
| `rs_bb_uart_tx_hal_gpio_open(p_ctrl)` | Configures the TX pin as a push-pull output, idling high, via `R_BSP_PinAccessEnable()`/`R_BSP_PinCfg()`/`R_BSP_PinAccessDisable()` — not a full `ioport_instance_t`, since a single statically-configured pin doesn't need that generality. |
| `rs_bb_uart_tx_hal_gpio_write(p_ctrl, level_high)` | Drives the TX pin. Called once per GPT ISR tick. **Every call is bracketed by `R_BSP_PinAccessEnable()`/`PinAccessDisable()`** — on this FSP/MCU combination, `R_BSP_PinWrite()` writes to the same write-protected PmnPFS register `R_BSP_PinCfg()` uses, so the write would be silently dropped otherwise. These enable/disable calls are a lightweight, reference-counted pair explicitly documented as ISR-safe. |
| `rs_bb_uart_tx_hal_gpio_close(p_ctrl)` | Returns the TX pin to a safe idle-high state (best-effort; does not revert pin muxing — that is an application-level decision). Not currently called by any Phase 1 code path; provided for completeness. |

##### 🔒 GPT ISR-context callback — INTERNAL

| Function | Purpose |
|---|---|
| `rs_bb_uart_tx_hal_gpt_callback(p_args)` | Registered as the GPT instance's `p_callback`. `p_args->p_context` is the owning `rs_bb_uart_tx_ctrl_t *`. Implements the two-half tick sequence described in [Architecture](#3-tx-bit-engine-pipeline-and-data-flow). Defensively returns immediately without touching engine/ring-buffer state if the lifecycle state is not TRANSMITTING (guards against a spurious/late interrupt). |

---

#### 10. Complete Internal Function Reference Table

| Function | Tag | File | Calling Context |
|---|---|---|---|
| `rs_bb_uart_tx_open_internal()` | 🔒 INTERNAL | `rs_bb_uart_tx.c` | Board config file's `ConfigCheck()` only |
| `rs_bb_uart_tx_start_internal()` | ⚫ PRIVATE | `rs_bb_uart_tx.c` | `Write()`/`WriteString()` only |
| `rs_bb_uart_tx_ring_buffer_reset()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Before GPT ISR enabled |
| `rs_bb_uart_tx_ring_buffer_enqueue()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Main-loop/task context |
| `rs_bb_uart_tx_ring_buffer_dequeue()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | GPT ISR context |
| `rs_bb_uart_tx_ring_buffer_count_get()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Any |
| `rs_bb_uart_tx_ring_buffer_free_get()` | 🔒 INTERNAL | `rs_bb_uart_tx_ring_buffer.c` | Any |
| `rs_bb_uart_tx_engine_reset()` | 🔒 INTERNAL | `rs_bb_uart_tx_engine.c` | Before GPT ISR enabled |
| `rs_bb_uart_tx_engine_output_get()` | 🔒 INTERNAL | `rs_bb_uart_tx_engine.c` | GPT ISR context, TRANSMITTING only |
| `rs_bb_uart_tx_engine_advance()` | 🔒 INTERNAL | `rs_bb_uart_tx_engine.c` | GPT ISR context, TRANSMITTING only |
| `rs_bb_uart_tx_hal_gpt_open()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Called by `rs_bb_uart_tx_open_internal()` |
| `rs_bb_uart_tx_hal_gpt_start()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Called by `rs_bb_uart_tx_start_internal()` |
| `rs_bb_uart_tx_hal_gpt_stop()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | GPT ISR context |
| `rs_bb_uart_tx_hal_gpt_close()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Not called in Phase 1 |
| `rs_bb_uart_tx_hal_gpio_open()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Called by `rs_bb_uart_tx_open_internal()` |
| `rs_bb_uart_tx_hal_gpio_write()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | GPT ISR context |
| `rs_bb_uart_tx_hal_gpio_close()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | Not called in Phase 1 |
| `rs_bb_uart_tx_hal_gpt_callback()` | 🔒 INTERNAL | `rs_bb_uart_tx_hal.c` | GPT ISR — registered as `p_callback` |
| `rs_bb_uart_tx_ConfigGen()` | 🟢🔒 PUBLIC BRIDGE | `rs_bb_uart_tx_data_fpb_<board>.c` | Any, before `ConfigCheck()` |
| `rs_bb_uart_tx_ConfigCheck()` | 🟢🔒 PUBLIC BRIDGE | `rs_bb_uart_tx_data_fpb_<board>.c` | Any, once |

---

## Integration Guide

*Read this when you are: Wiring the Bit-Banged UART into your application.*

> This guide covers **how to wire the module into your application** — initialization order and calling-context rules. For **adapting the core logic to a different output or use case**, see [Reusability Guide](#reusability-and-adaptation-for-similar-use-cases). For the functions referenced below, see [Functions Guide](#functions-guide--public-and-internal-functions).

### 1. Minimal Integration

```c
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra4e2.h>   // pick your board's preset header

/* Configuration structure — must remain valid (static/global storage) for
 * the life of the application; the singleton retains a pointer to it. */
static rs_bb_uart_tx_cfg_t g_bb_uart_tx_cfg;

void app_init(void)
{
    rs_bb_uart_tx_status_t status;

    /* 1. Populate defaults + bind the GPT instance and TX pin */
    status = rs_bb_uart_tx_ConfigGen(&g_bb_uart_tx_cfg, &g_timer0_preset, BSP_IO_PORT_04_PIN_11);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status) { /* handle: NULL pointer, a programming error */ }

    /* 2. Optional: override any ConfigGen() default before ConfigCheck() */
    // g_bb_uart_tx_cfg.baud_rate = 9600U;

    /* 3. Validate AND open — module is READY on success */
    status = rs_bb_uart_tx_ConfigCheck(&g_bb_uart_tx_cfg);
    if (RS_BB_UART_TX_STATUS_SUCCESS != status) { /* handle: bad baud rate, GPT/GPIO HAL failure */ }

    /* 4. From here on, your application calls only: */
    rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\r\n");
}
```

There are no separate `Open()`/`Start()`/`Stop()`/`Close()` public calls — see [Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use) for why, and [Architecture](#1-lifecycle-state-machine) for the internal state machine this sequence drives.

---

### 2. Calling-Context Rules

| Function | Valid Calling Context | Notes |
|---|---|---|
| `rs_bb_uart_tx_ConfigGen()` | Startup / main-loop / task context | Not ISR-safe |
| `rs_bb_uart_tx_ConfigCheck()` | Startup / main-loop / task context | Not ISR-safe; call once |
| `rs_bb_uart_tx_ConfigGetSize()` | Any context | Not ISR-safe (no reason to call from an ISR) |
| `rs_bb_uart_tx_Write()` / `WriteString()` | Startup / main-loop / task context **only** | **NOT valid from ISR context in Phase 1.** If your event source is an interrupt, stage the data (e.g. a flag, a small application-owned queue) and call `Write()`/`WriteString()` from the resulting main-loop/task processing, not from the ISR itself |
| `rs_bb_uart_tx_StatusGet()` | Any context | Callable from an ISR for a best-effort diagnostic read, but not guaranteed atomic — do not base control-flow decisions on a value read this way from within the GPT ISR itself |

**Why `Write()`/`WriteString()` are not ISR-safe in Phase 1:** the ring buffer's enqueue side (`head`) is the producer side by design, and the producer is expected to be main-loop/task context — see [Architecture](#5-ring-buffer-concurrency-model). The GPT ISR is the sole *consumer*. If you need to trigger output from an interrupt, have the ISR set a flag or push into your own small application-level structure, and drain that from your main loop by calling `Write()`/`WriteString()` there.

---

### 3. Sizing the Ring Buffer for Your Traffic Pattern

`RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` (default 512, must be a power of two) determines how much data can be queued before a `Write()`/`WriteString()` call returns `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`. Usable capacity is one byte less than the configured size (see [Architecture](#5-ring-buffer-concurrency-model) for why).

Sizing guidance:

- **Larger than your single largest expected `Write()`/`WriteString()` call**, plus enough headroom for whatever else might already be queued and draining. The reference demo's ~300-byte startup banner, sent as one call, is why the default is 512 rather than the design doc's nominal 256 — see [Behavior and Performance](#5-ram-consumption).
- **Drain rate is fixed by baud rate**, not by buffer size: at 115200 baud, roughly one byte transmits every ~87 µs (10 bit periods per byte, 8N1). A full 512-byte buffer drains in a little under 45 ms at 115200 baud, and proportionally longer at lower baud rates — plan buffer size and call frequency together if your application transmits continuously.
- Reduce the buffer size (256/128/64) to save RAM in applications with small, infrequent messages and no risk of the reference demo's startup-banner burst pattern.

---

### 4. Handling `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`

Phase 1's buffer-full policy is fixed: **return error, never overwrite, never silently drop, never block.** Your application must decide what to do on `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`:

- **Retry later** (e.g. next main-loop iteration) if the message can wait.
- **Drop and count** if losing an occasional diagnostic line is acceptable — `rs_bb_uart_tx_StatusGet()`'s `buffer_full_count` field already gives you this count for free without extra bookkeeping.
- **Reduce message size or frequency** if buffer-full events are frequent — this usually indicates the buffer is undersized for your traffic pattern relative to the configured baud rate (see [Section 3](#3-sizing-the-ring-buffer-for-your-traffic-pattern)).

There is no blocking-write option in Phase 1 — see [Known Limitations](#known-limitations-phase-1-by-design).

---

### 5. Bare-Metal Main-Loop Integration Pattern (Reference)

The reference demo (`rs_bb_uart_tx_demo.c`) demonstrates the simplest integration pattern for a bare-metal (no-RTOS) application:

```c
void rs_bb_uart_tx_demo_run(void)
{
    /* ... ConfigGen() / ConfigCheck() as in Section 1 ... */

    demo_print_banner();   /* single WriteString() call */

    while (1)
    {
        R_BSP_SoftwareDelay(DEMO_HEARTBEAT_PERIOD_MS, BSP_DELAY_UNITS_MILLISECONDS);
        heartbeat_count++;

        if (0U == (heartbeat_count % DEMO_STATUS_EVERY_N_HEARTBEATS))
        {
            demo_print_status(heartbeat_count);   /* StatusGet() + WriteString() */
        }
        else
        {
            demo_print_heartbeat(heartbeat_count); /* WriteString() */
        }
    }
}
```

This pattern generalizes directly: any main-loop/task-context code path that wants to emit output calls `WriteString()`/`Write()` and checks the return status; the GPT ISR handles everything else asynchronously in the background.

---

### 6. Single-Instance Constraint

`rs_bb_uart_tx` is a **singleton**: exactly one GPIO pin, one GPT channel, one instance per application, internally owned as a static control block (`g_bb_uart_tx_ctrl`). There is no caller-managed control block to declare or pass around, and no supported way to run two independent instances in the same application (e.g. two different TX pins at two different baud rates simultaneously) in Phase 1.

If your application genuinely needs two independent software UART outputs, you would need to duplicate and rename the entire `src/rs_bb_uart_tx/` module (a distinct short name, distinct prefix, distinct static control block) — this is an architectural change beyond Phase 1's scope, not a supported configuration option. See [Reusability Guide](#reusability-and-adaptation-for-similar-use-cases) for what does and does not adapt easily.

---

## File Guide — Naming Conventions and What Is Safe to Modify

*Read this when you are: Deciding what to copy, keep, or replace.*

### 1. Core Code vs. Reference Application Code

The following table identifies which files are reusable core code and which belong only to the reference/demo application layer. The configuration path column refers to the two options explained fully in [Configuration Guide](#5-adapting-to-your-board): the **Preset path** (default, uses a standalone pre-built GPT instance) and the **FSP Stack Configurator path** (for custom boards or modified GPT settings).

| Layer | Files | Purpose | Reuse Expectation |
|---|---|---|---|
| **Core** | `src/rs_bb_uart_tx/rs_bb_uart_tx.c` | Public function implementations; internal lifecycle state machine (`rs_bb_uart_tx_open_internal()`, `rs_bb_uart_tx_start_internal()`); owns the single static control block `g_bb_uart_tx_ctrl` | Copy into your project; core orchestration logic |
| **Core** | `src/rs_bb_uart_tx/rs_bb_uart_tx.h` | Public interface header — all six public function prototypes, module version macros | Include directly; do not modify |
| **Core** | `src/rs_bb_uart_tx/rs_bb_uart_tx_types.h` | Public type definitions: `rs_bb_uart_tx_cfg_t`, `rs_bb_uart_tx_state_t`, `rs_bb_uart_tx_status_t`, lifecycle state enum | Include directly; do not modify |
| **Core (internal)** | `src/rs_bb_uart_tx/rs_bb_uart_tx_ctrl.h` | Internal-only control-block definition (`rs_bb_uart_tx_ctrl_t`) and the `rs_bb_uart_tx_open_internal()` declaration; not included by the public header, not visible to application code | Copy into your project alongside the rest of core; not an application-facing header |
| **Core** | `src/rs_bb_uart_tx/rs_bb_uart_tx_cfg.h` | Build-time configuration constants with `#ifndef` guards | Override individual constants as needed for your project (or via `-D` on the compiler command line) |
| **Core (internal)** | `src/rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.c/.h` | SPSC ring buffer manager — all-or-nothing enqueue, ISR-context dequeue, lock-free head/tail | Copy into your project; hardware-agnostic |
| **Core (internal)** | `src/rs_bb_uart_tx/rs_bb_uart_tx_engine.c/.h` | TX bit-level state machine (start/data/stop bit sequencing); calls back into the HAL shim only — never touches FSP types directly, so it is unit-testable on the host | Copy into your project; hardware-agnostic |
| **Core (internal)** | `src/rs_bb_uart_tx/rs_bb_uart_tx_hal.c/.h` | GPT timer adapter + GPIO adapter + the GPT ISR-context callback. The **only** file that includes GPT-specific FSP headers and drives the TX pin via direct BSP pin control | Copy into your project; this is the file to change if porting to a different timer peripheral (e.g. AGT instead of GPT) |
| **Preset Config** | `src/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_<board>.c/.h` | Pre-built, standalone GPT instance definition per board (`g_timer0_preset`). Also hosts the `rs_bb_uart_tx_ConfigGen()` and `rs_bb_uart_tx_ConfigCheck()` bridge function **definitions** — declared in the public `rs_bb_uart_tx.h` but defined here, unconditionally, so both configuration paths can call them | Reference only — do not edit; for a new board, add a new `rs_bb_uart_tx_data_fpb_<board>.c/.h` following this pattern |
| **Reference App** | `src/rs_bb_uart_tx_demo.c` | Full demonstration: generated-configuration setup, startup banner, bare-metal main-loop heartbeat, periodic `StatusGet()` printout. One copy per board project, differing only in the board name string, included config header, and TX pin argument | Adapt or replace entirely for your application |
| **Reference App** | `src/hal_entry.c` | FSP application hook; calls `rs_bb_uart_tx_demo_run()` | Adapt for your application's entry-point flow |
| **Reference App** | `src/hal_warmstart.c` | RASC-provided warm-start boilerplate | Adapt for your application if needed |
| **Reference App** | `src/fault_handler.c` | Fault exception handler for debug. **Differs by core**: FPB-RA2E3 (Cortex-M23, ARMv8-M Baseline) has a smaller register set (`SHCSR` only); FPB-RA4E2/FPB-RA6E2 (Cortex-M33, ARMv8-M Mainline) capture the full `CFSR`/`HFSR`/`MMFAR`/`BFAR` set — see the file's own top-of-file comment for the CMSIS-header-verified rationale | Optional; adapt or replace with your production fault handling |
| **FSP Generated / Vendored** | `ra/`, `ra_gen/` directories | FSP-generated configuration, drivers, middleware, CMSIS, board support. `ra/fsp/src/r_gpt/r_gpt.c` and `ra/fsp/src/r_ioport/` are genuinely vendored in every board project in this delivery — no additional Smart Configurator step is required to build | Do not edit manually; regenerate via the FSP Smart Configurator, the CMake pre-configure RASC step, or the Meson build's `rasc_generate` run target if you change `configuration.xml` |

**Key separation:** the demo's banner text, heartbeat cadence, and status-line formatting in `rs_bb_uart_tx_demo.c` are entirely reference-application code and are not part of the reusable module. The module core in `src/rs_bb_uart_tx/` has no knowledge of what an application writes to it — it only knows how to move bytes onto a GPIO pin at the configured baud rate.

---

### 2. Naming Conventions Used in This Module

All identifiers follow a consistent scheme using the `rs_` / `RS_` prefix, which identifies Renesas reference software and avoids conflicts with FSP middleware names (`rm_` / `RM_`). The short name for this module is `bb_uart_tx`.

| Category | Pattern | Examples |
|---|---|---|
| **Public functions** | `rs_bb_uart_tx_OperationName()` — lowercase prefix, UpperCamelCase operation | `rs_bb_uart_tx_Write()`, `rs_bb_uart_tx_WriteString()`, `rs_bb_uart_tx_StatusGet()` |
| **Internal (non-static) helper functions** | `rs_bb_uart_tx_*` — all lowercase | `rs_bb_uart_tx_open_internal()`, `rs_bb_uart_tx_hal_gpt_open()`, `rs_bb_uart_tx_engine_advance()`, `rs_bb_uart_tx_ring_buffer_enqueue()` |
| **File-local (`static`) helper functions** | `rs_bb_uart_tx_*` — all lowercase, scoped to one `.c` file | `rs_bb_uart_tx_start_internal()` (file-local to `rs_bb_uart_tx.c`) |
| **Build-time macros and config symbols** | `RS_BB_UART_TX_CFG_*` — all uppercase | `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES`, `RS_BB_UART_TX_CFG_GPT_IPL`, `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE` |
| **Version macros** | `RS_BB_UART_TX_VERSION*` — all uppercase | `RS_BB_UART_TX_VERSION_MAJOR`, `RS_BB_UART_TX_VERSION` |
| **Enumeration type names** | `rs_bb_uart_tx_*_t` — lowercase with `_t` suffix | `rs_bb_uart_tx_lifecycle_state_t`, `rs_bb_uart_tx_status_t`, `rs_bb_uart_tx_bit_state_t` |
| **Enumeration values** | `RS_BB_UART_TX_GROUP_VALUE` — all uppercase | `RS_BB_UART_TX_LIFECYCLE_READY`, `RS_BB_UART_TX_STATUS_TX_BUFFER_FULL`, `RS_BB_UART_TX_ENGINE_STATE_TX_IDLE` |
| **Struct type names** | `rs_bb_uart_tx_*_t` — lowercase with `_t` suffix | `rs_bb_uart_tx_cfg_t`, `rs_bb_uart_tx_ctrl_t`, `rs_bb_uart_tx_state_t` |
| **Source and header files** | `rs_bb_uart_tx_*.c / .h` — lowercase | `rs_bb_uart_tx.c`, `rs_bb_uart_tx_cfg.h`, `rs_bb_uart_tx_ring_buffer.h` |
| **Core source folder** | `src/rs_bb_uart_tx/` | All core module files live here |
| **Board config files** | `rs_bb_uart_tx_data_fpb_<board>.c/.h` | `rs_bb_uart_tx_data_fpb_ra2e3.c/.h`, `rs_bb_uart_tx_data_fpb_ra4e2.c/.h`, `rs_bb_uart_tx_data_fpb_ra6e2.c/.h` |
| **Project name** | `BitBangedUART_TX_<board>` — descriptive folder name | `BitBangedUART_TX_FPB_RA2E3` |

The key rule to remember: **public functions use the lowercase `rs_bb_uart_tx_` prefix** with an UpperCamelCase operation name (e.g. `rs_bb_uart_tx_WriteString`). Everything that is a macro, constant, or enumeration value uses the **all-uppercase `RS_BB_UART_TX_`** form. Type names and internal helpers are always all-lowercase.

---

### 3. Core Files — Copy and Keep; Modify with Care

These files contain reusable reference code. Copy them into your project. Modify only at the documented adaptation points. If you later adopt a newer release of this module, these files will require a merge review.

| File | Safe to Modify? | Notes |
|---|---|---|
| `rs_bb_uart_tx.c` | With care | Core orchestration and lifecycle logic; the internal `Open()`/`Start()` helpers are not exposed publicly by design (see [Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use)) |
| `rs_bb_uart_tx.h` | No | Public interface; adding or removing functions breaks compatibility |
| `rs_bb_uart_tx_types.h` | No | Public types; changing structures may break compatibility with any code that reads `rs_bb_uart_tx_cfg_t`/`rs_bb_uart_tx_state_t` fields |
| `rs_bb_uart_tx_ctrl.h` | No | Internal control-block layout; the GPT ISR callback is wired to the one static instance via `p_context` at open time — do not declare additional instances |
| `rs_bb_uart_tx_cfg.h` | **Yes** | Override any `#ifndef`-guarded constant with your project-specific value |
| `rs_bb_uart_tx_ring_buffer.c/.h` | With care | Requires `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` to remain a power of two (enforced by `_Static_assert`) — the lock-free wraparound uses a bitmask, not `%` |
| `rs_bb_uart_tx_engine.c/.h` | With care | Hardware-agnostic bit-level state machine; portable and host-unit-testable in isolation since it never includes FSP headers |
| `rs_bb_uart_tx_hal.c/.h` | With care | The one file that touches FSP GPT/GPIO types directly — modify only if switching timer peripheral or pin-control mechanism |

---

### 4. Preset Configuration Files — Reference Only; Do Not Edit

These files are pre-built for FPB-RA2E3, FPB-RA4E2, and FPB-RA6E2 and are marked "do not edit." Each is a reference-quality, standalone GPT instance definition for that board — not an alias of the FSP Stack Configurator's own `ra_gen/hal_data.c` instance, though both are wired in and either can be used (see [Configuration Guide](#5-adapting-to-your-board), `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE`).

| File | Notes |
|---|---|
| `config/rs_bb_uart_tx_data_fpb_ra2e3.c/.h` | Preset GPT ch0 instance for FPB-RA2E3: undivided PCLKD (48 MHz), TX pin P109, `cycle_end_ipl = 1` (hardcoded — Cortex-M23 supports only priorities 0–3). Also defines `rs_bb_uart_tx_ConfigGen()`/`ConfigCheck()`; do not edit |
| `config/rs_bb_uart_tx_data_fpb_ra4e2.c/.h` | Preset GPT ch0 instance for FPB-RA4E2: PCLKD/4 divider, TX pin P411, `cycle_end_ipl = RS_BB_UART_TX_CFG_GPT_IPL` (default 6). Also defines `rs_bb_uart_tx_ConfigGen()`/`ConfigCheck()`; do not edit |
| `config/rs_bb_uart_tx_data_fpb_ra6e2.c/.h` | Preset GPT ch0 instance for FPB-RA6E2: PCLKD/4 divider, TX pin P411, `cycle_end_ipl = RS_BB_UART_TX_CFG_GPT_IPL` (default 6). Also defines `rs_bb_uart_tx_ConfigGen()`/`ConfigCheck()`; do not edit |

To customize the GPT configuration for a different board, switch to the FSP Stack Configurator path as described in [Configuration Guide](#5-adapting-to-your-board) (`RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 0`), or add a new `rs_bb_uart_tx_data_fpb_<board>.c/.h` pair following the existing pattern.

---

### 5. Reference Application — Adapt Freely

These files demonstrate the module but are not intended for direct reuse. They contain board-specific and demo-specific scaffolding. Use them as a starting point and replace what you don't need.

| File | Adaptation Guidance |
|---|---|
| `rs_bb_uart_tx_demo.c` | Starting point for your own application entry point. One copy per board project; differs only in the board name string in the banner, the included board config header, and the `tx_pin` argument passed to `rs_bb_uart_tx_ConfigGen()` — see [Functions Guide](#functions-guide--public-and-internal-functions) |
| `hal_entry.c` | FSP application hook; adapt if your application needs a different entry sequence |
| `hal_warmstart.c` | Warm-start detection helper; adapt if your system uses a different reset-handling strategy |
| `fault_handler.c` | Debug fault handler; replace with your production fault handling. Remember this file's content genuinely differs between the Cortex-M23 board (FPB-RA2E3) and the two Cortex-M33 boards (FPB-RA4E2/FPB-RA6E2) — do not copy the wrong variant across cores |

---

### 6. FSP-Generated / Vendored Files — Do Not Edit Manually

| Directory | Notes |
|---|---|
| `ra/` | FSP-generated drivers, middleware, CMSIS, and board support, plus the genuinely vendored `r_gpt` (GPT driver source) and `r_ioport` (pin-configuration support types). No additional Smart Configurator step is required to build any of the three board projects as delivered |
| `ra_gen/` | Auto-generated by the FSP Smart Configurator from `configuration.xml`; overwritten on every re-generation. Includes the vector table wiring for the GPT counter-overflow interrupt (`ra_gen/vector_data.h/.c`) that the preset GPT instance's ISR depends on |
| `ra_cfg/` | Build-time FSP/BSP/driver configuration headers generated alongside `ra_gen/` |

If you ever reopen the FSP Smart Configurator perspective for one of these projects (via the `FSP Smart Configurator` VS Code task, or by double-clicking `configuration.xml`), see [Configuration Guide](#3-required-non-default-fsp-settings) for the settings that must be preserved per board so you don't accidentally revert them.

---

### 7. VS Code / CMake Project Files

Files specific to the VS Code / CMake delivery mechanism :

| File | Purpose | Safe to Modify? |
|---|---|---|
| `CMakeLists.txt` | Top-level CMake project definition; includes `Config.cmake` and the RASC-generated `cmake/GeneratedCfg.cmake` / `cmake/GeneratedSrc.cmake` | With care |
| `CMakePresets.json` | Debug/Release configure and build presets (Ninja generator) | Yes — add presets as needed |
| `CMakeProject.json` | Device/core/toolchain metadata consumed by the Renesas VS Code extensions (device name, entry symbol, include/source paths) | No — regenerated by tooling; treat as generated |
| `Config.cmake` | User-editable hook for `RASC_EXE_PATH` resolution | Yes — set `RASC_EXE_PATH` here or via environment/CLI |
| `cmake/GeneratedCfg.cmake`, `cmake/GeneratedSrc.cmake` | RASC-generated compiler flags and source-file globs (`ra_gen/`, `src/`, `ra/`) | No — regenerated by RASC; edit `CMakeLists.txt` to override instead |
| `cmake/llvm.cmake` | ATfE (LLVM Embedded Toolchain for Arm) toolchain file; resolves `ARM_TOOLCHAIN_PATH` | No — regenerated by RASC |
| `cmake/prebuild.cmake` | Backing script for the "Regenerate Project Content" VS Code task | No — regenerated by RASC |
| `.vscode/cmake-kits.json`, `.vscode/tasks.json`, `.vscode/launch.json`, `.vscode/c_cpp_properties.json`, `.vscode/settings.json` | VS Code CMake Tools kit definitions, build/RASC tasks, hardware debug launch configuration, IntelliSense paths | Yes — adjust for your local toolchain paths and workflow |
| `*.code-workspace` | VS Code multi-root workspace definition (single-folder here). **This is how you open the project** — double-click it, or use VS Code's File → Open Workspace from File... There is no e2studio-style import step | Yes |
| `*.jlink`, `JLinkLog.log` | SEGGER J-Link debugger configuration / log | `.jlink` yes if adapting debug probe settings; `JLinkLog.log` is a log artifact, not source |
| `.cproject`, `.project`, `.settings/`, `.api_xml`, `.sdm_json`, `.secure_*`, `.r365project` | e2studio/RASC/Renesas-365 project metadata carried alongside the CMake project for tooling compatibility | No — tooling-managed |

---

## Design Flow and Architecture

*Read this when you are: Understanding internal design or debugging state/timing issues.*

### 1. Lifecycle State Machine

The module implements a five-state internal lifecycle. Unlike modules that expose `Open()`/`Start()`/`Stop()`/`Close()` as public functions, every transition here happens as a side effect of a public function documented in [Design Pattern: Single-Call, Simple-Use](#21-design-pattern-single-call-simple-use) — there is no public lifecycle API to call directly.

```
                 rs_bb_uart_tx_ConfigGen()      rs_bb_uart_tx_ConfigCheck()
UNCONFIGURED ───────────────────────────→ CONFIGURED ───────────────────────→ READY
                                                                                 │  ↑
                                                                    Write()/     │  │ queue drains
                                                              WriteString()      │  │ (GPT ISR,
                                                              queues data,       │  │  queue_now_empty)
                                                              starts GPT         ▼  │
                                                                             TRANSMITTING

     Any state, on unrecoverable HAL failure
              (GPT/GPIO open or start failure)
                         │
                         ▼
                      ERROR
```

| State | Description |
|---|---|
| `RS_BB_UART_TX_LIFECYCLE_UNCONFIGURED` | Initial state. No valid configuration bound. `ConfigGen()` has not (yet) succeeded. |
| `RS_BB_UART_TX_LIFECYCLE_CONFIGURED` | `ConfigGen()` has populated `*p_cfg`, but `ConfigCheck()` has not yet been called (or a re-attempt is pending). No HAL resources opened yet. |
| `RS_BB_UART_TX_LIFECYCLE_READY` | `ConfigCheck()` succeeded: configuration validated, GPT and GPIO HAL resources opened, ring buffer and TX bit-engine reset. Idle, ready to transmit. |
| `RS_BB_UART_TX_LIFECYCLE_TRANSMITTING` | The GPT timer is running and its ISR is actively driving bits onto the TX pin. Entered when `Write()`/`WriteString()` successfully enqueues data while the module was idle (or already TRANSMITTING). Returns to READY automatically, from ISR context, once the ring buffer is confirmed empty. |
| `RS_BB_UART_TX_LIFECYCLE_ERROR` | Deterministic error state, entered from any state on an unrecoverable GPT/GPIO HAL failure. Requires re-initialization (a fresh `ConfigGen()`/`ConfigCheck()` sequence is not idempotent while already past `CONFIGURED` — see [Section 2](#2-public-function-state-gating)). |

**There is no public transition back out of ERROR, TRANSMITTING→READY (other than the automatic queue-drain transition above), or READY→UNCONFIGURED in Phase 1.** The internal `rs_bb_uart_tx_open_internal()` helper (called from the board configuration file's `ConfigCheck()` implementation) only accepts the UNCONFIGURED or CONFIGURED starting states — see [Known Limitations](#known-limitations-phase-1-by-design).

---

### 2. Public Function State Gating

| Public Function | UNCONFIGURED | CONFIGURED | READY | TRANSMITTING | ERROR |
|---|---|---|---|---|---|
| `rs_bb_uart_tx_ConfigGen()` | ✓ (→ CONFIGURED) | ✓ (re-populates `*p_cfg`; does not itself change lifecycle state) | ✓ | ✓ | ✓ |
| `rs_bb_uart_tx_ConfigCheck()` | ✗ `INVALID_POINTER`¹ | ✓ (→ READY on success) | `INVALID_STATE` | `INVALID_STATE` | `INVALID_STATE` |
| `rs_bb_uart_tx_ConfigGetSize()` | ✓ | ✓ | ✓ | ✓ | ✓ |
| `rs_bb_uart_tx_Write()` | `INVALID_STATE` | `INVALID_STATE` | ✓ (→ TRANSMITTING) | ✓ (queues more; stays TRANSMITTING) | `INVALID_STATE` |
| `rs_bb_uart_tx_WriteString()` | `INVALID_STATE` | `INVALID_STATE` | ✓ (→ TRANSMITTING) | ✓ | `INVALID_STATE` |
| `rs_bb_uart_tx_StatusGet()` | ✓ (fields read as zero/UNCONFIGURED) | ✓ | ✓ | ✓ | ✓ |

¹ `ConfigCheck()` called before a successful `ConfigGen()` will typically fail earlier, on `p_cfg->p_gpt_instance == NULL`, with `RS_BB_UART_TX_STATUS_INVALID_POINTER` — `ConfigGen()` is the call that transitions UNCONFIGURED → CONFIGURED.

Calling `ConfigCheck()` a second time after it has already succeeded (state READY, TRANSMITTING, or ERROR) returns `RS_BB_UART_TX_STATUS_INVALID_STATE` and increments the `invalid_call_count` diagnostic — this is not idempotent by design in Phase 1 (Architecture & Design: "Runtime remapping of GPIO or timer while transmission is active" is explicitly out of scope).

Every public function documents its own complete return-value list in [Functions Guide](#functions-guide--public-and-internal-functions) and in `rs_bb_uart_tx.h`.

---

### 3. TX Bit-Engine Pipeline and Data Flow

```
Application (main-loop / task context)
  │
  └──→ rs_bb_uart_tx_Write(p_data, length) / rs_bb_uart_tx_WriteString(p_str)
            │
            ├── rs_bb_uart_tx_ring_buffer_enqueue()   ← all-or-nothing copy into
            │                                            the static SPSC ring buffer
            └── rs_bb_uart_tx_start_internal()         ← starts the GPT timer if the
                    │                                     module was idle (READY);
                    │                                     no-op if already TRANSMITTING
                    ↓
         (call returns here — non-blocking; data is queued, not yet transmitted)


GPT ISR — rs_bb_uart_tx_hal_gpt_callback(), fires once per bit period
  │
  ├── 1. rs_bb_uart_tx_engine_output_get(&engine)     ← O(1), NO ring-buffer access.
  │        Returns the pin level already decided by
  │        the *previous* tick's advance() call.
  │
  ├── 2. rs_bb_uart_tx_hal_gpio_write(p_ctrl, level)  ← GPIO write happens FIRST,
  │        R_BSP_PinAccessEnable()/PinWrite()/           with zero variable-cost work
  │        PinAccessDisable()                            ahead of it on every tick.
  │
  ├── 3. rs_bb_uart_tx_engine_advance(&engine, &rb)   ← runs AFTER the pin write.
  │        Prepares the level for the *next* tick and     Dequeues from the ring buffer
  │        advances the bit-level FSM (see below).        here when a new byte is needed —
  │                                                        this is safe because it no longer
  │                                                        affects the tick that already ran.
  │
  ├── if adv.byte_transmitted:
  │        total_bytes_transmitted++
  │
  └── if adv.queue_now_empty:
           rs_bb_uart_tx_hal_gpt_stop(p_ctrl)          ← stop interrupting once idle
           state = READY                                (TRANSMITTING → READY)
```

#### 3.1 Why the Callback Is Split Into Two Halves

The GPT ISR callback deliberately performs `output_get()` (read the pre-decided pin level) **before** `advance()` (decide the *next* level, dequeuing from the ring buffer if needed). This pipelining means the GPIO write for the current tick never has to wait on a ring-buffer access — only the *next* tick's preparation does, and that preparation has a full bit period of slack (as little as ~8.68 µs at 115200 baud) rather than needing to complete before the pin write.

#### 3.2 Bit-Level State Machine

```
TX_IDLE --(byte available)--> TX_START_BIT --> TX_DATA_BITS --> TX_STOP_BIT
   ^                                                                  |
   |                                                                  v
   +----------(queue empty, freshly re-checked)----- TX_NEXT_BYTE <---+
                                                            |
                                                            +--(another byte queued)--> TX_START_BIT
```

Frame format (fixed in Phase 1): **8N1, idle high, LSB-first.**

| Segment | Level | Duration |
|---|---|---|
| Idle | High | (between frames) |
| Start bit | Low | 1 bit period |
| Data bits 0–7 | LSB first | 8 bit periods |
| Stop bit | High | 1 bit period |

`TX_STOP_BIT`'s `advance()` call *prefetches* the next dequeue one tick early — since dequeuing from an empty queue is a harmless no-op, doing so speculatively costs nothing if it finds nothing. But the actual decision to stop the GPT timer is **never** taken from that prefetch: only `TX_NEXT_BYTE`'s `advance()` — one tick later, performing a fully fresh dequeue — is allowed to declare the queue empty and stop the timer. This means a `Write()` call landing in the narrow window between the prefetch and the fresh re-check is always picked up correctly; nothing queued during that window is ever stranded until the next transmission is manually kicked off.

**One accepted, deliberate launch-latency cost:** the very first bit period after an idle-to-transmitting transition always outputs idle-high (a "wasted" bit period) before the first byte's start bit begins on the following tick — about one bit period of extra latency, once per transition from idle to transmitting. This is a one-time launch-latency cost, not per-bit jitter, and has no effect on a receiver, which does not care when a transmission begins, only that its bit timing is accurate once it does.

---

### 4. Module File Responsibilities

| Module | Responsibility |
|---|---|
| `rs_bb_uart_tx.c` | Public function implementations; internal lifecycle state machine; owns the single static control block `g_bb_uart_tx_ctrl`; RAM-budget `_Static_assert` against `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` |
| `rs_bb_uart_tx_ring_buffer.c` | Static, fixed-capacity SPSC ring buffer. All-or-nothing enqueue (producer, main-loop/task context); single-byte dequeue (consumer, GPT ISR context). Lock-free — see [Section 5](#5-ring-buffer-concurrency-model) |
| `rs_bb_uart_tx_engine.c` | Hardware-agnostic TX bit-level state machine. Never includes FSP headers directly — calls back into the HAL shim to actually drive the pin, so it is unit-testable on the host |
| `rs_bb_uart_tx_hal.c` | GPT timer adapter (open/start/stop/close, period calculation via `infoGet()`), GPIO adapter (direct BSP pin control), and the GPT ISR-context callback that ties the engine and ring buffer together on every tick. The only file that includes GPT-specific FSP headers |
| `rs_bb_uart_tx_data_fpb_<board>.c/.h` | Board preset GPT instance; also hosts `rs_bb_uart_tx_ConfigGen()`, `rs_bb_uart_tx_ConfigCheck()`, `rs_bb_uart_tx_ConfigGetSize()` — see [Functions Guide](#2-configuration-bridge) |

---

### 5. Ring Buffer Concurrency Model

The ring buffer is a classic **single-producer/single-consumer (SPSC)** design:

- `head` is written **only** by the producer (`rs_bb_uart_tx_ring_buffer_enqueue()`, called from main-loop/task context via `Write()`/`WriteString()`).
- `tail` is written **only** by the consumer (`rs_bb_uart_tx_ring_buffer_dequeue()`, called exclusively from GPT ISR context via the TX engine).

Because each side only ever *writes* the index it owns and only *reads* the index it doesn't, and because a naturally-aligned 32-bit store to `head` or `tail` is indivisible with respect to interrupts on this MCU family, correctness does **not** require disabling interrupts anywhere in the enqueue/dequeue path. Both fields are `volatile` so each side always observes the other's latest published value rather than a compiler-cached stale one. Occupancy is derived on demand from `head - tail` rather than tracked in a separate shared `count` field, which removes the need for a critical section on either operation.

Because `head == tail` must remain an unambiguous "empty" signal with no separate `count` field, the design reserves one slot: usable capacity is `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1` bytes, one fewer than the raw storage size — the standard, well-known trade-off for a head/tail-only circular buffer. Index wraparound uses a bitmask (`& (SIZE - 1)`), not `%`, which is why `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` must be a power of two (`64`/`128`/`256`/`512`/`1024` are the documented reference sizes; enforced by a `_Static_assert` in `rs_bb_uart_tx_ring_buffer.h`).

See [Behavior and Performance](#demonstrated-behavior-and-performance) for RAM budget details and [Configuration Guide](#configuration-guide) for why the default buffer size was raised from the design doc's nominal 256 bytes to 512 bytes.

---

## Demonstrated Behavior and Performance

*Read this when you are: Validating the module meets your timing/memory budget.*

### 1. Functional Behavior Demonstrated

- Generated-configuration path (`ConfigGen()` → `ConfigCheck()`) bringing the module from UNCONFIGURED to READY on all three boards
- A single `WriteString()` call transmitting a multi-line startup banner, sent as one call (not several back-to-back calls) so the entire banner is queued before the GPT timer starts — see [Section 4](#4-isr-and-enqueue-performance)
- A bare-metal main-loop heartbeat, printing `[NNNNNN] Heartbeat OK` once per second via `R_BSP_SoftwareDelay()` + `WriteString()`
- A periodic `StatusGet()` snapshot printed every 5th heartbeat instead of the plain heartbeat line, demonstrating `queued`/`free`/`tx`/`ok`/`full` diagnostic counters
- Default 115200 baud, 8N1, LSB-first framing, verified against the on-board J-Link VCOM path with no external adapter required, on all three reference boards
- Non-blocking `Write()`/`WriteString()` behavior: each call returns as soon as data is queued, without waiting for the GPT ISR to drain it

---

### 2. Bit-Period Timing Reference

The nominal bit period is `1 / baud_rate`. Reference values used throughout this module's design and validation:

| Baud Rate | Bit Period |
|---|---|
| 1200 | 833.33 µs |
| 2400 | 416.67 µs |
| 4800 | 208.33 µs |
| 9600 | 104.17 µs |
| 19200 | 52.08 µs |
| 38400 | 26.04 µs |
| 57600 | 17.36 µs |
| 115200 | 8.68 µs |

The actual GPT period register value is derived at `ConfigCheck()` time (inside `rs_bb_uart_tx_hal_gpt_open()`) using integer-only, round-to-nearest arithmetic — never floating point:

```
period_counts = round(clock_frequency / baud_rate) - 1
```

implemented as:

```c
period_counts = (clock_frequency + (baud_rate / 2U)) / baud_rate;
if (period_counts > 0U) { period_counts -= 1U; }
```

`clock_frequency` is queried at runtime from the opened GPT instance via `timer_api_t::infoGet()` — not assumed from a compile-time clock-tree constant — so this same code runs unmodified regardless of which prescaler/divider a given board's GPT configuration uses. Only the *input* clock frequency changes per board; see [Configuration Guide](#configuration-guide) for each board's actual clock tree and divider choice. Rounding to nearest (rather than truncating) minimizes baud-rate error at every supported rate — truncation alone would bias the calculated period consistently short.

| Board | GPT Clock Source | Divider | Effective Clock | Period Counts @ 115200 baud (approx.) |
|---|---|---|---|---|
| FPB-RA2E3 | HOCO 48 MHz → PCLKD | `TIMER_SOURCE_DIV_1` (undivided) | 48 MHz | ~417 |
| FPB-RA4E2 | HOCO/PLL → PCLKD | `TIMER_SOURCE_DIV_4` | ~100 MHz ÷ 4 = ~25 MHz | ~217 |
| FPB-RA6E2 | HOCO 20 MHz × PLL 10.0 → PCLKD/2 | `TIMER_SOURCE_DIV_4` | ~100 MHz ÷ 4 = ~25 MHz | ~217 |

FPB-RA2E3's undivided clock gives it *better* timing resolution than the other two boards at the same baud rate (~417 counts vs. ~217) — see [Configuration Guide](#configuration-guide) for why each board's divider choice differs.

---

### 3. Baud-Rate Accuracy and Jitter

**Accuracy requirement:** measured baud-rate error shall not exceed **±3%** of the configured baud rate (`RS_BB_UART_TX_CFG_BAUD_ERROR_PPT_MAX = 30` parts-per-thousand), under idle system conditions, representative bare-metal application load, and representative interrupt load — validated across all three reference boards.

**Jitter guidance:** a recommended design target for bit-to-bit jitter is **≤ ±10%** of the ideal bit period. This is a characterization/architecture-review target, not necessarily a hard pass/fail release gate — finalize acceptance criteria during your own validation planning if jitter is critical to your application.

**Primary jitter risk:** interrupt latency. Mitigations already in the design:
- The GPT ISR (`rs_bb_uart_tx_hal_gpt_callback()`) writes the GPIO pin as its *first* action, before any ring-buffer access — see [Architecture](#3-tx-bit-engine-pipeline-and-data-flow).
- `RS_BB_UART_TX_CFG_GPT_IPL` (default 6 on FPB-RA4E2/FPB-RA6E2; hardcoded to 1 on FPB-RA2E3 — see [Configuration Guide](#configuration-guide)) lets you raise the bit-period interrupt's preemption priority relative to other application interrupts if jitter under interrupt load is a concern.

**Recommended measurement equipment:** a logic analyzer or oscilloscope on the TX pin directly gives the most accurate picture of actual bit timing, independent of any USB/VCOM path — see [Section 7](#7-on-board-vcom-baud-range-caveat) for why the VCOM bridge itself is not always representative at the low end of the baud range.

---

### 4. ISR and Enqueue Performance

**`rs_bb_uart_tx_Write()`/`WriteString()` enqueue cost:** a bounded-time, all-or-nothing memory copy into the ring buffer (no division, no critical section — see [Architecture](#5-ring-buffer-concurrency-model)), followed by a GPT-start call only if the module was previously idle. Both calls are non-blocking and return once the data is queued, not once it has transmitted.

**GPT ISR overhead, per bit period:**
1. `rs_bb_uart_tx_engine_output_get()` — O(1), no ring-buffer access.
2. `rs_bb_uart_tx_hal_gpio_write()` — one `R_BSP_PinAccessEnable()`/`PinWrite()`/`PinAccessDisable()` sequence, documented as ISR-safe.
3. `rs_bb_uart_tx_engine_advance()` — prepares the *next* tick's output; dequeues from the ring buffer only when a new byte is needed. This step has a full bit period of slack (as little as ~8.68 µs at 115200 baud), not the sub-microsecond budget of step 2.

No allocation, no blocking, and no application-level callback occurs anywhere in the ISR path. See [Architecture](#3-tx-bit-engine-pipeline-and-data-flow) for the full per-tick sequence and the rationale for splitting the pin write from the ring-buffer access.

**Demo banner send pattern:** the reference demo intentionally builds its ~300-byte startup banner into one local buffer and sends it as a *single* `WriteString()` call, rather than several back-to-back calls. The first `WriteString()` call in a sequence starts the GPT timer; every call after that would enqueue more data while the timer is already running — unnecessary risk for no benefit. A single combined call enqueues everything up front, before the timer starts.

---

### 5. RAM Consumption

**No dynamic memory allocation.** The entire control block (`rs_bb_uart_tx_ctrl_t`, including the ring buffer and TX engine state) is a single static/global instance, `g_bb_uart_tx_ctrl`. Its size is verified at **build time** via `_Static_assert` against `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` (default 2048 bytes) — the build fails rather than silently exceeding budget.

**Default configuration (`RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES = 512`) control-block breakdown, approximate:**

| Component | Approx. Size |
|---|---|
| Ring buffer byte storage (`data[512]`) | 512 bytes |
| Ring buffer `head`/`tail` indices (2 × `volatile uint32_t`) | 8 bytes |
| TX bit-engine state (`state`, `current_byte`, `bit_index`, `next_output_high`) | ~4–8 bytes |
| Diagnostic counters (4 × `uint32_t`) | 16 bytes |
| Version, size, state, `p_cfg` pointer | ~16 bytes |
| **Total (approx.)** | **~560 bytes**, well under the 2048-byte limit |

Query the exact value on your build via `rs_bb_uart_tx_ConfigGetSize()` — it returns `sizeof(rs_bb_uart_tx_ctrl_t)` computed by the compiler for your exact configuration and target ABI, which is authoritative over the estimate above.

**Why the default buffer size is 512, not the design doc's nominal 256 bytes:** the demo application's own startup banner is roughly 304 bytes, sent as a single `WriteString()` call before the GPT timer has drained any of it. Hands-on hardware testing confirmed that 256 bytes causes `StatusGet()`'s `buffer_full_count` to read non-zero (rejected enqueue attempts) during every boot. 512 bytes comfortably accommodates the demo banner with headroom; if your application sends shorter bursts you may safely reduce `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` to 256, 128, or 64 to save RAM — see [Configuration Guide](#configuration-guide).

---

### 6. No Dynamic Allocation, No Floating Point

- **No `malloc`/`free`** anywhere in `rs_bb_uart_tx.c`, `rs_bb_uart_tx_ring_buffer.c`, `rs_bb_uart_tx_engine.c`, or `rs_bb_uart_tx_hal.c`. All storage is static or stack-resident.
- **No floating-point arithmetic.** The GPT period calculation uses integer round-to-nearest division only (see [Section 2](#2-bit-period-timing-reference)).

---

### 7. On-Board VCOM Baud-Range Caveat

`rs_bb_uart_tx_ConfigCheck()` will accept and correctly configure any baud rate in the documented 1200–115200 range on the TX pin itself — the GPT divider math is sized to generate correct signal timing down to 1200 baud regardless of board. However, if you are observing output through the **default on-board J-Link VCOM bridge** in a terminal application (Tera Term, etc.), the *bridge's own firmware* — not this module — may not relay the full range end-to-end. This has been confirmed on **all three reference boards** (FPB-RA2E3, FPB-RA4E2, FPB-RA6E2): the on-board VCOM does not reliably support baud rates below 9600, consistent with all three boards sharing the same on-board J-Link OB debug-probe VCOM firmware family.

To exercise or validate the low end of the baud range (1200–4800) on any board:

- **Use a logic analyzer or oscilloscope directly on the TX pin.** This verifies the module's actual bit timing independent of the VCOM bridge entirely — the only way to observe true 1200 baud signaling through the on-board probe.
- **Use an external TTL-to-USB adapter** wired to a different GPIO, bypassing the on-board VCOM bridge entirely — see [Configuration Guide](#6-using-an-external-ttl-to-usb-adapter) for wiring.

> This is a sub-9600-baud terminal-observability caveat about the on-board debug probe's VCOM firmware, not a functional limitation of `rs_bb_uart_tx` itself.

---

## Reusability and Adaptation for Similar Use Cases

*Read this when you are: Reusing the module for a different use case.*

> This guide covers **adapting the core logic for a different target, pin, or output need**. For **wiring the module into your application as-is** (calling-context rules, buffer sizing), see [Integration Guide](#integration-guide).

### 1. Core Function Reusability

`rs_bb_uart_tx_Write()`/`WriteString()` are the universal entry points and are, by design, **agnostic to what the transmitted bytes represent**. The core module has no knowledge of whether the caller is sending a diagnostic banner, a binary protocol frame, or `printf()`-redirected text — it only knows how to move bytes onto a GPIO pin at the configured baud rate, non-blocking, all-or-nothing. No changes to the core source are required to change *what* you send.

The TX bit-engine (`rs_bb_uart_tx_engine.c`) and ring buffer (`rs_bb_uart_tx_ring_buffer.c`) are also fully hardware-agnostic — neither file includes any FSP header. Only `rs_bb_uart_tx_hal.c` (and the board-specific `rs_bb_uart_tx_data_fpb_<board>.c/.h` preset) touch FSP/GPT/GPIO types directly. This isolation is what makes porting to a new board, or a new timer peripheral, a contained change — see [Architecture](#4-module-file-responsibilities).

---

### 2. Portions Applicable Without Modification

- All of `src/rs_bb_uart_tx/rs_bb_uart_tx.c/.h`, `_types.h`, `_ctrl.h`, `_cfg.h`, `_ring_buffer.c/.h`, and `_engine.c/.h` can be included directly in any FSP v6.5.0 bare-metal project with an available GPT channel and a spare GPIO pin, on any RA MCU, without modification — provided the configuration is supplied via `rs_bb_uart_tx_ConfigGen()` with a valid `timer_instance_t` and pin.
- `rs_bb_uart_tx_hal.c/.h` is also reusable **unmodified** on any RA target whose GPT driver implements the standard `timer_api_t` interface (`open`/`infoGet`/`periodSet`/`reset`/`start`/`stop`/`close`) and whose pin-write path matches this module's `R_BSP_PinAccessEnable()`/`R_BSP_PinCfg()`/`R_BSP_PinWrite()`/`R_BSP_PinAccessDisable()` pattern — true for every RA family MCU using the standard FSP BSP.
- The ring buffer's lock-free SPSC enqueue/dequeue pattern (`rs_bb_uart_tx_ring_buffer.c`) represents a portable, heap-free, single-producer/single-consumer byte-queue pattern reusable in similar producer/ISR-consumer designs beyond UART TX — see [Architecture](#5-ring-buffer-concurrency-model) for the concurrency argument that makes it safe without a critical section.
- The two-phase ISR pipeline pattern (`output_get()` before `advance()`, see [Architecture](#3-tx-bit-engine-pipeline-and-data-flow)) is a reusable technique for any fixed-period ISR that needs to keep its time-critical output action off a variable-cost data-structure access path — applicable well beyond UART bit-banging.

---

### 3. Portions Requiring Modification

- **Board-specific preset config files** (`config/rs_bb_uart_tx_data_fpb_<board>.c/.h`): create a new preset file for other target boards, or switch to the FSP Stack Configurator path (see [Configuration Guide](#5-adapting-to-your-board)). This is where the GPT clock divider, TX pin, and interrupt priority for a given board live.
- **`rs_bb_uart_tx_hal.c`'s GPT clock divider choice** is *not* hardcoded in that file — it lives entirely in each board's `timer_cfg_t.source_div` in the preset config file, so `rs_bb_uart_tx_hal.c` itself needs no change when porting; only the preset config does.
- **Reference application** (`rs_bb_uart_tx_demo.c`, `hal_entry.c`, `hal_warmstart.c`, `fault_handler.c`): replace the banner, heartbeat cadence, and status-print logic with application-specific code. `fault_handler.c` specifically must be re-derived (not copy-pasted) if you port to a different Cortex-M core generation — see [File Guide](#file-guide--naming-conventions-and-what-is-safe-to-modify) for the Cortex-M23 vs. Cortex-M33 register-set difference already handled across the three delivered boards.
- **Frame format** (8N1, LSB-first): fixed in Phase 1 inside `rs_bb_uart_tx_engine.c`'s state machine and `rs_bb_uart_tx_cfg.h`'s `RS_BB_UART_TX_DATA_BITS`/`RS_BB_UART_TX_STOP_BITS` macros. Adding parity or a different stop-bit count requires extending the bit-engine state machine — not a configuration-only change.
- **Buffer-full policy** (return error only): fixed in Phase 1 (`RS_BB_UART_TX_CFG_OVERFLOW_POLICY_RETURN_ERROR` is not `#ifndef`-guarded — it cannot be overridden by a project define). Adding an overwrite-oldest or blocking policy requires modifying `rs_bb_uart_tx_ring_buffer.c` and the public `Write()`/`WriteString()` contract in `rs_bb_uart_tx.c`/`rs_bb_uart_tx.h`.

---

### 4. Example: Porting to a Fourth Board

```c
// New file: config/rs_bb_uart_tx_data_fpb_ra6m5.h  (illustrative — not delivered)

// 1. Determine PCLKD (or equivalent) and GPT channel width for the new MCU,
//    then choose a source_div that keeps period_counts in range at 1200 baud
//    — see the Configuration Guide, Section 4 for the worked reasoning.
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

### 5. Example: Redirecting `printf()` Through the Module

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

Remember the calling-context restriction still applies: `printf()` calls redirected this way must themselves only be made from main-loop/task context, never from an ISR — see [Integration Guide](#2-calling-context-rules).

---

### 6. What This Module Is Not Designed to Do

- **Not a receive path.** There is no RX pin, no RX ring buffer, and no interrupt-driven receive state machine anywhere in this module. Adding receive is a substantial architectural addition (a second GPIO pin sampled at the bit rate, typically via a separate timer or edge-triggered interrupt scheme), not a configuration change.
- **Not multi-instance.** See [Integration Guide](#6-single-instance-constraint) — the singleton design is a deliberate deviation from a multi-instance-capable reference pattern, made because the target use case never needs more than one GPIO pin's worth of software UART output per application.
- **Not a drop-in replacement for a hardware SCI/UART driver's full feature set.** No parity, no configurable stop-bit count, no hardware flow control, no DMA/DTC offload — this module exists specifically for the case where no hardware UART channel is available at all.

---

## Release Notes and Adopting a Newer Release

*Read this when you are: Adopting a newer release of this module.*

### 1. Current Version

| Field | Value |
|---|---|
| Version | 1.0.0 |
| FSP Baseline | v6.5.0 |
| Toolchain | LLVM Embedded Toolchain for Arm (ATfE) |
| Build System (this delivery) | CMake + Ninja + VS Code |
| Reference Targets | FPB-RA2E3, FPB-RA4E2, FPB-RA6E2 |

There is no version-query public function in Phase 1 (unlike some other reference modules that expose a `VersionGet()` call) — the version is available at compile time via the `RS_BB_UART_TX_VERSION`, `RS_BB_UART_TX_VERSION_MAJOR`, `RS_BB_UART_TX_VERSION_MINOR`, and `RS_BB_UART_TX_VERSION_PATCH` macros in `rs_bb_uart_tx.h`, and at runtime via `rs_bb_uart_tx_StatusGet()`'s `version` field (`rs_bb_uart_tx_state_t.version`).

---

### 2. Delivery Scope

This is the **Phase 1** delivery: TX-only, 8N1, LSB-first, 1200–115200 baud, non-blocking. This documentation set specifically covers the **VS Code / CMake** example projects.

All three reference boards (FPB-RA2E3, FPB-RA4E2, FPB-RA6E2) are part of this delivery's original scope, not a later addition to a smaller initial release — see [Configuration Guide](#configuration-guide) for the per-board hardware differences already accounted for.

---

### 3. Per-Board Notes

All three boards use the identical module core (`src/rs_bb_uart_tx/`), differing only in the files below:

| Item | FPB-RA2E3 | FPB-RA4E2 | FPB-RA6E2 |
|---|---|---|---|
| Preset config file | `rs_bb_uart_tx_data_fpb_ra2e3.c/.h` | `rs_bb_uart_tx_data_fpb_ra4e2.c/.h` | `rs_bb_uart_tx_data_fpb_ra6e2.c/.h` |
| TX pin | P109 | P411 | P411 |
| GPT clock divider | `TIMER_SOURCE_DIV_1` (undivided) | `TIMER_SOURCE_DIV_4` | `TIMER_SOURCE_DIV_4` |
| GPT bit-period interrupt priority | 1 (hardcoded) | `RS_BB_UART_TX_CFG_GPT_IPL` (6) | `RS_BB_UART_TX_CFG_GPT_IPL` (6) |
| Fault handler register set | `SHCSR` only | `CFSR`/`HFSR`/`MMFAR`/`BFAR` | `CFSR`/`HFSR`/`MMFAR`/`BFAR` |

See [Configuration Guide](#4-gpt-clock-divider--why-it-differs-per-board) for why the GPT divider and interrupt-priority differences are genuine, verified per-board engineering decisions rather than inconsistencies.

---

### 4. Known Issues

- **On-board VCOM baud floor below 9600, confirmed on all three boards.** The GPT signal timing itself is correct down to 1200 baud on every board's TX pin, but the on-board J-Link VCOM bridge's own firmware does not reliably relay baud rates below 9600 end-to-end. This has been hands-on confirmed on FPB-RA2E3, FPB-RA4E2, and FPB-RA6E2 — all three share the same on-board J-Link OB VCOM firmware family and exhibit the same floor. See [Behavior and Performance](#7-on-board-vcom-baud-range-caveat).

---

### 5. Adopting a Newer Release

This module is delivered as reference source code. Once you incorporate and modify it, the modified implementation becomes **your own**. Future updates to the module may require manual review and merge against your copy.

---

### 6. What Typically Changes Between Releases

- Core source files (`rs_bb_uart_tx.c`, `rs_bb_uart_tx_ring_buffer.c`, `rs_bb_uart_tx_engine.c`, `rs_bb_uart_tx_hal.c`) may receive reliability improvements, new features (e.g. a future receive path, or a public lifecycle API), or FSP version compatibility updates.
- Public headers (`rs_bb_uart_tx.h`, `rs_bb_uart_tx_types.h`) may add new public functions or extend existing structures. Additions are generally straightforward to adopt; signature changes or removals are not always accompanied by a `RS_BB_UART_TX_VERSION` bump — always diff the public headers rather than relying on the version macro alone.
- `rs_bb_uart_tx_cfg.h` constants may gain new `#ifndef`-guarded defaults. Existing overrides continue to take precedence without modification.
- Preset config files may be updated for new FSP releases, corrected for hardware errata, or extended to new boards.

---

### 7. Recommended Merge Process

1. Diff the updated core files (`src/rs_bb_uart_tx/`) against your local copies.
2. Apply upstream changes selectively, preserving any application-specific modifications you made in `rs_bb_uart_tx_hal.c` or elsewhere.
3. Review the updated `rs_bb_uart_tx.h` and `rs_bb_uart_tx_types.h` for interface changes; update call sites in your application accordingly.
4. If the FSP version changes between your baseline and the update, re-run the FSP Smart Configurator (or the CMake "Regenerate Project Content" task, or, for the Meson build, the `rasc_generate` run target) to regenerate `ra_gen`/`ra_cfg` content for the new FSP version.
5. Rebuild and re-verify baud-rate accuracy on target hardware, especially if the update touches `rs_bb_uart_tx_hal_gpt_open()`'s period-calculation logic.

Releases do not need to track every minor FSP release automatically. Confirm the supported FSP version in the release notes before upgrading.

---

*End of User Manual — `rs_bb_uart_tx` v1.0.0*
