# Configuration Guide

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Configuration Overview](#1-configuration-overview)
2. [Configuration Assumptions Per Reference Board](#2-configuration-assumptions-per-reference-board)
3. [Required Non-Default FSP Settings](#3-required-non-default-fsp-settings)
4. [GPT Clock Divider — Why It Differs Per Board](#4-gpt-clock-divider--why-it-differs-per-board)
5. [Adapting to Your Board](#5-adapting-to-your-board)
6. [Using an External TTL-to-USB Adapter](#6-using-an-external-ttl-to-usb-adapter)
7. [Key Adaptation Points](#7-key-adaptation-points)
8. [Dependencies](#8-dependencies)

---

## 1. Configuration Overview

`rs_bb_uart_tx` relies on a combination of:

- A GPT HAL instance (`timer_instance_t`), either a standalone preset or FSP Stack Configurator-generated
- Build-time configuration constants in `rs_bb_uart_tx_cfg.h`
- A runtime configuration structure (`rs_bb_uart_tx_cfg_t`) bound via `rs_bb_uart_tx_ConfigGen()`

The reference implementation targets **FPB-RA2E3**, **FPB-RA4E2**, and **FPB-RA6E2** — all three via VS Code / CMake example projects in this delivery.

---

## 2. Configuration Assumptions Per Reference Board

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

## 3. Required Non-Default FSP Settings

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

## 4. GPT Clock Divider — Why It Differs Per Board

FPB-RA2E3 uses an **undivided** PCLKD (`TIMER_SOURCE_DIV_1`); FPB-RA4E2 and FPB-RA6E2 both need a **PCLKD/4** divider (`TIMER_SOURCE_DIV_4`). This is a genuine per-board engineering decision, not a copy-paste inconsistency:

- **FPB-RA2E3's clock tree has no PLL** — HOCO (48 MHz) feeds PCLKD directly at ÷1, giving PCLKD = 48 MHz. At 48 MHz undivided, even the lowest supported baud rate (1200) needs only ~40,000 counts — comfortably within 16-bit range (65,535) with no divider needed at all.
- **FPB-RA2E3's GPT channel 0 is additionally a genuine 32-bit channel** (`BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0x01`), unlike FPB-RA4E2/FPB-RA6E2 (`0x00` — every GPT channel is 16-bit only), reinforcing that overflow is not a concern on FPB-RA2E3 even without a divider.
- **FPB-RA4E2/FPB-RA6E2's PCLKD runs at roughly 100 MHz undivided.** At 1200 baud, an undivided ~100 MHz clock would need ~83,333 counts — this overflows their 16-bit-only GPT counters. The PCLKD/4 divider brings the effective rate down to ~25 MHz, keeping period_counts within 16-bit range across the full 1200–115200 baud range.
- **Leaving FPB-RA2E3's clock undivided also gives better timing resolution** than the other two boards: ~417 counts at 115200 baud vs. ~217 counts on the PCLKD/4-divided boards.

FPB-RA6E2's clock tree (HOCO 20 MHz × PLL 10.0 = 200 MHz system clock, PCLKD ÷2 ≈ 100 MHz) produces effectively the same PCLKD as FPB-RA4E2, so the same ÷4 divider value applies unchanged on both boards.

---

## 5. Adapting to Your Board

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

## 6. Using an External TTL-to-USB Adapter

To bypass the on-board VCOM bridge entirely (see [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md#7-on-board-vcom-baud-range-caveat)), change the `tx_pin` argument passed to `rs_bb_uart_tx_ConfigGen()` in `rs_bb_uart_tx_demo.c` to your chosen GPIO, and wire an external TTL-level USB-to-UART adapter:

| Board signal | Adapter signal | Required | Notes |
|---|---|---|---|
| Configured GPIO TX | RXD | Yes | Board transmit output → adapter receive input |
| GND | GND | Yes | Common reference |
| — | TXD | No | Not used — Phase 1 is TX-only |
| — | VCC | Usually no | Only if the adapter needs a target reference voltage; confirm voltage compatibility first |

**Use a TTL-level adapter, not a true RS-232 level adapter** — never connect the target GPIO directly to an RS-232 DB9 interface.

---

## 7. Key Adaptation Points

| Item | Location | Effect |
|---|---|---|
| TX pin | `tx_pin` argument to `rs_bb_uart_tx_ConfigGen()` | Change to any available GPIO; confirm it isn't claimed by a default peripheral pairing first |
| GPT instance | `p_gpt_instance` argument to `rs_bb_uart_tx_ConfigGen()` | Preset (`g_timer0_preset`) or FSP Stack Configurator (`g_timer0`); switch via `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE` |
| Baud rate | `rs_bb_uart_tx_cfg_t.baud_rate`, set after `ConfigGen()` and before `ConfigCheck()` | Any value in `[RS_BB_UART_TX_CFG_MIN_BAUD_RATE, RS_BB_UART_TX_CFG_MAX_BAUD_RATE]` (default 1200–115200) |
| Default baud rate | `RS_BB_UART_TX_CFG_DEFAULT_BAUD_RATE` in `rs_bb_uart_tx_cfg.h` | Default 115200 |
| Ring buffer size | `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` in `rs_bb_uart_tx_cfg.h` | Must remain a power of two (64/128/256/512/1024 documented references); default 512 — see [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md#5-ram-consumption) for why |
| GPT bit-period interrupt priority | `RS_BB_UART_TX_CFG_GPT_IPL` in `rs_bb_uart_tx_cfg.h` | Default 6; **not applicable on FPB-RA2E3**, whose board config file hardcodes 1 instead (Cortex-M23 priority range is 0–3) |
| Control-block RAM budget guard | `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` in `rs_bb_uart_tx_cfg.h` | Default 2048 bytes; build fails via `_Static_assert` if exceeded |
| Parameter checking | `RS_BB_UART_TX_CFG_PARAM_CHECKING_ENABLE` in `rs_bb_uart_tx_cfg.h` | Mirrors `BSP_CFG_PARAM_CHECKING_ENABLE` when defined; defaults to enabled (1) otherwise |
| Configuration path | `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE` in `rs_bb_uart_tx_cfg.h` | `1` = preset (default), `0` = FSP Stack Configurator |
| Fault handler | `src/fault_handler.c` | Replace with your production fault handling; remember the Cortex-M23 vs. Cortex-M33 register-set difference if copying across boards — see [FILE_GUIDE.md](FILE_GUIDE.md) |

---

## 8. Dependencies

### Hardware Dependencies

| Resource | Usage | Owned By |
|---|---|---|
| GPT channel 0 (`r_gpt`) | Bit-period interrupt generation | This module |
| TX GPIO pin (P109 on FPB-RA2E3; P411 on FPB-RA4E2/FPB-RA6E2) | Software-driven UART TX output, direct BSP pin control | This module |
| On-board J-Link VCOM (USB) | Default demonstration transport (TTL-to-USB conversion) | Board hardware |

### Software Dependencies

| Component | Version | Notes |
|---|---|---|
| Renesas FSP | v6.5.0 | Provides `r_gpt`, `r_ioport` types, BSP, startup |
| LLVM Embedded Toolchain for Arm (ATfE) | Bundled with e2studio's FSP v6.5.0 toolchain bundle | `clang`/`clang++` targeting `arm-none-eabi`, referenced by `cmake/llvm.cmake` |
| RASC (standalone) | v6.5.0-compatible | Required for headless project-content regeneration from the CMake configure step — see [BUILD.md](BUILD.md) |
| SEGGER J-Link (on-board) | — | Debug/flash on all three FPB boards |

### Driver Dependency Details

This module references the GPT HAL driver exclusively through the FSP abstract interface type `timer_instance_t *`. This means the GPT-facing core source (`rs_bb_uart_tx_hal.c`) is written once and used unchanged on all three boards — board-specific differences are confined entirely to each board's `rs_bb_uart_tx_data_fpb_<board>.c/.h` preset file (or `configuration.xml`, on the FSP Stack Configurator path).

This module has **no dependency on any other `rs_` module.** It does not open, own, or depend on a UART/SCI HAL instance at all — the whole point of the module is to synthesize UART-style output without one.

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
