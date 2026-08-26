# Bit-Banged UART TX — FPB-RA2E3 (e2 studio, FSP 6.5.0, LLVM)

Software-driven, transmit-only UART implemented with one GPIO output pin and
GPT timer-based bit-period control. Enables UART-style console/diagnostic
output when a hardware SCI/UART channel is unavailable or reserved for
another function.

PES short name: `bb_uart_tx` — component ID `RS_BB_UART_TX` — see
`src/rs_bb_uart_tx/` for the reusable core, `src/rs_bb_uart_tx_demo.c` +
`src/hal_entry.c` for the reference application.

Phase 1 scope: TX-only, 8N1, LSB-first, 1200-115200 baud (configurable),
non-blocking. No receive, no interactive CLI input. Default demo path uses
the on-board J-Link VCOM (P109) — no external adapter needed.

Full requirements/architecture: see the accepted MRD/acceptance document and
`PES_Bit-BangedUART_Acceptance_ArchDesign_v03` (Architecture & Design v0.3).
This project is the Phase 3 "Develop & Test" deliverable for that design.

This project was built by integrating the PES source into a real
e2 studio-generated bare-metal + GPT project (FSP 6.5.0, LLVM Embedded
Toolchain for Arm) for **R7FA2E3073CFL**. The GPT driver
(`ra/fsp/src/r_gpt/r_gpt.c`) is genuinely vendored — no additional Smart
Configurator step is required to build.

Ported from the FPB-RA4E2/FPB-RA6E2 deliveries of this same PES. Every
hardware fact this port depends on was independently re-verified against
this board's own reference project — several genuinely differ from the
other two boards, not just names:
- **PCLKD is 48 MHz here** (no PLL; HOCO 48 MHz feeds PCLKD directly at
  /1), not the ~100 MHz PCLKD on FPB-RA4E2/FPB-RA6E2 (both HOCO x PLL 10.0
  /2).
- **GPT channel 0 is a genuine 32-bit channel** on this device
  (`BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0x01`), unlike FPB-RA4E2/
  FPB-RA6E2 (both `0x00`, 16-bit only).
- **The default VCOM-adjacent TX pin's peripheral pairing is SCI9 Simple
  I2C mode** (2-pin pairing: P109 TXD9 + P110 RXD9), not SCI0 Simple SPI
  mode (3-pin pairing) the way P411 was paired on the other two boards.
- **The GPT overflow ELC event ID differs** (`0x04A` here vs `0x0C6` on
  the other boards) — harmless, since the code references it symbolically
  (`ELC_EVENT_GPT0_COUNTER_OVERFLOW`), not by hardcoded number.
- **LED1 is P213** (`BSP_IO_PORT_02_PIN_13`), not P207.

See `PhasePlan.md` and `docs/README.md` for the full comparison and the
reasoning behind each adapted value (in particular, why this board's GPT
uses an **undivided** clock while the other two need a /4 divider).

---

## Opening This Project

**Import into e2 studio:** File → Import → General → Existing Projects into
Workspace → select this folder. e2 studio will recognize `.project`/
`.cproject` and set up the FSP/RA views automatically.

**Build:** Project → Build Project (or the hammer icon). Default build
configuration is Debug, LLVM Embedded Toolchain for Arm.

**Flash/Debug:** Use the included debug launch configuration
(`BitBangedUART_TX_FPB_RA2E3 Debug_Flat.launch`) — Run → Debug Configurations
→ select it → Debug. This targets the on-board J-Link debugger via SEGGER
J-Link, consistent with the `.jlink` script also included in this folder.

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
   "Disabled" or the callback reverts to `NULL`, the PES's bit timing will
   not run — re-enable it (priority 0-3) and re-set the callback name,
   then **Generate Project Content**.
2. **P109 pin mode** — the Pins tab has P109 set to **GPIO Output (high)**,
   not its default "Peripheral: SCI9 TXD9" — this PES bit-bangs the frame
   in software and does not use the SCI9 hardware channel. If you ever need
   SCI9 on this board for something else, remap the Bit-Banged UART TX pin
   to a different GPIO first (see `rs_bb_uart_tx_ConfigGen()`'s `tx_pin`
   argument in `src/rs_bb_uart_tx_demo.c`).
3. **SCI9's Operation Mode is disabled entirely** (not just its pin
   assignments) — `configuration.xml`'s SCI9 entry has no active mode and
   no leftover pairing property at all (unlike SCI0 on FPB-RA4E2/
   FPB-RA6E2, which keeps one inert `sci0.pairing.b` line — SCI9's Simple
   I2C mode here has no separate pairing selector to preserve). This is
   required, not optional: SCI9's default Simple I2C mode claims P109
   (TXD9) and P110 (RXD9) as a 2-pin pairing regardless of what either
   pin's own assignment says, and the Smart Configurator's conflict
   checker will flag P109 as contested with the PES's GPIO reassignment
   if this mode is re-enabled. If you ever need SCI9 for something else on
   this board, you'll need to both re-enable its Operation Mode AND move
   the Bit-Banged UART TX pin elsewhere (see item 2 above). P110 reverts
   to a plain unconfigured input as a result (see `ra_gen/pin_data.c`) —
   applied proactively here based on the confirmed fix pattern discovered
   on FPB-RA6E2 (same underlying SCI-pairing mechanism, different channel/
   pin numbers), not yet independently re-confirmed via a fresh RASC
   regeneration on this specific board.

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

### On-board J-Link VCOM Bridge May Not Support the Full Baud Range

**Confirmed by hands-on testing on FPB-RA4E2** (not yet independently
re-verified on physical FPB-RA2E3 hardware, but flagged here proactively
since both boards use the same on-board J-Link OB debug probe family):
on FPB-RA4E2, the VCOM (virtual COM port) bridge does not support baud
rates below 9600 — the PES itself is not the limiting factor there. This
is a constraint of the debug probe's VCOM firmware, not of
`rs_bb_uart_tx`: the GPT divider math (see above) is specifically sized
to generate correct signal timing down to 1200 baud on the TX pin itself;
the limitation, where it applies, is in relaying that signal through the
on-board bridge to a USB CDC serial port your PC can open. If you test
the low end of the baud range (1200-4800) on this board, please report
back whether the same 9600 floor applies here too, so this note can be
updated from "expected" to "confirmed."

Practical impact: `rs_bb_uart_tx_ConfigCheck()` will happily accept and
correctly configure any baud rate in the documented 1200-115200 range
(Architecture & Design Section 5.4) — but if you're observing output via
Tera Term through the **default on-board VCOM path**, only 9600-115200
will actually be receivable end-to-end that way. To exercise or validate
the low end of the range (1200-4800), use one of:

- **A logic analyzer or oscilloscope directly on the TX pin.** This
  verifies the PES's actual bit timing independent of the VCOM bridge
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
Project      : e2 studio bare-metal example
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

Change the `tx_pin` argument passed to `rs_bb_uart_tx_ConfigGen()` in
`src/rs_bb_uart_tx_demo.c` to your chosen GPIO, and wire an external
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

```
BitBangedUART_TX_FPB_RA2E3/
  .project, .cproject, .settings/        (Eclipse CDT / e2 studio project files)
  .clangd                                 (LLVM tooling config for IDE code intelligence)
  BitBangedUART_TX_FPB_RA2E3 Debug_Flat.jlink/.launch  (debug/flash configs)
  README.md                               (this file)
  PhasePlan.md                            (implementation checkpoint / progress log)
  configuration.xml                       (FSP Smart Configurator project — GPT + GPIO configured)
  script/fsp.lld, memory_regions.lld, bsp_linker_info.h
  ra/                                     (vendored FSP: bsp, arm/CMSIS, board, r_ioport, r_gpt)
  ra_cfg/fsp_cfg/                         (bsp + r_ioport + r_gpt build-time config)
  ra_gen/                                 (generated: pin/common/hal data, GPT-enabled vector
                                            table, bare-metal main.c)
  src/
    hal_entry.c                           (bare-metal application entry point)
    hal_warmstart.c                       (Renesas-provided BSP warm-start boilerplate, unmodified)
    fault_handler.c                       (Cortex-M23 fault handler, adapted from the
                                             FPB-RA4E2/FPB-RA6E2 Cortex-M33 version — see
                                             its top-of-file comment)
    rs_bb_uart_tx_demo.c                  (reference application — NOT reusable PES code)
    rs_bb_uart_tx/                        (*** the reusable PES core ***)
      rs_bb_uart_tx.h                     public API (6 functions — full Doxygen docs)
      rs_bb_uart_tx.c                     core orchestration + internal lifecycle
      rs_bb_uart_tx_types.h               public types (cfg_t, state_t, status_t enum)
      rs_bb_uart_tx_ctrl.h                internal-only control block (singleton)
      rs_bb_uart_tx_cfg.h                 build-time configuration macros
      rs_bb_uart_tx_ring_buffer.h/.c      ring buffer manager
      rs_bb_uart_tx_engine.h/.c           TX bit-level state machine
      rs_bb_uart_tx_hal.h/.c              GPT + GPIO target-adaptation shim, ISR
      config/
        rs_bb_uart_tx_data_fpb_ra2e3.h/.c   board preset + ConfigGen/ConfigCheck
```

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
outcome) in both its header declaration and its `.c` definition. This PES
is a **singleton** — one instance per application (one GPIO pin, one GPT
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

## Troubleshooting

### Garbled/corrupted characters at startup (banner), but clean output from `[000001] Heartbeat OK` onward

**Confirmed on FPB-RA2E3 via real hardware testing — root cause found and
fixed, not just worked around.** An earlier version of this note
attributed this to USB VCOM host-attach timing; that theory was tested
(a 1-second startup delay) and **did not fix it**, which led to the real
root cause below.

**Root cause:** `rs_bb_uart_tx_ring_buffer_enqueue()`'s critical section
holds interrupts disabled for the *entire* `memcpy()` of whatever length
is being enqueued — its duration is not a fixed handful of instructions,
it scales with request size. The original demo banner made ~10 rapid,
back-to-back `WriteString()` calls: the first one starts the GPT timer,
and every call after it enqueues more data *while that timer is already
running*. On this board's Cortex-M23 CPU (48 MHz — more than 2x slower
than FPB-RA4E2/FPB-RA6E2's Cortex-M33 @ 100 MHz), a single `memcpy()` of
a few dozen bytes can take long enough in real time to be comparable to
one bit period at 115200 baud (~8.68 µs), delaying the GPT ISR enough to
corrupt whichever bit was due during that window. Confirmed via a
targeted experiment: a disposable "warm-up" string sent *before* the real
banner came through perfectly clean, while the real banner (the next
transmission, made of many rapid calls) was still corrupted — ruling out
"first transmission ever" as the cause and pointing specifically at the
rapid-multi-call pattern.

**Fix applied in this delivery:** `demo_print_banner()` in
`src/rs_bb_uart_tx_demo.c` now builds the entire banner into one buffer
and sends it via a single `WriteString()` call instead of ~10 separate
ones — eliminating the "later call lands while an earlier one is already
transmitting" pattern that triggered this. The 1-second startup delay
from the earlier (incorrect) theory is left in place as a harmless,
possibly still marginally useful safety margin, but it was not the actual
fix.

**Broader implication, not just a demo bug:** this is a genuine
characteristic of the PES core, not something specific to the demo
application — see `src/rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.c`'s
file-level comment and `docs/README.md` for the full write-up. A real
application making large or frequent `Write()`/`WriteString()` calls,
especially while a previous transmission may still be in progress on a
slower CPU or at a high baud rate, should be aware of this.

### `full=N` (non-zero) in the demo's status line

`rs_bb_uart_tx_StatusGet()`'s `buffer_full_count` — shown as `full=N` in
the demo's periodic status line — counts `Write()`/`WriteString()` calls
that were rejected because the request didn't fit in the ring buffer's
free space at that moment (all-or-nothing enqueue, Architecture & Design
Section 13.3 — nothing partial is ever written). This is silent unless
checked; the demo ignores the return value on purpose to keep the example
simple.

**Fix:** increase the buffer size in `src/rs_bb_uart_tx/rs_bb_uart_tx_cfg.h`:
```c
#ifndef RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES
  #define RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES     (512U)   // was 256U
#endif
```
Validated reference sizes (Architecture & Design Section 5.7): 64, 128,
256, 512, 1024 bytes. Rebuild after changing — a `_Static_assert` in
`rs_bb_uart_tx.c` will fail the build with a clear message rather than
silently overrun RAM if the chosen size doesn't fit
`RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` (2048 bytes by default).

**Note:** a bigger buffer only absorbs *bursts* (e.g. several
back-to-back `WriteString()` calls before the timer has drained any of
them — this is exactly what the demo's startup banner does). It does not
fix a *sustained* rate mismatch — if data is enqueued faster, on average,
than the configured baud rate can drain it, `full` will keep climbing
regardless of buffer size. In that case, lower the data rate, raise the
baud rate, or have the application check `free_bytes` (via
`StatusGet()`) before large writes.

## Known Limitations (Phase 1, by design — see the accepted MRD)

- TX only. No receive, no interactive CLI, no full-duplex.
- Not ISR-safe: `Write()`/`WriteString()` must be called from startup or
  main-loop/task context, never from an interrupt handler.
- No dynamic reconfiguration while transmitting (baud rate, pin, and timer
  channel are fixed once `ConfigCheck()` succeeds).
- Buffer-full policy is "return error" only — no overwrite-oldest,
  drop-newest, or blocking option in Phase 1.

## Engineering Guidelines Traceability

This implementation follows `Product_Enabling_Software_Engineering_Guides`:
naming (`rs_bb_uart_tx_*` / `RS_BB_UART_TX_*`, §13), the
public/HAL-shim/board-data-file split (§9.2), no dynamic allocation (§12
rule 5), PES-specific status enum with documented values (§12 rule 2),
target-specific isolation confined to `rs_bb_uart_tx_hal.c` and the board
config data file (§9.2), full public-function documentation of inputs,
outputs, blocking behavior, calling contexts, and error behavior (§6.3),
and demo/reusable-code separation (§5) between `src/rs_bb_uart_tx/**`
(reusable) and `src/rs_bb_uart_tx_demo.c` + `src/hal_entry.c` (demo-only).
