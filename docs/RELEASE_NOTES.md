# Release Notes and Adopting a Newer Release

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).

## Table of Contents

1. [Current Version](#1-current-version)
2. [Delivery Scope](#2-delivery-scope)
3. [Per-Board Notes](#3-per-board-notes)
4. [Known Issues](#4-known-issues)
5. [Adopting a Newer Release](#5-adopting-a-newer-release)
6. [What Typically Changes Between Releases](#6-what-typically-changes-between-releases)
7. [Recommended Merge Process](#7-recommended-merge-process)

---

## 1. Current Version

| Field | Value |
|---|---|
| Version | 0.1.0 |
| FSP Baseline | v6.5.0 |
| Toolchain | LLVM Embedded Toolchain for Arm (ATfE) |
| Build System (this delivery) | CMake + Ninja + VS Code |
| Reference Targets | FPB-RA2E3, FPB-RA4E2, FPB-RA6E2 |

There is no version-query public function in Phase 1 (unlike some other reference modules that expose a `VersionGet()` call) — the version is available at compile time via the `RS_BB_UART_TX_VERSION`, `RS_BB_UART_TX_VERSION_MAJOR`, `RS_BB_UART_TX_VERSION_MINOR`, and `RS_BB_UART_TX_VERSION_PATCH` macros in `rs_bb_uart_tx.h`, and at runtime via `rs_bb_uart_tx_StatusGet()`'s `version` field (`rs_bb_uart_tx_state_t.version`).

---

## 2. Delivery Scope

This is the **Phase 1** delivery: TX-only, 8N1, LSB-first, 1200–115200 baud, non-blocking. This documentation set specifically covers the **VS Code / CMake** example projects. A parallel e2studio-based delivery of the same module core also exists for all three boards (see the repository root `README.md`); the module core itself (`src/rs_bb_uart_tx/`) is identical between the two delivery mechanisms — only the build tooling differs.

All three reference boards (FPB-RA2E3, FPB-RA4E2, FPB-RA6E2) are part of this delivery's original scope, not a later addition to a smaller initial release — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md) for the per-board hardware differences already accounted for.

---

## 3. Per-Board Notes

All three boards use the identical module core (`src/rs_bb_uart_tx/`), differing only in the files below:

| Item | FPB-RA2E3 | FPB-RA4E2 | FPB-RA6E2 |
|---|---|---|---|
| Preset config file | `rs_bb_uart_tx_data_fpb_ra2e3.c/.h` | `rs_bb_uart_tx_data_fpb_ra4e2.c/.h` | `rs_bb_uart_tx_data_fpb_ra6e2.c/.h` |
| TX pin | P109 | P411 | P411 |
| GPT clock divider | `TIMER_SOURCE_DIV_1` (undivided) | `TIMER_SOURCE_DIV_4` | `TIMER_SOURCE_DIV_4` |
| GPT bit-period interrupt priority | 1 (hardcoded) | `RS_BB_UART_TX_CFG_GPT_IPL` (6) | `RS_BB_UART_TX_CFG_GPT_IPL` (6) |
| Fault handler register set | `SHCSR` only | `CFSR`/`HFSR`/`MMFAR`/`BFAR` | `CFSR`/`HFSR`/`MMFAR`/`BFAR` |

See [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#4-gpt-clock-divider--why-it-differs-per-board) for why the GPT divider and interrupt-priority differences are genuine, verified per-board engineering decisions rather than inconsistencies.

---

## 4. Known Issues

- **On-board VCOM baud floor below 9600, confirmed on all three boards.** The GPT signal timing itself is correct down to 1200 baud on every board's TX pin, but the on-board J-Link VCOM bridge's own firmware does not reliably relay baud rates below 9600 end-to-end. This has been hands-on confirmed on FPB-RA2E3, FPB-RA4E2, and FPB-RA6E2 — all three share the same on-board J-Link OB VCOM firmware family and exhibit the same floor. See [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md#7-on-board-vcom-baud-range-caveat).

---

## 5. Adopting a Newer Release

This module is delivered as reference source code. Once you incorporate and modify it, the modified implementation becomes **your own**. Future updates to the module may require manual review and merge against your copy.

---

## 6. What Typically Changes Between Releases

- Core source files (`rs_bb_uart_tx.c`, `rs_bb_uart_tx_ring_buffer.c`, `rs_bb_uart_tx_engine.c`, `rs_bb_uart_tx_hal.c`) may receive reliability improvements, new features (e.g. a future receive path, or a public lifecycle API), or FSP version compatibility updates.
- Public headers (`rs_bb_uart_tx.h`, `rs_bb_uart_tx_types.h`) may add new public functions or extend existing structures. Additions are generally straightforward to adopt; signature changes or removals are not always accompanied by a `RS_BB_UART_TX_VERSION` bump — always diff the public headers rather than relying on the version macro alone.
- `rs_bb_uart_tx_cfg.h` constants may gain new `#ifndef`-guarded defaults. Existing overrides continue to take precedence without modification.
- Preset config files may be updated for new FSP releases, corrected for hardware errata, or extended to new boards.

---

## 7. Recommended Merge Process

1. Diff the updated core files (`src/rs_bb_uart_tx/`) against your local copies.
2. Apply upstream changes selectively, preserving any application-specific modifications you made in `rs_bb_uart_tx_hal.c` or elsewhere.
3. Review the updated `rs_bb_uart_tx.h` and `rs_bb_uart_tx_types.h` for interface changes; update call sites in your application accordingly.
4. If the FSP version changes between your baseline and the update, re-run the FSP Smart Configurator (or the CMake "Regenerate Project Content" task — see [BUILD.md](BUILD.md#-regenerating-fsp-project-content)) to regenerate `ra_gen`/`ra_cfg` content for the new FSP version.
5. Rebuild and re-verify baud-rate accuracy on target hardware, especially if the update touches `rs_bb_uart_tx_hal_gpt_open()`'s period-calculation logic.

Releases do not need to track every minor FSP release automatically. Confirm the supported FSP version in the release notes before upgrading.

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
