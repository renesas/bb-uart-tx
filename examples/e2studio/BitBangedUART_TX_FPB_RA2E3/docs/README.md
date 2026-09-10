# Documentation

Full customer-facing documentation (README/Quick Start/Integration Guide/
Function Reference/Configuration & Adaptation Guide, per PES_Deliverables
and the Engineering Guidelines Section 16) is a **Document & Package**
(Phase 4) deliverable, not part of this **Develop & Test** (Phase 3) source
drop.

For now:
- `../README.md` covers build/flash/run instructions and the public API
  summary, and is being kept up to date as an interim reference during
  bring-up.
- `../PhasePlan.md` is the development/bring-up log — useful for *how* a
  decision was made, but not customer-facing.
- `src/rs_bb_uart_tx/*.h` files carry full Doxygen-style API documentation
  inline (function contracts, calling-context rules, error codes).
- The 8 architecture/UML diagrams (component, sequence, state machines,
  timing, signal path, concurrency model) referenced by the Architecture &
  Design document exist as both `.svg`/`.png` renders and `.puml` source
  from earlier in this project's working history.

## Notes for the Phase 4 documentation pass

Running list of operational findings from real hardware bring-up that
should be written up properly (Troubleshooting section, Configuration &
Adaptation Guide, etc.) once formal documentation is produced. Each entry
below is a raw note, not polished prose — expand these into the actual
guide rather than copying verbatim.

### 1. `buffer_full_count` ("full" in the demo's status line) non-zero — increase the ring buffer size

**Symptom:** `rs_bb_uart_tx_StatusGet()`'s `buffer_full_count` field (shown
as `full=N` in the demo's periodic status line) is greater than zero.

**What it means:** one or more `rs_bb_uart_tx_Write()` /
`rs_bb_uart_tx_WriteString()` calls were rejected because the requested
data didn't fit in the ring buffer's *currently free* space at the moment
of the call (Architecture & Design Section 13.3's all-or-nothing enqueue
policy — nothing partial is ever written; the whole call fails). This is
silent unless the caller checks the return value — the demo application
deliberately ignores it (`(void) rs_bb_uart_tx_WriteString(...)`) to keep
the example simple, so a real integration should decide whether to check
it.

**Original finding (FPB-RA4E2 bring-up, not this board):** the demo's
startup banner is ~304 bytes, sent as ~10 back-to-back `WriteString()`
calls before the GPT timer has had a chance to drain any of it. At a
256-byte buffer, the last couple of calls in that burst didn't fit and
were dropped. **This port (FPB-RA2E3) never hit this bug** — the fix
(buffer size default raised to 512, see below) was already applied to
the shared Bit-Banged UART core before this board was ported, so the port started
from the corrected default.

**How to increase the buffer, if you ever need to (e.g. a larger custom
banner, or a smaller buffer for RAM-constrained targets going the other
direction):**
1. Open `src/rs_bb_uart_tx/rs_bb_uart_tx_cfg.h`.
2. Change:
   ```c
   #ifndef RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES
     #define RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES     (512U)   // current default
   #endif
   ```
   to whichever value you need. Architecture & Design Section 5.7 lists the
   validated reference sizes: 64, 128, 256, 512, 1024 bytes. (Any positive
   value compiles, but only those five have been validated against the
   timing/RAM requirements.)
3. Rebuild. `sizeof(rs_bb_uart_tx_ctrl_t)` grows by the same amount (the
   buffer is statically embedded in the control block — no dynamic
   allocation, per Engineering Guidelines Software Development Rule 5), so
   there's a compile-time safety net: `rs_bb_uart_tx.c` has a
   `_Static_assert` against `RS_BB_UART_TX_CFG_CTRL_RAM_LIMIT_BYTES` (2048
   bytes by default) that will fail the *build* with a clear message
   rather than silently overrun RAM if the buffer is made too large for
   the target's available RAM.

**Important nuance to include in the real write-up:** a bigger buffer only
absorbs *bursts* (like the banner example above) — it does not fix a
*sustained* rate mismatch. If an application enqueues data faster, on
average, than the configured baud rate can drain it, `buffer_full_count`
will keep climbing no matter how large the buffer is; the real fix in
that case is a lower data rate, a higher baud rate, or the application
pacing its own writes (e.g. checking `free_bytes` via `StatusGet()`
before a large write). The Configuration & Adaptation Guide should
clearly distinguish "occasional burst, fixed by a bigger buffer" from
"sustained overrun, not fixable by buffer size alone."

### 2. On-board J-Link VCOM bridge may not support the full documented baud range

See `../README.md`'s "On-board J-Link VCOM Bridge May Not Support the
Full Baud Range" section. **Confirmed by hands-on testing on both
FPB-RA4E2 and FPB-RA6E2** that the on-board VCOM path doesn't relay below
9600 baud, even though the Bit-Banged UART itself correctly supports the full
1200-115200 range on the TX pin — **not yet independently confirmed on
physical FPB-RA2E3 hardware** (flagged proactively since all three boards
use the same on-board J-Link OB debug probe family, but this should be
verified on this board specifically before the Phase 4 write-up asserts
it as fact for FPB-RA2E3 rather than "expected by analogy, 2 for 2 so
far"). Needs a clear callout in the Configuration & Adaptation Guide and
in whatever supersedes the MRD's Section 6.2 validation instructions, so
a future validator doesn't waste time trying to open a sub-9600 Tera Term
session against the default demo path on any of the three boards.

### 3. Porting to a new board: what actually needs to change vs. what carries over

Notes accumulated across the FPB-RA4E2 -> FPB-RA6E2 -> FPB-RA2E3 ports,
useful for the next board and worth folding into the eventual
Configuration & Adaptation Guide's own "porting to a new target" section.
FPB-RA2E3 is a particularly good example of why every item in the second
list below must be independently re-checked, not assumed from precedent
— it differs from FPB-RA4E2/FPB-RA6E2 on *every single one* of them:

**Carries over unchanged (board-agnostic Bit-Banged UART core):** every file under
`src/rs_bb_uart_tx/` except `config/rs_bb_uart_tx_data_fpb_<board>.h/.c`.
This includes the `R_BSP_PinWrite()` PFS-access-enable/disable fix in
`rs_bb_uart_tx_hal.c` — it's a generic FSP/BSP behavior
(`ra/fsp/src/bsp/mcu/all/bsp_io.h`), not board-specific, so it did not
need rediscovering for FPB-RA6E2 or FPB-RA2E3. The GPT period-calculation
logic in `rs_bb_uart_tx_hal_gpt_open()` is also unchanged — it reads the
*actual* clock frequency from the HAL driver at runtime (`infoGet()`),
so it works correctly regardless of PCLKD frequency or divider choice
without needing per-board code changes; only the board config data
file's `source_div` constant needs to be chosen correctly per board.

**Must be independently re-verified per board, not assumed identical —
FPB-RA2E3 differed on every one of these:**
- `BSP_FEATURE_GPT_32BIT_CHANNEL_MASK` (in
  `ra/fsp/src/bsp/mcu/<device>/bsp_feature.h`) — FPB-RA4E2/FPB-RA6E2 both
  `0x00` (no 32-bit channels, PCLKD/4 divider required to avoid 16-bit
  overflow). **FPB-RA2E3 is `0x01`** (channel 0 genuinely 32-bit).
- Actual PCLKD frequency (`ra_gen/bsp_clock_cfg.h`'s clock source/divider
  chain) — FPB-RA4E2/FPB-RA6E2 both ~100 MHz (HOCO x PLL 10.0, /2).
  **FPB-RA2E3 is 48 MHz** (no PLL at all — HOCO 48 MHz direct, PCLKD /1).
  Combined with the 32-bit channel above, this meant FPB-RA2E3 correctly
  uses **no clock divider** (`TIMER_SOURCE_DIV_1`) — copying the other
  boards' `TIMER_SOURCE_DIV_4` would have been a real (if low-severity —
  just needlessly reduced timing resolution, not a functional bug)
  mistake here.
- The board's default TX-pin peripheral pairing — FPB-RA4E2/FPB-RA6E2
  both pair their VCOM pin (P411) with **SCI0 in Simple SPI mode**, a
  3-pin pairing (TXD0/RXD0/SCK0) that leaves one inert `sci0.pairing.b`
  property behind after the fix. **FPB-RA2E3's P109 is paired with SCI9
  in Simple I2C mode instead** — a different peripheral channel entirely,
  a different mode, only a 2-pin pairing (TXD9/RXD9, no clock line), and
  critically **no leftover pairing property at all** after the fix (see
  §4 below) — copying the "keep `sci0.pairing.b`" assumption verbatim
  would have left a real orphaned/incorrect property in the XML here.
- The GPT overflow ELC event ID (`bsp_elc.h`'s
  `ELC_EVENT_GPT0_COUNTER_OVERFLOW`) — `0x0C6` on FPB-RA4E2/FPB-RA6E2,
  **`0x04A` on FPB-RA2E3**. Harmless in practice since the code references
  the event symbolically, not by hardcoded number, but still worth a
  direct check per board as a general discipline.
- LED1 pin (`board_leds.c`) — P207 on FPB-RA4E2/FPB-RA6E2, **P213 on
  FPB-RA2E3**. Not used by the Bit-Banged UART itself, but relevant if you ever reuse
  the LED-blink hardware-sanity-check technique from the FPB-RA4E2
  bring-up (see PhasePlan.md) on this board.

**Must be redone per board regardless of whether the values end up
matching:** the `configuration.xml` patch (GPT interrupt enable +
priority, callback name, the TX-pin-to-GPIO-output reassignment, and the
full pairing-mode removal — see §4 below) — applied fresh against each
board's own reference project file, not copied from another board's
already-patched XML. The FPB-RA2E3 pairing removal in particular has a
*different line count and different lines* than the FPB-RA4E2/FPB-RA6E2
one (7 lines removed here vs. 10 there, no property kept here vs. one
kept there) — this is not just "the same 10-line patch with different
pin numbers substituted in," it required rediscovering the actual
pairing structure from this board's own reference file.

### 4. Confirmed-pattern fix, independently re-derived for this board's SCI9/P109 pairing (not copied from the SCI0/P411 fix)

**Symptom, if you ever regenerate via the Smart Configurator:** opening
the FSP Configurator and clicking "Generate Project Content" flags a pin
conflict on P109 between the Bit-Banged UART TX's GPIO reassignment
and SCI9.

**Root cause:** the same underlying mechanism discovered on FPB-RA6E2's
SCI0/P411 conflict (see the FPB-RA4E2/FPB-RA6E2 deliveries'
`docs/README.md` for that original discovery), but on a different
peripheral: SCI9's default **Operation Mode** (Simple I2C) is active
independently of whether an SCI9 stack appears in the module list, and it
claims a 2-pin pairing — P109 (TXD9) and P110 (RXD9) — as a group.
Reassigning just P109 would leave P110 and the mode itself still claiming
the pairing.

**Fix, applied proactively to this delivery's `configuration.xml` and
`ra_gen/pin_data.c` from the start** (based on the confirmed SCI0/P411
mechanism, adapted to this board's actual SCI9/P109 structure — found by
directly searching this board's own reference `configuration.xml` for
every `p109.*`/`p110.*`/`sci9.*` line, not by assuming the FPB-RA6E2 fix's
line list applies with names substituted): remove all of the following —
unlike the SCI0 fix, **nothing `sci9.*`-prefixed is kept**, since this
mode has no separate pairing-selector property the way SCI0's Simple SPI
mode did:
```
<configSetting altId="p109.sci9.txd9" configurationId="p109"/>
<configSetting altId="p109.gpio_mode.gpio_mode_peripheral" configurationId="p109.gpio_mode"/>
<configSetting altId="p110.sci9.rxd9" configurationId="p110"/>
<configSetting altId="p110.gpio_mode.gpio_mode_peripheral" configurationId="p110.gpio_mode"/>
<configSetting altId="sci9.mode.simplei2c.free" configurationId="sci9.mode"/>
<configSetting altId="sci9.rxd9.p110" configurationId="sci9.rxd9"/>
<configSetting altId="sci9.txd9.p109" configurationId="sci9.txd9"/>
```
(P109's own two lines are *replaced*, not just removed — with
`p109.output.high` / `p109.gpio_mode.gpio_mode_out.high`, matching the
GPIO-output-high pattern used for the TX pin on the other boards.)

P110 reverts to a plain unconfigured input as a result — applied to
`ra_gen/pin_data.c` to match (`IOPORT_CFG_PORT_DIRECTION_INPUT`, the same
pattern used elsewhere in that file for genuinely unconfigured pins).

**Confidence level — different from the FPB-RA4E2/FPB-RA6E2 fix:** the
SCI0/P411 fix was verified against a real, hardware-tested, regenerated
`configuration.xml` the user provided after a successful build+flash. The
SCI9/P109 fix here has **not** had the equivalent confirmation yet — it's
applied based on the same mechanism and the same care in reading this
board's own reference file, but should be treated as "should be correct,
following a proven pattern" rather than "confirmed correct" until this
board is actually built, and ideally until someone attempts a fresh RASC
regeneration on it the way FPB-RA6E2's fix was discovered and verified.

### 5. Ring buffer design, TX engine/ISR pipeline, and the UART TX timing/jitter specification

**Ring buffer** (`rs_bb_uart_tx_ring_buffer.c`/`.h`): a lock-free
single-producer/single-consumer structure. `head` is written only by the
producer (`enqueue()`, main-loop/task context); `tail` is written only by
the consumer (`dequeue()`, GPT ISR context). Occupancy is derived on
demand from `head - tail` rather than tracked in a separate shared
`count` field, which means neither `enqueue()` nor `dequeue()` needs a
critical section — see the header's file-level comment for the full
correctness argument. Usable capacity is
`RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1` bytes (one slot reserved so
`head == tail` stays an unambiguous "empty" signal); the configured size
must be a power of two, enforced by a `_Static_assert`.

**TX engine and GPT ISR** (`rs_bb_uart_tx_engine.c`/`.h`,
`rs_bb_uart_tx_hal.c`): structured as a one-tick pipeline.
`rs_bb_uart_tx_engine_output_get()` is O(1) and touches no ring-buffer
state — it returns the pin level already decided by the *previous*
tick's `rs_bb_uart_tx_engine_advance()` call. The GPT ISR calls this and
writes the GPIO pin first, before anything else; only afterward does it
call `rs_bb_uart_tx_engine_advance()`, which prepares the *next* tick's
output (including any ring-buffer dequeue) with a full bit period of
slack rather than none. See `rs_bb_uart_tx_engine.h`'s file-level comment
for the complete design, including the race-safety argument for the
one-tick-ahead prefetch (a `Write()` call landing while the prefetch is
in flight is always picked up correctly on the following tick's fresh
recheck — nothing is ever stranded).

One accepted, deliberate trade-off: the first bit period after a fresh
`Write()` from idle always outputs idle-high before the first byte's
start bit begins one tick later (~8.68µs of extra launch latency at
115200 baud, once per idle-to-transmitting transition). This is a
launch-latency characteristic, not a bit-timing one, and has no effect
on what a receiver decodes.

**UART TX timing/jitter specification**, three separate measurements
(see `RA2E3_Bit_Banged_UART_TX_Jitter_Specification.txt`, kept alongside
this project, for the full research this is based on):

1. **Average baud-rate error** — whole-burst average, target ≤±1%
   (preferred ≤±0.5%).
2. **Intra-character edge jitter** — every adjacent bit-period pair
   *within* one byte (start bit through the transition into the stop
   bit), evaluated relative to that byte's own start-bit edge. Target
   ≤10% for this project.
3. **Stop-bit duration** — a minimum-duration requirement, not a
   symmetric jitter target: `Tstop >= Tbit * (1 - allowed_error)`
   (default 1% → `Tstop >= 0.99*Tbit`). A UART receiver re-synchronizes
   its bit-timing reference at every start-bit falling edge, so the gap
   from one byte's stop bit to the next byte's start bit only needs to
   be long enough — it does not need to match nominal timing.

TX-request-to-start-bit latency is a separate, application-specific
metric, not part of this specification.

`tools/jitter_decode.py` decodes a logic-analyzer capture of the TX pin
and reports all three measurements. Real hardware captures taken against
this design on this board show zero violations on all three.
