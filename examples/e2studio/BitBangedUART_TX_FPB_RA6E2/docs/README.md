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
were dropped. **This port (FPB-RA6E2) never hit this bug** — the fix
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
Full Baud Range" section. **Confirmed by hands-on testing on FPB-RA4E2**
that its on-board VCOM path doesn't relay below 9600 baud, even though
the Bit-Banged UART itself correctly supports the full 1200-115200 range on the TX
pin — **not yet independently re-confirmed on physical FPB-RA6E2
hardware** (flagged proactively since both boards use the same on-board
J-Link OB debug probe family, but this should be verified on this board
specifically before the Phase 4 write-up asserts it as fact for FPB-RA6E2
rather than "expected by analogy"). Needs a clear callout in the
Configuration & Adaptation Guide and in whatever supersedes the MRD's
Section 6.2 validation instructions, so a future validator doesn't waste
time trying to open a sub-9600 Tera Term session against the default demo
path on either board.

### 3. Porting to a new board: what actually needs to change vs. what carries over

Notes from the FPB-RA4E2 -> FPB-RA6E2 port, useful for the next board
(FPB-RA2E3) and worth folding into the eventual Configuration & Adaptation
Guide's own "porting to a new target" section:

**Carries over unchanged (board-agnostic Bit-Banged UART core):** every file under
`src/rs_bb_uart_tx/` except `config/rs_bb_uart_tx_data_fpb_<board>.h/.c`.
This includes the `R_BSP_PinWrite()` PFS-access-enable/disable fix in
`rs_bb_uart_tx_hal.c` — it's a generic FSP/BSP behavior
(`ra/fsp/src/bsp/mcu/all/bsp_io.h`), not board-specific, so it did not
need rediscovering for FPB-RA6E2.

**Must be independently re-verified per board, not assumed identical:**
- `BSP_FEATURE_GPT_32BIT_CHANNEL_MASK` (in
  `ra/fsp/src/bsp/mcu/<device>/bsp_feature.h`) — determines whether the
  PCLKD/4 clock divider strategy is still necessary. FPB-RA6E2 happened to
  match FPB-RA4E2 (both 0 = no 32-bit channels), but this is a
  per-silicon-family fact, not a board-family one — do not assume it for
  FPB-RA2E3 without checking that device's own `bsp_feature.h`.
- Actual PCLKD frequency (`ra_gen/bsp_clock_cfg.h`'s PLL/divider chain) —
  FPB-RA6E2 happened to also land on ~100 MHz, but a board with a
  different crystal or PLL multiplier would need the divider math
  reconsidered, not just copied.
- The board's default VCOM TX pin mapping (`configuration.xml`'s default
  `p<pin>.sci0.txd0` assignment) — happened to be P411 (BSP_IO_PORT_04_PIN_11)
  on both boards so far; do not assume this for a third board.
- The GPT overflow ELC event ID (`bsp_elc.h`'s `ELC_EVENT_GPT0_COUNTER_OVERFLOW`)
  — happened to be the same numeric value (0x0C6) on both boards; still
  worth a direct check per board since it's what makes the vector table
  derivation trustworthy rather than assumed.

**Must be redone per board regardless of whether the values end up
matching:** the `configuration.xml` patch (GPT interrupt enable +
priority, callback name, the P411-to-GPIO-output pin reassignment, and
the SCI0 Operation Mode removal — see §4 below for the full patch, now
15 properties total, not the 5 originally assumed sufficient) — applied
fresh against each board's own reference project file, not copied from
another board's already-patched XML, since the surrounding XML content
(component list, other pin defaults) genuinely differs per board even
where the specific values being patched happen to match.

### 4. Confirmed fix: SCI0 Operation Mode must be fully disabled, not just its pin assignments

**Symptom:** opening the FSP Configurator and clicking "Generate Project
Content" (or building after any change that triggers regeneration) flags
a pin conflict on P411 between the Bit-Banged UART TX's GPIO
reassignment and SCI0.

**Root cause:** the original patch (still described in earlier PhasePlan
cycles as "5 properties") removed only `sci0.txd0.p411` — the assumption
at the time was that the rest of the `sci0.*` block was inert catalog
metadata since no active SCI0 stack exists in the module list. That
assumption was wrong. SCI0's default **Operation Mode** (Simple SPI) is
active independently of whether an SCI0 stack appears in the module
list, and it claims a 3-pin pairing — P411 (TXD0), P410 (RXD0), P102
(SCK0) — as a group. Reassigning just P411 leaves the other two pins
(and the mode itself) still claiming the pairing, which is what the
Configurator's conflict checker catches.

**Confirmed fix** (verified against a real regenerated, hardware-tested
`configuration.xml` the user provided after a successful FPB-RA6E2
bring-up — not inferred/guessed): remove all of the following, keeping
only `sci0.pairing.b`:
```
<configSetting altId="p102.sci0.sck0" configurationId="p102"/>
<configSetting altId="p102.gpio_speed.gpio_speed_h" configurationId="p102.gpio_drivecapacity"/>
<configSetting altId="p102.gpio_mode.gpio_mode_peripheral" configurationId="p102.gpio_mode"/>
<configSetting altId="p410.sci0.rxd0" configurationId="p410"/>
<configSetting altId="p410.gpio_speed.gpio_speed_h" configurationId="p410.gpio_drivecapacity"/>
<configSetting altId="p410.gpio_mode.gpio_mode_peripheral" configurationId="p410.gpio_mode"/>
<configSetting altId="sci0.mode.simplespi.b" configurationId="sci0.mode"/>
<configSetting altId="sci0.rxd0.p410" configurationId="sci0.rxd0"/>
<configSetting altId="sci0.sck0.p102" configurationId="sci0.sck0"/>
<configSetting altId="sci0.txd0.p411" configurationId="sci0.txd0"/>
```
P410 and P102 revert to plain unconfigured inputs as a result — applied
to `ra_gen/pin_data.c` to match (both now use
`IOPORT_CFG_PORT_DIRECTION_INPUT`, the same pattern used elsewhere in
that file for genuinely unconfigured pins).

**Applied to both FPB-RA4E2 and FPB-RA6E2 deliveries** — both had the
identical latent gap, since both reused the same original patch;
FPB-RA4E2 just hadn't hit it yet because the user had never triggered a
RASC regeneration on that board. Worth remembering for the next port
(FPB-RA2E3): apply this full 15-property patch from the start, not the
originally-assumed 5.

### 5. Ring buffer design, TX engine/ISR pipeline, and the UART TX timing/jitter specification

**Ring buffer** (`rs_bb_uart_tx_ring_buffer.c`/`.h`): a lock-free
single-producer/single-consumer structure. `head` is written only by the
producer (`enqueue()`, main-loop/task context); `tail` is written only by
the consumer (`dequeue()`, GPT ISR context). Occupancy is derived on
demand from `head - tail` rather than tracked in a separate shared
`count` field, which means neither `enqueue()` nor `dequeue()` needs a
critical section — see the header's file-level comment for the full
correctness argument. Usable capacity is
`RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES - 1` bytes; the configured size
must be a power of two, enforced by a `_Static_assert`. The demo's
startup banner is sent as a single `WriteString()` call rather than
several separate ones, so the entire message is enqueued before the GPT
timer starts.

**TX engine and GPT ISR** (`rs_bb_uart_tx_engine.c`/`.h`,
`rs_bb_uart_tx_hal.c`): structured as a one-tick pipeline.
`rs_bb_uart_tx_engine_output_get()` is O(1) and touches no ring-buffer
state — it returns the pin level already decided by the *previous*
tick's `rs_bb_uart_tx_engine_advance()` call. The GPT ISR writes the
GPIO pin first, before anything else; only afterward does it call
`rs_bb_uart_tx_engine_advance()`, which prepares the *next* tick's
output (including any ring-buffer dequeue) with a full bit period of
slack rather than none. See `rs_bb_uart_tx_engine.h`'s file-level
comment for the complete design, including the race-safety argument for
the one-tick-ahead prefetch.

**UART TX timing/jitter specification**, three separate measurements
(see `docs/RA2E3_Bit_Banged_UART_TX_Jitter_Specification.txt`, kept
alongside this project, for the full research this is based on):

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
and reports all three measurements.

**Verification status on this board specifically:** Bit-Banged UART core
files confirmed byte-identical to the reference delivery (same files,
unmodified); host-build compiled and the full engine-pipeline test suite
(FIFO ordering, race-window simulation, byte-boundary sequencing) passes
here too. A hardware capture for this board is pending.
