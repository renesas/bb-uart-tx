# File Guide — Naming Conventions and What Is Safe to Modify

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Core Code vs. Reference Application Code](#1-core-code-vs-reference-application-code)
2. [Naming Conventions Used in This Module](#2-naming-conventions-used-in-this-module)
3. [Core Files — Copy and Keep; Modify with Care](#3-core-files--copy-and-keep-modify-with-care)
4. [Preset Configuration Files — Reference Only; Do Not Edit](#4-preset-configuration-files--reference-only-do-not-edit)
5. [Reference Application — Adapt Freely](#5-reference-application--adapt-freely)
6. [FSP-Generated / Vendored Files — Do Not Edit Manually](#6-fsp-generated--vendored-files--do-not-edit-manually)
7. [VS Code / CMake Project Files](#7-vs-code--cmake-project-files)

---

## 1. Core Code vs. Reference Application Code

The following table identifies which files are reusable core code and which belong only to the reference/demo application layer. The configuration path column refers to the two options explained fully in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board): the **Preset path** (default, uses a standalone pre-built GPT instance) and the **FSP Stack Configurator path** (for custom boards or modified GPT settings).

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
| **FSP Generated / Vendored** | `ra/`, `ra_gen/` directories | FSP-generated configuration, drivers, middleware, CMSIS, board support. `ra/fsp/src/r_gpt/r_gpt.c` and `ra/fsp/src/r_ioport/` are genuinely vendored in every board project in this delivery — no additional Smart Configurator step is required to build | Do not edit manually; regenerate via the FSP Smart Configurator, the CMake pre-configure RASC step (see [BUILD.md](BUILD.md)), or the Meson build's `rasc_generate` run target (see [BUILD_MESON.md](BUILD_MESON.md)) if you change `configuration.xml` |

**Key separation:** the demo's banner text, heartbeat cadence, and status-line formatting in `rs_bb_uart_tx_demo.c` are entirely reference-application code and are not part of the reusable module. The module core in `src/rs_bb_uart_tx/` has no knowledge of what an application writes to it — it only knows how to move bytes onto a GPIO pin at the configured baud rate.

---

## 2. Naming Conventions Used in This Module

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

## 3. Core Files — Copy and Keep; Modify with Care

These files contain reusable reference code. Copy them into your project. Modify only at the documented adaptation points. If you later adopt a newer release of this module, these files will require a merge review.

| File | Safe to Modify? | Notes |
|---|---|---|
| `rs_bb_uart_tx.c` | With care | Core orchestration and lifecycle logic; the internal `Open()`/`Start()` helpers are not exposed publicly by design (see [BIT_BANGED_UART_TX-USER_MANUAL.md](BIT_BANGED_UART_TX-USER_MANUAL.md#21-design-pattern-single-call-simple-use)) |
| `rs_bb_uart_tx.h` | No | Public interface; adding or removing functions breaks compatibility |
| `rs_bb_uart_tx_types.h` | No | Public types; changing structures may break compatibility with any code that reads `rs_bb_uart_tx_cfg_t`/`rs_bb_uart_tx_state_t` fields |
| `rs_bb_uart_tx_ctrl.h` | No | Internal control-block layout; the GPT ISR callback is wired to the one static instance via `p_context` at open time — do not declare additional instances |
| `rs_bb_uart_tx_cfg.h` | **Yes** | Override any `#ifndef`-guarded constant with your project-specific value |
| `rs_bb_uart_tx_ring_buffer.c/.h` | With care | Requires `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` to remain a power of two (enforced by `_Static_assert`) — the lock-free wraparound uses a bitmask, not `%` |
| `rs_bb_uart_tx_engine.c/.h` | With care | Hardware-agnostic bit-level state machine; portable and host-unit-testable in isolation since it never includes FSP headers |
| `rs_bb_uart_tx_hal.c/.h` | With care | The one file that touches FSP GPT/GPIO types directly — modify only if switching timer peripheral or pin-control mechanism |

---

## 4. Preset Configuration Files — Reference Only; Do Not Edit

These files are pre-built for FPB-RA2E3, FPB-RA4E2, and FPB-RA6E2 and are marked "do not edit." Each is a reference-quality, standalone GPT instance definition for that board — not an alias of the FSP Stack Configurator's own `ra_gen/hal_data.c` instance, though both are wired in and either can be used (see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board), `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE`).

| File | Notes |
|---|---|
| `config/rs_bb_uart_tx_data_fpb_ra2e3.c/.h` | Preset GPT ch0 instance for FPB-RA2E3: undivided PCLKD (48 MHz), TX pin P109, `cycle_end_ipl = 1` (hardcoded — Cortex-M23 supports only priorities 0–3). Also defines `rs_bb_uart_tx_ConfigGen()`/`ConfigCheck()`; do not edit |
| `config/rs_bb_uart_tx_data_fpb_ra4e2.c/.h` | Preset GPT ch0 instance for FPB-RA4E2: PCLKD/4 divider, TX pin P411, `cycle_end_ipl = RS_BB_UART_TX_CFG_GPT_IPL` (default 6). Also defines `rs_bb_uart_tx_ConfigGen()`/`ConfigCheck()`; do not edit |
| `config/rs_bb_uart_tx_data_fpb_ra6e2.c/.h` | Preset GPT ch0 instance for FPB-RA6E2: PCLKD/4 divider, TX pin P411, `cycle_end_ipl = RS_BB_UART_TX_CFG_GPT_IPL` (default 6). Also defines `rs_bb_uart_tx_ConfigGen()`/`ConfigCheck()`; do not edit |

To customize the GPT configuration for a different board, switch to the FSP Stack Configurator path as described in [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#5-adapting-to-your-board) (`RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 0`), or add a new `rs_bb_uart_tx_data_fpb_<board>.c/.h` pair following the existing pattern.

---

## 5. Reference Application — Adapt Freely

These files demonstrate the module but are not intended for direct reuse. They contain board-specific and demo-specific scaffolding. Use them as a starting point and replace what you don't need.

| File | Adaptation Guidance |
|---|---|
| `rs_bb_uart_tx_demo.c` | Starting point for your own application entry point. One copy per board project; differs only in the board name string in the banner, the included board config header, and the `tx_pin` argument passed to `rs_bb_uart_tx_ConfigGen()` — see [FUNCTIONS_GUIDE.md](FUNCTIONS_GUIDE.md) |
| `hal_entry.c` | FSP application hook; adapt if your application needs a different entry sequence |
| `hal_warmstart.c` | Warm-start detection helper; adapt if your system uses a different reset-handling strategy |
| `fault_handler.c` | Debug fault handler; replace with your production fault handling. Remember this file's content genuinely differs between the Cortex-M23 board (FPB-RA2E3) and the two Cortex-M33 boards (FPB-RA4E2/FPB-RA6E2) — do not copy the wrong variant across cores |

---

## 6. FSP-Generated / Vendored Files — Do Not Edit Manually

| Directory | Notes |
|---|---|
| `ra/` | FSP-generated drivers, middleware, CMSIS, and board support, plus the genuinely vendored `r_gpt` (GPT driver source) and `r_ioport` (pin-configuration support types). No additional Smart Configurator step is required to build any of the three board projects as delivered |
| `ra_gen/` | Auto-generated by the FSP Smart Configurator from `configuration.xml`; overwritten on every re-generation. Includes the vector table wiring for the GPT counter-overflow interrupt (`ra_gen/vector_data.h/.c`) that the preset GPT instance's ISR depends on |
| `ra_cfg/` | Build-time FSP/BSP/driver configuration headers generated alongside `ra_gen/` |

If you ever reopen the FSP Smart Configurator perspective for one of these projects (via the `FSP Smart Configurator` VS Code task, or by double-clicking `configuration.xml`), see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#3-required-non-default-fsp-settings) for the settings that must be preserved per board so you don't accidentally revert them.

---

## 7. VS Code / CMake Project Files

Files specific to the VS Code / CMake delivery mechanism (not present, or present in a different form, in an e2studio-based delivery of this module):

| File | Purpose | Safe to Modify? |
|---|---|---|
| `CMakeLists.txt` | Top-level CMake project definition; includes `Config.cmake` and the RASC-generated `cmake/GeneratedCfg.cmake` / `cmake/GeneratedSrc.cmake` | With care — see [BUILD.md](BUILD.md) |
| `CMakePresets.json` | Debug/Release configure and build presets (Ninja generator) | Yes — add presets as needed |
| `CMakeProject.json` | Device/core/toolchain metadata consumed by the Renesas VS Code extensions (device name, entry symbol, include/source paths) | No — regenerated by tooling; treat as generated |
| `Config.cmake` | User-editable hook for `RASC_EXE_PATH` resolution | Yes — set `RASC_EXE_PATH` here or via environment/CLI |
| `cmake/GeneratedCfg.cmake`, `cmake/GeneratedSrc.cmake` | RASC-generated compiler flags and source-file globs (`ra_gen/`, `src/`, `ra/`) | No — regenerated by RASC; edit `CMakeLists.txt` to override instead |
| `cmake/llvm.cmake` | ATfE (LLVM Embedded Toolchain for Arm) toolchain file; resolves `ARM_TOOLCHAIN_PATH` | No — regenerated by RASC |
| `cmake/prebuild.cmake` | Backing script for the "Regenerate Project Content" VS Code task | No — regenerated by RASC |
| `.vscode/cmake-kits.json`, `.vscode/tasks.json`, `.vscode/launch.json`, `.vscode/c_cpp_properties.json`, `.vscode/settings.json` | VS Code CMake Tools kit definitions, build/RASC tasks, hardware debug launch configuration, IntelliSense paths | Yes — adjust for your local toolchain paths and workflow; see [BUILD.md](BUILD.md) |
| `*.code-workspace` | VS Code multi-root workspace definition (single-folder here). **This is how you open the project** — double-click it, or use VS Code's File → Open Workspace from File... There is no e2studio-style import step | Yes |
| `*.jlink`, `JLinkLog.log` | SEGGER J-Link debugger configuration / log | `.jlink` yes if adapting debug probe settings; `JLinkLog.log` is a log artifact, not source |
| `.cproject`, `.project`, `.settings/`, `.api_xml`, `.sdm_json`, `.secure_*`, `.r365project` | e2studio/RASC/Renesas-365 project metadata carried alongside the CMake project for tooling compatibility | No — tooling-managed |

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
