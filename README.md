# Bit-Banged UART TX — (FSP 6.5.0, LLVM)

Software-driven, transmit-only UART implemented with one GPIO output pin and
GPT timer-based bit-period control. Enables UART-style console/diagnostic
output when a hardware SCI/UART channel is unavailable or reserved for
another function.

Module short name: `bb_uart_tx` — component ID `RS_BB_UART_TX` — see
`src/rs_bb_uart_tx/` for the reusable core.

Phase 1 scope: TX-only, 8N1, LSB-first, 1200-115200 baud (configurable),
non-blocking. No receive, no interactive CLI input. Default demo path uses
the on-board J-Link VCOM — no external adapter needed.

This project was built by integrating the Bit-Banged UART module source into a bare-metal + GPT project (FSP 6.5.0, LLVM Embedded
Toolchain for Arm). The GPT driver
(`ra/fsp/src/r_gpt/r_gpt.c`) is genuinely vendored — no additional Smart
Configurator step is required to build.

---

## Opening The Example Projects

This repository delivers the Bit-Banged UART TX module a VS Code / CMake delivery.

### VS Code / CMake (`examples/VSCode/BitBangedUART_TX_FPB_<board>/`)

A parallel CMake-based project set is available for VS Code, covering all
three reference boards — `examples/VSCode/BitBangedUART_TX_FPB_RA2E3/`,
`examples/VSCode/BitBangedUART_TX_FPB_RA4E2/`, and
`examples/VSCode/BitBangedUART_TX_FPB_RA6E2/`. **VS Code has no e2 studio-style
"Import" step** — you open the project by opening its workspace file
directly:

**Open:** double-click the board's `.code-workspace` file (e.g.
`BitBangedUART_FPB_RA2E3.code-workspace`), or in VS Code use **File → Open
Workspace from File...** and select it. Opening the folder directly
(**File → Open Folder...**) also works, since each board's project is fully
self-contained — there is no multi-board workspace. VS Code's CMake Tools
extension detects `CMakeLists.txt`/`CMakePresets.json` automatically; no
import or project-recognition step is required.

**Build:** select the **"ARM LLVM - Ninja"** CMake kit and a Debug/Release
configure preset, then use the CMake Tools status-bar Build button (or the
equivalent command-line `cmake --preset`/`cmake --build` invocation).

**Flash/Debug:** **Run and Debug → Renesas GDB Debugging**
(`.vscode/launch.json`), targeting the on-board SEGGER J-Link — the same
physical debug path as the e2 studio project above, through VS Code's
debug UI instead of e2 studio's.

Full VS Code / CMake build, tooling, and troubleshooting details — including
per-board device names and every VS Code task — are documented separately in
that delivery's own documentation set: see `doc/BUILD.md` and
`doc/EXAMPLES_GUIDE.md` under `examples/VSCode/`.

## GPT Interrupt Setup

This project's GPT driver source (`ra/fsp/src/r_gpt/r_gpt.c`) **is
included and already configured** — no Smart Configurator step is required
to build or flash. Three things are worth knowing if you ever reopen the
FSP Configurator perspective in e2 studio (double-click
`configuration.xml`), so you don't accidentally revert them:

1. **GPT0 interrupt priority and callback** — the `g_timer0` stack's
   Interrupts tab has the counter-overflow interrupt enabled (priority 1)
   with its callback set to `rs_bb_uart_tx_hal_gpt_callback`. **Note the
   valid priority range on this board is 0 (highest) to 3 (lowest) only**
   — FPB-RA2E3 uses a Cortex-M23 core (only 2 NVIC priority bits
   implemented), unlike FPB-RA4E2/FPB-RA6E2's Cortex-M33 (4 bits, 0-15
   range) — do not try to set priority 6 here, the Configurator won't
   offer it and it would be invalid if forced. If this ever shows as
   "Disabled" or the callback reverts to `NULL`, the Bit-Banged UART's bit
   timing will not run — re-enable it (priority 0-3) and re-set the callback name,
   then **Generate Project Content**.
2. **P109 pin mode** — the Pins tab has P109 set to **GPIO Output (high)**,
   not its default "Peripheral: SCI9 TXD9" — this module bit-bangs the frame
   in software and does not use the SCI9 hardware channel. If you ever need
   SCI9 on this board for something else, remap the Bit-Banged UART TX pin
   to a different GPIO first (see `rs_bb_uart_tx_ConfigGen()`'s `tx_pin`
   argument in `examples/VSCode/BitBangedUART_TX_FPB_<board>/src/rs_bb_uart_tx_demo.c`).
3. **SCI9's Operation Mode is disabled entirely** (not just its pin
   assignments) — `configuration.xml`'s SCI9 entry has no active mode and
   no leftover pairing property at all (unlike SCI0 on FPB-RA4E2/
   FPB-RA6E2, which keeps one inert `sci0.pairing.b` line — SCI9's Simple
   I2C mode here has no separate pairing selector to preserve). This is
   required, not optional: SCI9's default Simple I2C mode claims P109
   (TXD9) and P110 (RXD9) as a 2-pin pairing regardless of what either
   pin's own assignment says, and the Smart Configurator's conflict
   checker will flag P109 as contested with the Bit-Banged UART's GPIO reassignment
   if this mode is re-enabled. If you ever need SCI9 for something else on
   this board, you'll need to both re-enable its Operation Mode AND move
   the Bit-Banged UART TX pin elsewhere (see item 2 above). P110 reverts
   to a plain unconfigured input as a result (see `ra_gen/pin_data.c`) —
   applied proactively here based on the confirmed fix pattern discovered
   on FPB-RA6E2 (same underlying SCI-pairing mechanism, different channel/
   pin numbers).

### GPT Clock Divider

`g_timer0` uses an **undivided PCLKD** (`TIMER_SOURCE_DIV_1`) — different
from FPB-RA4E2/FPB-RA6E2, which both need a PCLKD/4 divider. This is a
correctness decision based on this board's actual clock tree and GPT
capabilities, not a copy-paste of the other boards' setting:
- This board's PCLKD is **48 MHz** (no PLL — HOCO 48 MHz feeds PCLKD
  directly at /1), much lower than the other boards' ~100 MHz PCLKD. At 48
  MHz undivided, even the lowest supported baud rate (1200) needs only
  ~40,000 counts — comfortably within 16-bit range (65,535) with no
  divider needed at all.
- R7FA2E307's GPT channel 0 is additionally a genuine **32-bit** channel
  (`BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0x01`), unlike FPB-RA4E2/
  FPB-RA6E2 (`0x00` — 16-bit only), reinforcing that overflow is not a
  concern here even without a divider.
- Leaving the clock undivided also gives **better** timing resolution than
  the other two boards: ~417 counts at 115200 baud here, versus ~217
  counts on FPB-RA4E2/FPB-RA6E2's divided clock.

See `rs_bb_uart_tx_hal_gpt_open()` in
`src/rs_bb_uart_tx/rs_bb_uart_tx_hal.c` (generic, baud-rate-driven period
calculation — unchanged across all three boards) and the comment above
`g_timer0_extend`/`g_timer0_cfg` in `ra_gen/hal_data.c` for this board's
specific divider choice.

### On-board J-Link VCOM Bridge Does Not Support the Full Baud Range

**Confirmed by hands-on testing on FPB-RA2E3, FPB-RA4E2, and FPB-RA6E2:**
the on-board VCOM (virtual COM port) bridge does not support baud
rates below 9600 on any of the three boards — the Bit-Banged UART module
itself is not the limiting factor there. This
is a constraint of the debug probe's VCOM firmware, not of
`rs_bb_uart_tx`: the GPT divider math (see above) is specifically sized
to generate correct signal timing down to 1200 baud on the TX pin itself;
the limitation is in relaying that signal through the
on-board bridge to a USB CDC serial port your PC can open. All three
boards share the same on-board J-Link OB debug probe VCOM firmware family,
consistent with the same floor applying across all of them.

Practical impact: `rs_bb_uart_tx_ConfigCheck()` will happily accept and
correctly configure any baud rate in the documented 1200-115200 range — but if you're observing output via
Tera Term through the **default on-board VCOM path**, only 9600-115200
will actually be receivable end-to-end that way, on any of the three
boards. To exercise or validate the low end of the range (1200-4800), use
one of:

- **A logic analyzer or oscilloscope directly on the TX pin.** This
  verifies the module's actual bit timing independent of the VCOM bridge
  entirely, and is the only way to observe true 1200 baud signaling with
  this board's on-board probe.
- **An external TTL-to-USB adapter** wired to a different GPIO (see
  "Using a Non-VCOM GPIO / External TTL-to-USB Adapter Instead" below) —
  bypasses the on-board J-Link VCOM bridge entirely, so its own baud-rate
  support (typically down to well below 1200 on common adapters like the
  FTDI TTL-232RG) is what applies instead.

## Running the Demo

Connect the board's USB debug/VCOM connector to your PC. After flashing,
open the enumerated COM port in Tera Term (or equivalent):

| Setting | Value |
|---|---|
| Port | COM port assigned to the on-board VCOM interface |
| Baud rate | 115200 (default) |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |

Expected output:

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
[000005] Status       : queued=0 free=256 tx=187 ok=187 full=0
[000006] Heartbeat OK
...
```

## Using a Non-VCOM GPIO / External TTL-to-USB Adapter Instead

In the example projects, change the `tx_pin` argument passed to `rs_bb_uart_tx_ConfigGen()` in
`examples/VSCode/BitBangedUART_TX_<board>/src/rs_bb_uart_tx_demo.c` to your chosen GPIO, and wire an external
TTL-level USB-to-UART adapter:

| FPB-RA2E3 signal | Adapter signal | Required | Notes |
|---|---|---|---|
| Configured GPIO TX | RXD | Yes | Board transmit output to adapter receive input |
| GND | GND | Yes | Common reference |
| — | TXD | No | Not used — Phase 1 is TX-only |
| — | VCC | Usually no | Only if the adapter needs a target reference voltage; confirm voltage compatibility first |

Use a TTL-level adapter, not a true RS-232 level adapter — never connect the
target GPIO directly to an RS-232 DB9 interface.

---

## Repository Layout

```text
.
├── README.md                          ← This file: high-level overview, quick start, documentation map
├── meson.build                        ← Root Meson project; ties together src/, tests/, firmware/
├── meson_options.txt                  ← Meson build options (gen_board, board, mcu, ra_core, device, unity_tx_pin, ...)
├── metadata.r365.xml                  ← Software Building Block (SBB) release metadata (name, version, supported devices)
├── .gitignore
│
├── docs/
│   ├── BIT_BANGED_UART_TX-USER_MANUAL.md   ← Start here: use case, core capabilities, documentation map
│
├── examples/
│   └── VSCode/
│       ├── BitBangedUART_TX_FPB_RA2E3/    ← VS Code / CMake project for FPB-RA2E3
│       ├── BitBangedUART_TX_FPB_RA4E2/    ← VS Code / CMake project for FPB-RA4E2
│       └── BitBangedUART_TX_FPB_RA6E2/    ← VS Code / CMake project for FPB-RA6E2
│
│
├── src/
│   └── rs_bb_uart_tx/                 ← core source and per-board preset configuration
│       ├── rs_bb_uart_tx.c / .h
│       ├── rs_bb_uart_tx_cfg.h
│       ├── rs_bb_uart_tx_ctrl.h
│       ├── rs_bb_uart_tx_engine.c / .h
│       ├── rs_bb_uart_tx_hal.c / .h
│       ├── rs_bb_uart_tx_ring_buffer.c / .h
│       ├── rs_bb_uart_tx_types.h
│       └── config/
│           └── rs_bb_uart_tx_data_fpb_<board>.c / .h


```

See `doc/FILE_GUIDE.md` under `examples/VSCode/` for the full file-by-file
breakdown of the VS Code / CMake project structure, including which files
are VS Code/CMake-specific.

## Public API

```c
#include <rs_bb_uart_tx/rs_bb_uart_tx.h>
#include <rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.h>

rs_bb_uart_tx_cfg_t    g_cfg;
rs_bb_uart_tx_status_t status;

status = rs_bb_uart_tx_ConfigGen(&g_cfg, &g_timer0_preset, BSP_IO_PORT_01_PIN_09);
status = rs_bb_uart_tx_ConfigCheck(&g_cfg);   /* validates AND opens on success */
if (RS_BB_UART_TX_STATUS_SUCCESS == status)
{
    (void) rs_bb_uart_tx_WriteString("Bit-Banged UART Ready\r\n");
}
```

Six public functions total: `rs_bb_uart_tx_ConfigGetSize()`,
`rs_bb_uart_tx_ConfigGen()`, `rs_bb_uart_tx_ConfigCheck()`,
`rs_bb_uart_tx_Write()`, `rs_bb_uart_tx_WriteString()`,
`rs_bb_uart_tx_StatusGet()`. Every function — public API, internal helpers,
HAL shim, ring buffer, TX engine — carries a complete Doxygen block
(`@brief`, `@param` for every parameter, `@return`/`@retval` for every
outcome) in both its header declaration and its `.c` definition. This
module is a **singleton** — one instance per application (one GPIO pin, one GPT
channel); there is no caller-managed control block to declare or pass
around (see `rs_bb_uart_tx.h`'s top-of-file note for the rationale).

Two configuration paths are available, selected by
`RS_BB_UART_TX_CFG_PRESET_CONFIG_USE` in `rs_bb_uart_tx_cfg.h` (default 1):
- **Preset (default):** binds the standalone `g_timer0_preset` instance
  (`src/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.c`) — has no
  dependency on `ra_gen` matching any particular Configurator state.
- **FSP Stack Configurator:** binds `ra_gen/hal_data.c`'s `g_timer0`
  instead — already fully configured and wired to the same callback in
  this project, so both paths work out of the box.

## Known Limitations (Phase 1, by design)

- TX only. No receive, no interactive CLI, not full-duplex.
- Not ISR-safe: `Write()`/`WriteString()` must be called from startup or
  main-loop/task context, never from an interrupt handler.
- No dynamic reconfiguration while transmitting (baud rate, pin, and timer
  channel are fixed once `ConfigCheck()` succeeds).
- Buffer-full policy is "return error" only — no overwrite-oldest,
  drop-newest, or blocking option in Phase 1.

---

## Next Steps

If you are:

- Evaluating the Bit-Banged UART TX → Start with [`docs/BIT_BANGED_UART_TX-USER_MANUAL.md`](docs/BIT_BANGED_UART_TX-USER_MANUAL.md).