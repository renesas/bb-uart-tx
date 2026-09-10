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

**Real example hit during bring-up:** the demo's startup banner is ~304
bytes, sent as ~10 back-to-back `WriteString()` calls before the GPT timer
has had a chance to drain any of it. This was first hit at the original
256-byte buffer default, where the last couple of calls in that burst
didn't fit and were dropped — user observed `full=2` and correctly
diagnosed it via `StatusGet()`. **The shipped default has since been
raised to 512** (this fix is already applied, not just documented — see
`src/rs_bb_uart_tx/rs_bb_uart_tx_cfg.h`), so a fresh checkout of this
project does not hit this bug; the guidance below remains for anyone who
needs a different size for their own application.

**How to change the buffer size:**
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

### 2. On-board J-Link VCOM bridge does not support the full documented baud range

See `../README.md`'s "On-board J-Link VCOM Bridge Does Not Support the
Full Baud Range" section — confirmed by hands-on testing that the
on-board VCOM path on FPB-RA4E2 doesn't relay below 9600 baud, even
though the Bit-Banged UART itself correctly supports the full 1200-115200 range on
the TX pin. Needs a clear callout in the Configuration & Adaptation Guide
and in whatever supersedes the MRD's Section 6.2 validation instructions,
so a future validator doesn't waste time trying to open a sub-9600 Tera
Term session against the default demo path.

### 3. Confirmed fix: SCI0 Operation Mode must be fully disabled, not just its pin assignments

**Symptom:** opening the FSP Configurator and clicking "Generate Project
Content" (or building after any change that triggers regeneration) flags
a pin conflict on P411 between the Bit-Banged UART TX's GPIO
reassignment and SCI0.

**Root cause:** the original `configuration.xml` patch removed only
`sci0.txd0.p411` — the assumption at the time was that the rest of the
`sci0.*` block was inert catalog metadata since no active SCI0 stack
exists in the module list. That assumption was wrong. SCI0's default
**Operation Mode** (Simple SPI) is active independently of whether an
SCI0 stack appears in the module list, and it claims a 3-pin pairing —
P411 (TXD0), P410 (RXD0), P102 (SCK0) — as a group. Reassigning just
P411 leaves the other two pins (and the mode itself) still claiming the
pairing, which is what the Configurator's conflict checker catches. This
was discovered on FPB-RA6E2 (the user had to disable it manually via the
GUI to get a successful build+regeneration), then confirmed to be the
same latent gap in this FPB-RA4E2 delivery's `configuration.xml` — just
never triggered here because a RASC regeneration had never been attempted
on this board.

**Confirmed fix** (verified against a real regenerated, hardware-tested
`configuration.xml` the user provided after a successful FPB-RA6E2
bring-up — not inferred/guessed) — already applied to this delivery's
`configuration.xml`, not just documented: remove all of the following,
keeping only `sci0.pairing.b`:
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
`ra_gen/pin_data.c` did not need changing for this board — this project's
`pin_data.c` was already a minimal custom table containing only the P411
entry (written that way from the start, unlike FPB-RA6E2 which kept the
reference project's full board pin table), so it never carried the
now-removed P410/P102 peripheral assignments in the first place.

### 3. Ring buffer design, TX engine/ISR pipeline, and the UART TX timing/jitter specification

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
