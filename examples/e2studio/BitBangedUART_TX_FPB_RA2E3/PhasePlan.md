# PhasePlan -- Bit-Banged UART TX PES (FPB-RA4E2)

**Purpose of this file:** Living checkpoint for an in-progress implementation task. If the chat
session is interrupted or exceeds max length, paste this file back to Claude (along with the
7 originally uploaded reference documents, if starting a fresh conversation) and say
"resume from PhasePlan.md" to continue exactly where work left off.

**Last updated:** Cycle 9 addendum 6 (resolved) -- standalone FPB-RA2E3
recapture confirms debugger-attachment state does NOT explain the
Section 5.6 miss: max jitter matches the original debugger-attached
capture to 2 decimal places (21.89% both times). The failure is real and
repeatable, not a test-setup artifact. One secondary finding: stop-bit
timing spread is 10x wider standalone (stdev 210.8ns vs 18.8ns) though
mean/max are unchanged. Scope note: this only rules out the debugger
variable for RA2E3, not for the still-unexplained RA4E2-vs-RA6E2 gap.
See Section 18.

---

## 1. Goal

Implement the **Bit-Banged UART TX** Product Enabling Software (PES) -- short name `bb_uart_tx`,
component ID `RS_BB_UART_TX` -- for the **FPB-RA4E2** target board first, as a bare-metal VS
Code / CMake example project, following:

- `PES_Bit-BangedUART_Acceptance_ArchDesign_v03.pdf` (Candidate Acceptance + Architecture &
  Design -- the authoritative functional/architectural spec; same content as the
  `PES_BitBangedUART_ArchitectureAndDesign_v03_NumberingFixed.md` from earlier in this
  conversation).
- `BitBangedUartUMLs.zip` -- the 8 `.puml` diagrams (already produced earlier in this
  conversation) that render poorly embedded in the Confluence-exported PDF; used here as the
  visual cross-check for module boundaries, state machines, and sequencing.
- `Product_Enabling_Software_Engineering_Guides_072926.pdf` -- naming conventions, file
  layout, the 19 Software Development Rules, metadata framework.
- `PES_Creation_Process.pdf` / `PES_Deliverables.pdf` / `Product_Enabling_Software_for_EmbeddedProducts.pdf`
  -- process/deliverable context (this work corresponds to Step 3, "Develop & Test").
- `RTC_EVENT_LOGGER_FPB_RA4E2.zip` -- reference VS Code/CMake FSP project. **Used as the
  structural template** for file naming, the public/HAL-shim/config-data split, the CMake
  build, and the "preset instance" pattern -- NOT copied verbatim, since that project is
  FreeRTOS-based and ours must be **bare-metal** (FreeRTOS is explicitly out of scope for
  Phase 1 per the AUC).

## 2. Key facts extracted from the reference project (do not re-derive -- reuse this)

- Build system: CMake + Ninja + LLVM ARM toolchain. Source files under `src/`, `ra_gen/`,
  `ra/` are picked up by `file(GLOB_RECURSE ...)` in `cmake/GeneratedSrc.cmake` -- **new PES
  source files do NOT need to be added to CMakeLists.txt manually**, just placed under `src/`.
- `cmake/GeneratedSrc.cmake` invokes RASC (Renesas Smart Configurator) as a pre-build step to
  regenerate `ra_gen/*` from `configuration.xml` when available. We do not have RASC in this
  sandbox, so `ra_gen/*` files here are **hand-authored to match typical RASC output** and
  are clearly marked as such -- see Section 6 Known Limitations.
- Vendored FSP driver source under `ra/fsp/src/<module>/` is only present for modules
  actually used by a project (confirmed: reference project has `r_rtc`, `r_sci_uart`,
  `r_icu`, `r_ioport`, `bsp`, `rm_freertos_port` -- **no `r_gpt`**, since it doesn't use a
  GPT timer). Our PES needs GPT -> see Section 6.
- Naming pattern confirmed against Engineering Guidelines: `rs_<shortname>_<Operation>()`
  public functions, `rs_<shortname>_<file>.c/h` files, `RS_<SHORTNAME>_<NAME>` macros,
  `rs_<shortname>_cfg_t` / `_ctrl_t` / `_status_t` types, board-specific data file pattern
  `rs_<shortname>_data_<board>.c/h`.
- "Preset instance" pattern (from `rs_rtc_logger_data_fpb_ra4e2.c`): a board-specific config
  data file defines a **standalone FSP driver instance** (control block + cfg struct + `.p_api`
  pointer to the FSP driver's API struct, e.g. `g_rtc_on_rtc`) that does NOT depend on
  `ra_gen/hal_data.c` having been Configurator-generated for that exact instance. This lets
  the PES compile and be usable even before/without a full Configurator pass, as long as the
  underlying FSP driver source (`ra/fsp/src/r_xxx/`) is present. We replicate this for GPT.
  IOPORT (GPIO) is lower-risk: `r_ioport` is already vendored, and P411 direct pin control
  needs no Configurator step.
- `main.c` (bare-metal FSP variant, not present in this FreeRTOS reference project but
  standard FSP convention): `main()` calls `g_hal_init()` then `hal_entry()`; application
  code lives in `hal_entry()` in a project-owned `hal_entry.c`. We use this pattern instead
  of the reference project's `blinky_thread_entry.c` (FreeRTOS thread).
- `src/hal_warmstart.c` and pin-config-via-`R_BSP_WarmStart` are generic FSP boilerplate,
  reusable essentially as-is.

## 3. Target repository layout

**Superseded by Cycle 5 (Section 11) -- kept below for history.** The layout
described in this section (CMake/VS Code project, `Bit_Banged_UART_TX_FPB_RA4E2/`)
was the delivery target through Cycle 4. As of Cycle 5 the delivered
project is an **e2 studio** project (`BitBangedUART_TX_FPB_RA4E2/` -- no
underscores after "Bit", matching the user's e2 studio upload's naming,
`.project`/`.cproject` instead of `CMakeLists.txt`/`CMakePresets.json`).
See this project's own `README.md` "Repository Layout" section for the
current, accurate structure. The PES core source tree itself
(`src/rs_bb_uart_tx/**`) is unchanged between the two.

```
Bit_Banged_UART_TX_FPB_RA4E2/   (historical -- Cycles 1-4, CMake/VS Code)
  README.md
  CMakeLists.txt              (adapted from reference: project name, include paths)
  CMakePresets.json           (copied from reference, unchanged)
  Config.cmake                (copied from reference, unchanged)
  cmake/
    GeneratedCfg.cmake        (copied from reference -- placeholder, RASC fills real flags)
    GeneratedSrc.cmake        (copied from reference, unchanged)
    llvm.cmake                (copied from reference, unchanged)
    prebuild.cmake            (copied from reference, unchanged)
  configuration.xml           (adapted: BSP settings reused from reference minus AWS
                               FreeRTOS RTOS selection; GPT + P411 GPIO stack added
                               best-effort -- see Section 6)
  ra/
    board/, arm/, fsp/src/{bsp,r_ioport}/   (copied from reference -- generic FSP core)
    fsp/src/r_gpt/                          (NOT included -- see Section 6, must be added via RASC)
  ra_cfg/ , ra_gen/           (adapted: hal_data/pin_data/vector_data hand-edited for
                               GPT + P411 GPIO-output; bsp_clock_cfg etc. copied)
  src/
    hal_entry.c                              (bare-metal application entry point)
    hal_warmstart.c                          (copied from reference, unchanged)
    fault_handler.c                          (copied from reference, unchanged)
    rs_bb_uart_tx_demo.c                     (example/demo layer -- banner, heartbeat strings)
    rs_bb_uart_tx/
      rs_bb_uart_tx.h                        (public API)
      rs_bb_uart_tx.c                        (core orchestration + internal lifecycle FSM)
      rs_bb_uart_tx_types.h                  (cfg_t / ctrl_t / status_t / enums)
      rs_bb_uart_tx_cfg.h                    (build-time config macros)
      rs_bb_uart_tx_ring_buffer.c/.h         (ring buffer manager -- internal)
      rs_bb_uart_tx_engine.c/.h              (TX bit-level state machine -- internal)
      rs_bb_uart_tx_hal.c/.h                 (GPT + GPIO target-adaptation shim, ISR)
      config/
        rs_bb_uart_tx_data_fpb_ra4e2.c/.h    (preset GPT instance + ConfigGen/Check/GetSize)
  docs/
    (optional light docs; full docs are a Phase-4 "Document & Package" deliverable, not
    required for this Phase-3 "Develop & Test" pass -- kept minimal here)
```

## 4. Architectural decisions locked in (from the Arch&Design doc -- do not re-litigate)

- Public functions (only these 6 in the public header, non-blocking, `fsp_err_t`-free --
  uses PES-specific `rs_bb_uart_tx_status_t` per Engineering Guidelines Section 12 rule 2):
  `rs_bb_uart_tx_ConfigGetSize()`, `rs_bb_uart_tx_ConfigGen()`, `rs_bb_uart_tx_ConfigCheck()`,
  `rs_bb_uart_tx_Write()`, `rs_bb_uart_tx_WriteString()`, `rs_bb_uart_tx_StatusGet()`.
- `Open/Start/Stop/Close` exist ONLY as internal `static` helpers inside `rs_bb_uart_tx.c`
  (Arch doc Section 10 explicit instruction) -- not in the public header.
- Internal lifecycle states: `UNCONFIGURED -> CONFIGURED -> READY <-> TRANSMITTING`, plus `ERROR`
  reachable from any state (Arch doc Section 10 / diagram 4).
- TX bit-engine states: `TX_IDLE -> TX_START_BIT -> TX_DATA_BITS -> TX_STOP_BIT -> TX_NEXT_BYTE`,
  looping back to `TX_START_BIT` or `TX_IDLE` (Arch doc Section 12.2 / diagram 5).
- Frame: 8N1, LSB-first, idle-high, one GPT compare-match interrupt per bit period.
- Ring buffer: static, default 256 B (configurable 64/128/256/512/1024), all-or-nothing
  enqueue, buffer-full -> deterministic error return (no overwrite, no drop, no blocking).
- No dynamic memory allocation anywhere.
- Non-blocking write functions only; not ISR-safe in Phase 1 (`Write`/`WriteString` valid
  from startup + main-loop/task context only, NOT from ISR context -- matches Section 17.1 table).
- Board mapping (FPB-RA4E2): TX pin **P411** (board VCOM TXD0 path), one dedicated GPT
  channel, on-board J-Link VCOM is the default PC connection (no external adapter needed).
- Default baud 115200, supported set {1200,2400,4800,9600,19200,38400,57600,115200}, timing
  tolerance +/-3%, jitter target <=+/-10% of bit period (Confluence acceptance doc Section 5/6).

## 5. Progress tracker

| # | Item | Status | Notes |
|---|------|--------|-------|
| 1 | Explore uploads, extract zips, study reference project | **Done** | See Section 2 above for extracted facts |
| 2 | Write PhasePlan.md (this file) | **Done** | |
| 3 | `rs_bb_uart_tx_cfg.h` | **Done** | |
| 4 | `rs_bb_uart_tx_types.h` | **Done** | Public types only -- `ctrl_t` moved out, see Section 8 |
| 5 | `rs_bb_uart_tx_ring_buffer.h/.c` | **Done** | PRIMASK-based critical section, all-or-nothing enqueue |
| 6 | `rs_bb_uart_tx_engine.h/.c` | **Done** | Hand-traced tick-by-tick timing for off-by-one errors -- clean |
| 7 | `rs_bb_uart_tx_hal.h/.c` | **Done** | GPT + GPIO shim, ISR callback; timer stops itself when queue empties |
| 8 | `rs_bb_uart_tx.h` (public API) | **Done** | |
| 9 | `rs_bb_uart_tx.c` (core + internal lifecycle) | **Done** | Singleton `g_bb_uart_tx_ctrl`; RAM budget `_Static_assert` |
| 10 | `config/rs_bb_uart_tx_data_fpb_ra4e2.h/.c` | **Done** | Preset GPT instance; flagged fields to verify vs. installed FSP |
| 11 | `src/rs_bb_uart_tx_demo.c` | **Done** | Banner + heartbeat + periodic StatusGet, matches doc's Section 8.5 sample output |
| 12 | `src/hal_entry.c`, `hal_warmstart.c`, `fault_handler.c` | **Done** | fault_handler adapted to drop the FreeRTOS stack-overflow hook |
| 13 | CMake/project scaffolding (copy + adapt from reference) | **Done** | CMakeLists.txt, CMakePresets.json, Config.cmake, cmake/*, script/fsp.lld, memory_regions.lld, code-workspace |
| 14 | `ra_gen`/`ra_cfg` -- GPT + P411 fully resolved | **Done** | Real `r_gpt.c` vendored; `hal_data.c`, `vector_data.h/.c`, `main.c`, `bsp_cfg.h` all verified against real RASC output (Cycle 4) |
| 14b | `configuration.xml` | **Done** | Included (Cycle 4) -- copied from real reference, 5 targeted patches applied and verified |
| 15 | `README.md` with build instructions | **Done** | Simplified in Cycle 4 -- old "add GPT stack" instructions replaced with small "GPT Interrupt Setup" + "GPT Clock Divider" notes |
| 16 | Zip and deliver source (checkpoint before tool-time runs out) | **Done** | Delivered Cycle 3; Cycle 4 upgrade zip pending re-delivery |
| 17 | Structural QA pass (brace/paren balance, include-graph check, duplicate-typedef scan) | **Done** | Re-run after Cycle 4 changes -- 35 files, all balanced |
| 18 | Final polish pass / second zip if time allows | Not started | Candidates: host-buildable unit-test stub for `rs_bb_uart_tx_engine.c`/`ring_buffer.c` (both already written to be host-testable via `RS_BB_UART_TX_TEST_HOST_BUILD`); port to FPB-RA6E2/FPB-RA2E3 |

## 6. Known limitations / honesty notes (tell the user, don't hide)

**Superseded by Cycle 4 (Section 9) -- kept here for history.** As of Cycle 4, the
GPT driver source IS vendored and `ra_gen`/`configuration.xml` ARE included
and verified against a real RASC-generated reference project, using a
second reference project the user uploaded specifically for this purpose
(`VSCode_FPB_RA4E2_Baremetal.zip`). No Smart Configurator step is required
to build. The items below described the state as of Cycle 3, before that
upload:

- ~~No RASC/e2 studio in this sandbox... must be verified/regenerated via
  the real RA Smart Configurator before building on hardware.~~ Resolved --
  see Section 9.
- ~~`ra/fsp/src/r_gpt/` ... driver source itself must be vendored by adding
  a GPT Timer stack in the Smart Configurator.~~ Resolved -- `r_gpt.c` is
  now vendored directly from a verified real reference. See Section 9.
- GPIO (P411 digital output) never had this gap -- `r_ioport` was already
  vendored and the direct pin-control path never required a Configurator
  pass. Still true.
- The PES core source code (`src/rs_bb_uart_tx/**`) remains the
  fully-original, fully-reviewable deliverable and the primary focus of
  engineering quality/review -- that has not changed. What changed in
  Cycle 4 is that the FSP/CMake scaffolding around it is now
  verified-correct rather than "starting point, verify in RASC."

**What is still worth independently verifying before flashing real
hardware** (true of any new timing-sensitive PES, not a gap specific to
this delivery): actual bit timing on a scope/logic analyzer at each
supported baud rate, per Architecture & Design Section 21.2's own required
validation strategy.

## 7. Design decisions made during implementation (Cycle 2-3) not in the original plan

These resolve items that Architecture & Design v0.3 Section 26 ("Open Items
for Design Review") left explicitly open. Documented here so a resumed
session doesn't re-litigate them without cause:

- **Singleton control block, not caller-allocated.** Re-reading Section
  19.4's example pseudocode, `rs_bb_uart_tx_Write()`/`WriteString()`/
  `ConfigGen()`/`ConfigCheck()` are all called with no control-block
  argument anywhere -- only one instance (one GPIO pin, one GPT channel) is
  ever needed. Implemented as a single static `g_bb_uart_tx_ctrl` inside
  `rs_bb_uart_tx.c`, with the struct definition itself moved to a new
  internal-only header (`rs_bb_uart_tx_ctrl.h`, not part of the public
  API) rather than living in the public `rs_bb_uart_tx_types.h` the way
  `rs_rtc_logger_ctrl_t` does in the reference project. This is a
  deliberate deviation from the reference project's multi-instance-capable
  pattern -- reduces the public surface, matches the doc's own pseudocode
  exactly, and resolves Open Items #1 and #3.
- **`ConfigCheck()` performs the internal Open() on success.** The doc's
  pseudocode never calls anything resembling an `Open()` between
  `ConfigCheck()` and the first `WriteString()`. Rather than inventing an
  undocumented extra call, `rs_bb_uart_tx_ConfigCheck()` -- on successful
  validation -- also binds the config, opens the GPT+GPIO HAL resources,
  resets the ring buffer/engine, and transitions the lifecycle to READY.
  This is explicitly sanctioned by the Engineering Guidelines (Section 6.2/Section 7:
  initialization "may be hidden inside a higher-level public function").
- **GPT timer starts/stops on demand**, not continuously. The timer is
  started the first time `Write()`/`WriteString()` moves the PES from
  READY to TRANSMITTING (queue was empty, now has data), and the GPT ISR
  itself calls `rs_bb_uart_tx_hal_gpt_stop()` and transitions back to READY
  the moment the ring buffer empties (`TX_NEXT_BYTE` -> `TX_IDLE` in the
  bit-engine). This avoids interrupting on every bit period while idle and
  maps exactly onto the documented READY <-> TRANSMITTING transitions
  (Section 10: "TRANSMITTING -> READY: TX ring buffer empties").
- **`ConfigGetSize()` moved out of the board data file into the core
  `rs_bb_uart_tx.c`.** The reference project's `rs_rtc_logger_ConfigGetSize()`
  takes a `p_ctrl` argument (for a generic `sizeof(*p_ctrl)` trick that
  works for any board). Since our control block is a compile-time-fixed
  singleton with no caller-visible type, `ConfigGetSize(size_t *p_size)`
  needs no argument and no board-specific knowledge, so it lives directly
  in `rs_bb_uart_tx.c` instead. `ConfigGen()`/`ConfigCheck()` remain in the
  board data file since they genuinely are board-specific (GPT preset
  instance, pin mapping).

## 8. Cycle 4 -- GPT driver resolution using a real reference project

The user uploaded `VSCode_FPB_RA4E2_Baremetal.zip`: a real RASC-generated
FPB-RA4E2 bare-metal project that already has a GPT Timer stack (`g_timer0`,
channel 0, disabled interrupt) added, meaning `ra/fsp/src/r_gpt/r_gpt.c`,
`ra/fsp/inc/{api,instances}`, `ra_cfg/fsp_cfg/r_gpt_cfg.h`, and a real
`configuration.xml` GPT module block were all available to cross-reference.
This resolved Section 6's biggest caveat and also surfaced two real defects:

- **Bug found and fixed: missing `ra/fsp/inc/{api,instances}`.** The Cycle 3
  zip's `ra/` copy omitted this folder entirely -- it holds `bsp_api.h`,
  `r_timer_api.h`, `r_ioport_api.h`, `fsp_common_api.h`. This would have been
  a hard build break (every file `#include`-ing `bsp_api.h` would fail to
  resolve). Fixed by wholesale-copying `ra/` from the new reference instead
  of the earlier selective copy.
- **Bug found and fixed: wrong GPT channel-width advice.** Cycle 3's
  guidance was "select a 32-bit GPT channel." **The RA4E2 has zero 32-bit
  GPT channels** (`BSP_FEATURE_GPT_32BIT_CHANNEL_MASK == 0x00` in
  `bsp_feature.h` -- every channel is 16-bit, max 65,535 counts). Undivided
  PCLKD (~100 MHz) would overflow the counter at 1200 baud (needs ~83,333
  counts). Fixed by adding `TIMER_SOURCE_DIV_4` (PCLKD/4, ~25 MHz) to both
  `ra_gen/hal_data.c`'s `g_timer0_cfg` and the PES's own
  `g_timer0_cfg_preset`, keeping `period_counts` within 16-bit range across
  the full 1200-115200 baud range while retaining ~217 counts of resolution
  at 115200 baud.
- **`gpt_extended_cfg_t` / `timer_cfg_t` field-completeness fixed.** The
  Cycle 3 hand-written preset was missing `compare_match_c/d/e/f_ipl/irq`,
  `compare_match_value[]`, `compare_match_status`, `gtior_setting`,
  `gtioca_polarity`/`gtiocb_polarity` (all zero-initialized by the
  designated-initializer, so it likely still compiled, but didn't match
  real driver expectations), and `timer_cfg_t.duty_cycle_counts` was
  missing entirely. All now copied field-for-field from the verified real
  reference.
- **GPT driver source now genuinely vendored.** `ra/fsp/src/r_gpt/r_gpt.c`
  is copied directly from the reference (real Renesas-vendored middleware,
  not fabricated) -- the "add a GPT stack via Smart Configurator" README
  instruction from Cycle 3 is gone; nothing further is required to build.
- **Vector table now correctly enables the GPT interrupt, non-speculatively.**
  Traced `BSP_PRV_VECT_ENUM(event, group)` to its definition in
  `bsp_elc.h`: it macro-expands to plain `ELC_ ## event` -- the `group`
  parameter is decorative only, with no effect on the compiled value. For a
  project with exactly one interrupt (index 0), there is no IELSR
  group-slot allocation left to guess. `ra_gen/vector_data.h/.c` now
  declare/define this properly (`VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW`,
  `gpt_counter_overflow_isr`, `ELC_EVENT_GPT0_COUNTER_OVERFLOW`) instead of
  the Cycle 3 empty placeholder.
- **`configuration.xml` now included** (omitted entirely in Cycle 3). Copied
  from the reference and patched (all 5 targeted string replacements
  verified to match exactly once): `module.driver.timer.ipl` enabled at
  priority 6, `module.driver.timer.p_callback` set to
  `rs_bb_uart_tx_hal_gpt_callback`, and P411 reassigned from its default
  "Peripheral: SCI0 TXD0" to "GPIO Output (high)" (three XML edits -- altId,
  gpio_mode, and removing the orphaned `sci0.txd0.p411` cross-reference)
  since this PES bit-bangs the frame and never touches the SCI0 hardware
  channel. This matters beyond documentation: if a user ever regenerates
  via RASC without this patch, P411 would silently revert to a UART
  peripheral pin and break the bit-banged output -- now it won't.
- **`ra_gen/main.c` simplified** to match the real, much simpler RASC
  bare-metal template (`int main(void) { hal_entry(); return 0; }` -- no
  `g_hal_init()` call needed/present in genuine bare-metal output).
- **`bsp_cfg.h` cross-checked**: confirmed `BSP_CFG_RTOS`'s
  `RA_NOT_DEFINED`-comparison idiom is genuine RASC bare-metal template
  output (not something to simplify away as I'd assumed in Cycle 3) and
  correctly evaluates to 0; kept it verbatim. Re-applied this project's
  intentional deviations on top (`BSP_CFG_STACK_MAIN_BYTES` 0x400->0x800,
  `BSP_CFG_HEAP_BYTES` 0->0x400, `BSP_CFG_PARAM_CHECKING_ENABLE` 0->1) with
  comments explaining why.

**Net effect:** the project now builds without any Smart Configurator step.
The only remaining "verify before flashing real hardware" item is the usual
one for any new timing-sensitive PES -- confirm actual bit timing on a
scope/logic analyzer per Architecture & Design Section 21.2 -- not a gap in
this delivery.

## 9. Cycle 5 -- e2 studio integration + full Doxygen audit

The user uploaded `e2_BitBangedUART_TX_FPB_RA4E2.zip`: a real e2
studio-generated project (`.project`/`.cproject`/`.settings`, FSP 6.5.0,
LLVM Embedded Toolchain for Arm) already named for this PES, with a GPT
Timer stack present (channel 0, interrupt disabled by default -- same
"blinky + unconfigured GPT" template state as the Cycle 4 VSCode
reference). Two things were done this cycle:

**1. Integrated the full PES into this e2 studio project as the new
delivery base** (superseding the VSCode/CMake project from Cycles 1-4,
which is no longer the delivered artifact):

- Diffed this project's `ra_gen/hal_data.c`, `vector_data.h`, `pin_data.c`,
  `bsp_cfg.h` against the Cycle 4 VSCode reference's equivalents (ignoring
  line endings/whitespace) and confirmed the FSP-generated content is
  functionally byte-for-byte identical -- only e2 studio's code formatter
  differs from RASC's raw template output. This meant the Cycle 4 patches
  (verified field-by-field against real RASC output) could be transplanted
  directly rather than re-derived.
- Copied `ra_gen/{hal_data.c,hal_data.h,vector_data.h,vector_data.c,
  pin_data.c,common_data.c,common_data.h,main.c}` and
  `ra_cfg/fsp_cfg/bsp/bsp_cfg.h` from the Cycle 4 project into this one.
- Patched this project's own `configuration.xml` with the same 5 targeted
  string replacements as Cycle 4 (GPT `ipl` enabled at priority 6,
  `p_callback` set to `rs_bb_uart_tx_hal_gpt_callback`, P411 reassigned
  from SCI0-peripheral to GPIO-output across 3 edits) -- every match
  count verified to be exactly 1 before applying; XML re-validated after.
- Confirmed via `.cproject`'s `<sourceEntries>` that `src` (the whole
  folder, recursively) is already a registered Eclipse CDT managed-build
  source path -- new files under `src/rs_bb_uart_tx/**` are auto-discovered
  on the next build with no `.cproject`/makefile edits needed.
- Copied the full `src/rs_bb_uart_tx/` PES tree, `rs_bb_uart_tx_demo.c`,
  and `fault_handler.c` in; replaced the generic blinky `hal_entry.c` with
  the PES-calling version; confirmed `hal_warmstart.c` is byte-identical
  to the already-verified Cycle 4 version (left untouched -- genuine
  Renesas-provided boilerplate, not PES code).
- Excluded `Debug/` (prior build output/artifacts, ~1.8 MB, fully
  regenerable) and `JLinkLog.log` (transient debug session log) from the
  delivery; kept `.jlink`/`.launch` (debug/flash configs) and `.clangd`
  (LLVM IDE tooling config) since those give the user a working
  double-click-to-import, double-click-to-debug project.
- Wrote a fresh `README.md` for the e2 studio workflow (Eclipse import,
  Project -> Build, the included `.launch` debug config) -- no prior README
  existed in the e2 studio upload (RASC's e2 studio flow doesn't generate
  one; that was specific to the earlier VS Code/CMake project).

**2. Full Doxygen audit, per explicit user request** ("make sure to
include in the source doxygen style comment blocks and information for
all functions"). Found that prior cycles had front-loaded documentation
into public headers (standard practice) but left many `.c` file
*definitions* -- especially internal/static helpers -- with little or no
comment block. Added complete `@brief`/`@param` (one per parameter)/
`@return` or `@retval` (one per outcome) blocks directly above every
function *definition*, not just declarations, across:

- `rs_bb_uart_tx_ring_buffer.c` (5 functions)
- `rs_bb_uart_tx_engine.c` (2 functions)
- `rs_bb_uart_tx_hal.c` (8 functions, including the ISR callback and both
  internal-linkage-only GPT/GPIO adapter pairs)
- `rs_bb_uart_tx.c` (6 functions, including both `static`/internal-linkage
  helpers -- `rs_bb_uart_tx_start_internal()` and `rs_bb_uart_tx_open_internal()`)
- `rs_bb_uart_tx_data_fpb_ra4e2.c` (2 functions -- upgraded from a lighter
  existing block to full parameter/retval detail)
- `rs_bb_uart_tx_demo.c` (4 functions -- demo-only, but documented to the
  same standard for consistency)
- `hal_entry.c` (1 function)
- `fault_handler.c` (6 functions -- this file didn't exist in the e2 studio
  upload at all; carried over from Cycle 4 and given a full rewrite with
  complete documentation, including a `@name`-grouped block for the
  fault-register globals)

Verified coverage mechanically: for every file, `grep -c "@brief"` equals
`(function count) + 1` (the file-level `@file`/`@brief` block), confirming
no function was missed. Re-ran the brace/paren structural balance check
after all edits -- 35 files, all balanced.

**Deliberately left undocumented (with reasoning, not an oversight):**
`hal_warmstart.c`'s `R_BSP_WarmStart()` -- genuine unmodified Renesas
boilerplate (already carries a reasonable non-empty Doxygen-style
`@param` block as shipped), not code this PES development authored.

## 10. Cycle 6 -- build error fix: missing `#include "r_ioport.h"`

User built the project in e2 studio and hit:

```
../src/rs_bb_uart_tx/rs_bb_uart_tx_hal.c:221:30: error: use of undeclared identifier 'IOPORT_CFG_PORT_DIRECTION_OUTPUT'
../src/rs_bb_uart_tx/rs_bb_uart_tx_hal.c:221:76: error: use of undeclared identifier 'IOPORT_CFG_PORT_OUTPUT_HIGH'
2 errors generated.
... Build Failed. 4 errors, 0 warnings.
```

**Root cause, verified (not guessed):** these macro *names* are correct --
confirmed against `ra_gen/pin_data.c` in the real
`VSCode_FPB_RA4E2_Baremetal` reference project, where the identical macros
are used for the LED pins (P400/P401) and compile fine there. The bug is a
missing include, not a wrong macro. `IOPORT_CFG_PORT_DIRECTION_OUTPUT` /
`IOPORT_CFG_PORT_OUTPUT_HIGH` are declared in `r_ioport.h`
(`ra/fsp/inc/instances/r_ioport.h`), not in `bsp_api.h` or `r_timer_api.h`.
`rs_bb_uart_tx_hal.c`'s include chain (`rs_bb_uart_tx_hal.h` ->
`rs_bb_uart_tx_ctrl.h` -> `rs_bb_uart_tx_types.h` -> `bsp_api.h` +
`r_timer_api.h`) never touches `r_ioport.h`. `hal_warmstart.c` -- which uses
the same macros and does NOT error -- only works by accident, via an
unrelated chain: `hal_data.h` -> `common_data.h` -> `r_ioport.h`.

**Fix:** added `#include "r_ioport.h"` directly to `rs_bb_uart_tx_hal.c`
(the .c file, not the .h -- nothing in the header itself references these
macros, only the `rs_bb_uart_tx_hal_gpio_open()` function body), with a
comment explaining why the include is there so it doesn't look redundant
and get "cleaned up" later.

**Verification done before declaring this fixed:**
- Grepped every PES source file for `IOPORT_CFG_` usage -- confirmed only
  `rs_bb_uart_tx_hal.c` and `hal_warmstart.c` (Renesas boilerplate,
  untouched, already resolves fine) use these macros. No other file
  affected.
- Grepped every FSP/BSP-prefixed identifier used across
  `rs_bb_uart_tx.c`/`rs_bb_uart_tx_hal.c`/`rs_bb_uart_tx_data_fpb_ra4e2.c`
  and checked each resolves through an actual (not lucky-transitive)
  include: `rs_bb_uart_tx_data_fpb_ra4e2.h` directly includes `r_gpt.h`/
  `r_timer_api.h` (all its GPT-specific macros are covered);
  `BSP_FEATURE_GPT_32BIT_CHANNEL_MASK` and `BSP_IO_PORT_04_PIN_11` only
  ever appear inside comments in that file, not real code, so they have no
  compile dependency. `BSP_IO_LEVEL_HIGH/LOW` and `FSP_SUCCESS` in
  `rs_bb_uart_tx_hal.c` were not flagged by the compiler, confirming they
  already resolve fine via `bsp_api.h`.
- Re-checked brace/paren balance on the edited file after the fix.

**Known gap in this fix:** the user's build log summary said "4 errors,
0 warnings" but only 2 individual error lines are visible in the pasted
log (both at the same line, one per macro) -- the pasted file may be
truncated, or e2 studio's console may have interleaved output from the
parallel (`-j14`) build in a way that cut off two more lines. Since the
same two macros are used in exactly one other place (`hal_warmstart.c`,
which is already fixed by its own working include chain) I could not
identify a second file that would independently produce 2 more identical
errors. If a full rebuild still shows errors after this fix, get the
complete unredacted log rather than assuming this fix is incomplete.

## 11. Cycle 7 -- root cause of "silence despite success": R_BSP_PinWrite() needs PFS access enable/disable per call

This is the fix for the multi-message debugging chain that started with
"ConfigGen returns success, ConfigCheck returns success, but nothing
prints to Tera Term."

**Diagnostic path that got there** (documented in detail because it's a
good template for the next hardware bring-up bug):
1. Ruled out buffer sizing (banner is 304 bytes vs. 256-byte default --
   real secondary bug, fixed, but didn't change the symptom).
2. Ruled out the GPT/vector/clock-divider plumbing via direct inspection of
   `r_gpt.c` and `bsp_irq.c` source -- confirmed `R_GPT_Open()`'s
   `cycle_end_irq >= 0` guard passing (proving the vector is wired),
   confirmed `gpt_clock_frequency_get()` reads the actual hardware
   prescaler register (proving the /4 divider math is sound).
3. Isolated hardware from software with a raw busy-wait bit-bang test on
   P411 (no GPT, no interrupts, no PES code at all) -- still silent.
4. Isolated "is code running at all" with an LED blink test (P207, no
   COM port/VCOM dependency at all) -- LED turned on, appeared to never
   toggle off.
5. Investigated `R_BSP_SoftwareDelay()`'s actual busy-wait implementation
   and `SystemCoreClock` -- both confirmed correct (100 MHz, matching
   PCLKD/ICLK per `bsp_clock_cfg.h`'s /2 divider off the 200 MHz PLL -- my
   own initial "should be 200 MHz" comment to the user was wrong and
   corrected).
6. Confirmed via register inspection (`r0` in the Registers/Expressions
   view, decrementing across a timed pause/resume) that the CPU was
   genuinely executing the delay loop, not stuck/faulted.
7. **User found it empirically**: wrapping every `R_BSP_PinWrite()` call
   in `R_BSP_PinAccessEnable()`/`R_BSP_PinAccessDisable()` (not just the
   one-time `R_BSP_PinCfg()` call) fixed the LED blink test.

**Root cause, verified against the real header (not just taking the
empirical fix on faith):** `ra/fsp/src/bsp/mcu/all/bsp_io.h`'s own doc
comment above `R_BSP_PinWrite()` states: *"If PFS protection is enabled,
disable PFS protection using R_BSP_PinAccessEnable() before calling this
function."* Looking at the actual implementation, `R_BSP_PinWrite()` on
this FSP/MCU combination writes directly to the same write-protected
`PmnPFS` register that `R_BSP_PinCfg()` uses -- NOT a separate
always-writable output-data register (a design that differs from some
other Cortex-M GPIO models, which is why I didn't anticipate it). With
`BSP_CFG_PFS_PROTECT` enabled (the project default), a bare
`R_BSP_PinWrite()` call after protection has been re-locked is **silently
dropped by the PWPR write-protect hardware** -- no error, no fault, the
pin just never changes again after whatever `rs_bb_uart_tx_hal_gpio_open()`
last wrote before calling `R_BSP_PinAccessDisable()`.

**This is exactly why the original PES symptom looked the way it did:**
`rs_bb_uart_tx_hal_gpio_open()` correctly brackets its one-time
`R_BSP_PinCfg(...OUTPUT_HIGH)` call in Enable/Disable, so the pin
correctly goes to idle-high. But `rs_bb_uart_tx_hal_gpio_write()` -- called
on *every single GPT ISR tick* to actually bit-bang the UART frame -- used
a bare `R_BSP_PinWrite()` with no Enable/Disable around it. Every write
after the first was silently rejected by the hardware. The pin never
moved again. `ConfigGen()`/`ConfigCheck()` still reported SUCCESS
correctly (they only validate that HAL *open* succeeded, which it did) --
there was never a code path that could have detected this at the software
level, since the write-protect rejection is invisible to the calling code
(no error return, no fault, nothing -- the hardware just ignores the
write).

**Fix applied** to `src/rs_bb_uart_tx/rs_bb_uart_tx_hal.c`:
- `rs_bb_uart_tx_hal_gpio_write()` -- now wraps its `R_BSP_PinWrite()` call
  in `R_BSP_PinAccessEnable()`/`R_BSP_PinAccessDisable()`. This is the
  fix that actually matters -- this function runs on every bit period.
- `rs_bb_uart_tx_hal_gpio_close()` -- same fix applied for consistency,
  even though not currently called by any Phase 1 code path.

**Confirmed safe for ISR context, not just convenient:** read
`R_BSP_PinAccessEnable()`/`Disable()`'s actual implementation in
`bsp_io.h` -- they're a small reference-counted pair wrapped in
`FSP_CRITICAL_SECTION_ENTER/EXIT`, and the function-level doc comment
explicitly states they "protect against interrupts that could occur via
multiple threads **or an ISR re-entering this code**." This is exactly
the GPT ISR context `rs_bb_uart_tx_hal_gpio_write()` runs in, and the
mechanism is designed for it. Overhead per call is a handful of register
writes inside a brief critical section, not a blocking operation -- at
115200 baud (worst case, one call every ~8.68 microseconds), this is not
expected to threaten bit timing, but this has not yet been independently
re-verified on a scope after the fix (see open item below).

**CONFIRMED WORKING (Cycle 7, same session):** user rebuilt/reflashed with
this fix, restored `hal_entry.c` to call the real
`rs_bb_uart_tx_demo_run()` (not a diagnostic stub), set Tera Term back to
115200 8N1, and confirmed the banner + heartbeat output now appears
correctly. This closes out the "everything reports success but nothing
prints" investigation -- root cause and fix both verified end-to-end, not
just theorized.

**Still open (lower priority, not blocking):** bit timing has not been
independently verified on a scope/logic analyzer per Architecture &
Design Section 21.2's own validation strategy. The ISR now does slightly
more work per tick (the Enable/Disable pair around every pin write) than
originally designed; functionally confirmed working via Tera Term at
115200, but a scope check would confirm timing accuracy stays within the
documented +/-3% baud-rate-error / +/-10% jitter targets at the high end of
the baud range, especially if the baud rate is ever pushed higher than
115200 in a future revision.

**Hardware constraint discovered (same session, via hands-on testing):**
the on-board J-Link OB debug probe's VCOM bridge does not support baud
rates below 9600 on this board. This is a constraint of the debug
probe's VCOM firmware, not the PES -- `rs_bb_uart_tx_ConfigCheck()` still
correctly accepts and configures the full documented 1200-115200 range
(Section 5.4), and the GPT divider math is still correctly sized for it.
The limitation is specifically in relaying the signal through the
on-board bridge to a USB CDC port for Tera Term to open. Practical
consequence: validating the low end of the range (1200-4800 baud) via
Tera Term through the default on-board VCOM path is not possible on this
board; a logic analyzer/scope on the TX pin directly, or an external
TTL-to-USB adapter on a different GPIO (bypassing the on-board bridge
entirely), are the two ways to actually observe/validate 1200-4800 baud
operation. Documented in README.md's new "On-board J-Link VCOM Bridge
Does Not Support the Full Baud Range" section. This is a good thing to
flag explicitly in the Phase 4 "Document & Package" deliverables later,
since the Confluence acceptance doc's timing-tolerance validation
requirement (Section 6.2: "This shall be validated on the Targets under:
Idle system conditions...") doesn't currently call out this VCOM-bridge
caveat, and a future validator following that doc verbatim could get
confused trying to open a 1200 baud Tera Term session against the default
on-board VCOM path.

**`buffer_full_count` confirmed and fixed (same session):** user observed
`full=2` in the demo's status line via `StatusGet()`, correctly traced it
to the same 256-vs-304-byte buffer/banner mismatch flagged earlier in
Cycle 7 (the buffer-size suggestion made at the time of the original
silence investigation -- which didn't fix that bug, since the real cause
was the pin-write issue, but was still a real secondary bug in its own
right). Confirmed empirically this time: bumping
`RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` 256 -> 512 eliminated the `full`
count entirely. **Applied to the shipped default** in
`rs_bb_uart_tx_cfg.h` this cycle (not just documented) -- a fresh checkout
of this project now defaults to 512 bytes, so this bug does not
re-surface for anyone building from a clean copy of the delivered zip.
Per-finding documentation added to both `README.md` (new
"Troubleshooting" section) and the new `docs/README.md` (re-created this
cycle -- was present in the earlier CMake/VS Code project from Cycle 3 but
never carried over during the Cycle 5 e2 studio integration; now exists
as a running "Notes for the Phase 4 documentation pass" list, seeded with
this finding and the VCOM-bridge baud limitation above).

## 12. Cycle 8 -- port to FPB-RA6E2 (R7FA6E2BB3CFM)

User uploaded `e2_Bit_Banged_UART_TX_FPB_RA6E2_Ref.zip`: a real e2
studio-generated bare-metal + GPT reference project for FPB-RA6E2 (same
"blinky + GPT stack present but disabled" template pattern as the
FPB-RA4E2 references from earlier cycles), specifically to make this port
easier. **New delivery directory: `BitBangedUART_TX_FPB_RA6E2/`**
(FPB-RA4E2's `BitBangedUART_TX_FPB_RA4E2/` remains the separate, still-valid
delivery for that board -- this is an additional board, not a replacement).

**Hardware facts independently re-verified for RA6E2, not assumed from
RA4E2** (methodology: same as Cycle 4/5's "verify against real files,
don't guess" discipline):
- `BSP_FEATURE_GPT_32BIT_CHANNEL_MASK` in
  `ra/fsp/src/bsp/mcu/ra6e2/bsp_feature.h`: `0x00` -- same as RA4E2, no
  32-bit GPT channels, PCLKD/4 divider strategy still required.
- `ra_gen/bsp_clock_cfg.h`: HOCO 20 MHz x PLL 10.0 = 200 MHz system clock,
  `PCLKD_DIV = /2` -> PCLKD = 100 MHz -- same as RA4E2 (note: `ICLK_DIV` is
  `/1` here vs. RA4E2's `/2`, so ICLK differs -- 200 MHz vs 100 MHz -- but
  this doesn't affect GPT timing since GPT clocks from PCLKD, not ICLK;
  only affects `R_BSP_SoftwareDelay()`'s loop-count math, which
  self-adjusts via `SystemCoreClock` regardless).
- `configuration.xml`'s default pin assignment: P411
  (`BSP_IO_PORT_04_PIN_11`) -> SCI0 TXD0, same physical pin identifier as
  RA4E2 (confirmed via `p411.sci0.txd0` in this board's own reference
  project, not assumed).
- `ra/board/ra6e2_fpb/board_leds.c`: LED1 = `BSP_IO_PORT_02_PIN_07` (P207)
  -- same as RA4E2.
- `ra/fsp/src/bsp/mcu/ra6e2/bsp_elc.h`: `ELC_EVENT_GPT0_COUNTER_OVERFLOW = 0x0C6`
  -- same numeric value as RA4E2.
- Device: R7FA6E2BB3CFM, FSP 6.5.0 (confirmed via `configuration.xml`'s
  `#TargetName#`/`#DeviceCommand#` properties), LLVM toolchain.

**Because every one of these facts happened to match FPB-RA4E2 exactly**,
the already-verified `gpt_extended_cfg_t`/`timer_cfg_t` struct layout,
the `TIMER_SOURCE_DIV_4` divider math, and the
`VECTOR_NUMBER_GPT0_COUNTER_OVERFLOW`/vector-table derivation all carried
over to this board with only naming/board-string substitution -- not
re-derived from scratch. This is **not** assumed to generalize to a third
board (FPB-RA2E3) without the same independent verification -- see
`docs/README.md`'s new "Porting to a new board" note for exactly which
facts must be re-checked vs. which code carries over unchanged.

**What was ported / how:**
- New project scaffolded from the user's uploaded reference (not from
  scratch): `.project`/`.cproject` project-name references fixed
  throughout (`sed` substitution + manual `.project` `<name>` fix),
  `Debug/`/`JLinkLog.log` excluded as build output.
- Board-agnostic PES core (`src/rs_bb_uart_tx/` -- 11 files) copied
  unchanged, including the Cycle 7 `R_BSP_PinWrite()` PFS-access fix and
  the Cycle 7/8 512-byte buffer default -- neither needed rediscovery since
  neither is board-specific.
- New `rs_bb_uart_tx_data_fpb_ra6e2.h/.c` created (adapted from the
  verified RA4E2 version via naming substitution; hardware facts
  documented above, not re-guessed).
- `ra_gen/hal_data.c/.h`, `vector_data.h/.c` copied from the verified
  RA4E2 versions (field-for-field identical struct layout applies, since
  all the facts it depends on matched) with board-name substitution only.
- `ra_gen/main.c` -- confirmed the RA6E2 reference's own default bare-metal
  template is functionally identical to RA4E2's (diffed to confirm),
  replaced with the documented version for consistency.
- `ra_cfg/fsp_cfg/bsp/bsp_cfg.h` copied from the verified RA4E2 version
  (RTOS=0 idiom, RTC_USED=0, stack/heap sizing, PARAM_CHECKING=1) with
  comment substitution only -- confirmed the RA6E2 reference's own default
  bsp_cfg.h uses the identical `BSP_CFG_RTOS` idiom and the same
  RTOS=0/PARAM_CHECKING=0 baseline before adaptation, so no board-specific
  clock/RTOS values needed changing.
- `ra_gen/pin_data.c`: **kept this reference's full board pin table**
  (a change from the RA4E2 approach, which used a minimal custom table) --
  this reference's table is genuine, complete, working RASC output
  (includes LEDs etc. pre-configured as a bonus), so only the one P411
  entry was surgically replaced (SCI0-peripheral -> GPIO-output-high),
  verified via exact string match (1 match, applied cleanly) rather than
  a blind rewrite.
- `configuration.xml`: same 5-property patch as RA4E2 (GPT `ipl` enabled
  at priority 6, `p_callback` wired, 3-part P411 pin reassignment) --
  every match count verified to be exactly 1 before applying, applied
  fresh against this board's own XML (not copied from RA4E2's already-patched
  file), XML re-validated after.
- `common_data.c/.h` and `src/hal_warmstart.c` confirmed identical
  (diffed) to the verified RA4E2 versions -- no changes needed. Noted in
  passing: this reference's own genuine blinky `hal_entry.c` already
  correctly used the `R_BSP_PinAccessEnable()`/`Disable()` pattern around
  its `R_BSP_PinWrite()` loop -- good independent corroboration of the
  Cycle 7 fix, from a source that predates this project's need to
  discover it.
- `src/hal_entry.c` and `src/rs_bb_uart_tx_demo.c` -- RA4E2 versions
  copied/generated with board-name substitution (`sed` for the demo file,
  direct copy for hal_entry.c since it has no board-specific content).
- `src/fault_handler.c` copied unchanged (board-agnostic Cortex-M33 fault
  handling).

**Investigated and resolved: missing `memory_regions.lld`/`fsp_gen.lld`.**
Initially flagged as a blocking gap (`script/fsp.lld` has
`INCLUDE memory_regions.lld` / `INCLUDE fsp_gen.lld`, and neither file
existed anywhere in the delivered project after excluding `Debug/`).
Resolved by finding both files inside `Debug/` in **both** the RA4E2 and
RA6E2 reference uploads -- confirming they are build-time generated by e2
studio's Renesas-specific pre-build content generator
(`com.renesas.cdt.ddsc.contentgen.ddscBuilder`, confirmed present in
`.project`'s builder chain, runs before the standard
`genmakebuilder`/compile step), not static source files that need to be
shipped or version-controlled. This also retroactively explains why the
FPB-RA4E2 project has built successfully for the user throughout Cycles
5-7 despite these two files never being in any delivered zip -- e2 studio
regenerates them fresh on every build. Not a real gap; corrected the
"blocking issue" framing from the previous message before packaging.

**Documentation adapted, not just copied:**
- `README.md`: full board-name substitution, device part number
  (R7FA6E2BB3CFM) added explicitly, and -- importantly -- the VCOM
  baud-rate-limitation claim was **correctly rescoped**, not blindly
  copied: the original RA4E2 wording asserted the 9600 baud floor as
  fact; since that was confirmed via hands-on testing on RA4E2 hardware
  specifically and has NOT been independently tested on physical RA6E2
  hardware, the RA6E2 README now says "may not support" / "confirmed on
  FPB-RA4E2... not yet independently re-confirmed on FPB-RA6E2" and asks
  the user to report back once tested, rather than overclaiming.
- `docs/README.md`: same board-name substitution, PLUS caught and fixed a
  sed-substitution side effect where a historical "how we discovered
  this" narrative (buffer 256 -> 512) had its illustrative "before" code
  block accidentally rewritten to already show 512, breaking the
  "Change: ... to a larger value" instructional flow -- rewritten by hand
  to correctly state that FPB-RA6E2's port never hit this bug at all
  (started from the already-corrected default), while preserving the
  general portable guidance for future readers. Also added a new Section 3
  ("Porting to a new board") capturing exactly which facts must be
  re-verified vs. which code carries over, directly useful for the next
  port (FPB-RA2E3).

**Structural QA:** 35 files checked (brace/paren balance) across
`src/`, `ra_gen/`, `ra_cfg/` -- all balanced. `configuration.xml`
re-validated as well-formed XML after patching.

**`memory_regions.lld`/`fsp_gen.lld` theory independently confirmed
(user follow-up, this session):** user uploaded `Debug.zip`, real build
output from a build of the *original Blinky reference project* (not the
ported PES project -- `.elf` is named `BitBangedUART_TX_FPB_RA6E2_Blinky.elf`,
confirmed explicitly by the user when asked). `memory_regions.lld` opens
with the literal comment `/* generated memory regions file - do not
edit */` (same convention as `ra_gen/*.c`), and its content (RAM
0x20000000/0xa000 bytes, FLASH 0x0/0x40000 bytes, etc.) is purely derived
from the R7FA6E2BB3CFM device selection -- nothing project-specific.
`fsp_gen.lld` is the corresponding generic `MEMORY`/`SECTIONS` linker
script. Confirmed neither delivered project (RA4E2 or RA6E2) ships a
static copy of either file. This fully corroborates the Cycle 8 finding --
e2 studio's `ddscBuilder` regenerates both fresh every build.

**CONFIRMED WORKING ON REAL FPB-RA6E2 HARDWARE (same session, next
user follow-up):** user then built and ran the actual ported project --
banner + heartbeat visible in Tera Term. This closes out the "not yet
built or flashed" caveat above. **The Bit-Banged UART TX PES is now
functionally verified working on both FPB-RA4E2 and FPB-RA6E2.**

**Real gap found in the `configuration.xml` patch, via this hardware
bring-up:** to get there, the user had to open the FSP Configurator GUI
and manually disable SCI0's "Operation Mode" -- RASC was flagging a pin
conflict on P411 between the reassigned GPIO-output and SCI0's still-active
mode/pairing configuration. Root cause identified in the delivered
`configuration.xml`: the original 5-property patch (Cycle 8, reused from
the FPB-RA4E2 patch) removed only the `sci0.txd0.p411` cross-reference
line, but left `sci0.mode.simplespi.b` (line 448) and `sci0.pairing.b`
(line 449) untouched -- these still declare SCI0 as configured for Simple
SPI mode with an active pairing, which is apparently NOT inert metadata
the way it looked (contrary to the Cycle 8 assumption that the whole
`sci0.*` block was harmless catalog metadata since no active SCI0 stack
exists in the module list) -- it's enough for RASC's conflict checker to
flag P411 as contested between SCI0's pairing and the PES's GPIO
reassignment.

**Not yet fixed in the delivered file -- need one more piece of
information to do this correctly rather than guess:** asked the user to
share the `sci0.*` lines from their working, regenerated
`configuration.xml` (post-GUI-fix) so the exact correct property/value
can be applied here, instead of guessing at RASC's XML syntax for
"Operation Mode: Disabled" the way earlier cycles learned not to do for
`gpt_extended_cfg_t` fields. Once known, patch `configuration.xml`
(and cross-check whether the equivalent FPB-RA4E2 delivery has the same
latent gap -- likely yes, same original patch reused) and note in
`README.md`'s "GPT Interrupt Setup" section that this SCI0 conflict is a
known one-time GUI step if regenerating via the Smart Configurator,
until the XML patch itself is corrected to avoid it.

## CONFIRMED FIX APPLIED (same session, user provided working configuration.xml)

User uploaded their actual regenerated, hardware-tested `configuration.xml`
(post-GUI-fix, the one that builds and runs on real FPB-RA6E2 hardware).
Diffed directly against the delivered file rather than guessed. Exact,
complete diff:

**Removed (10 lines, verified via exact string match, each matched
exactly once):**
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
**Kept:** `<configSetting altId="sci0.pairing.b" configurationId="sci0.pairing"/>`
(the only `sci0.*` line remaining in the confirmed-working file).

**Real mechanism, now understood precisely (not just "disable Operation
Mode" as a black box):** SCI0's default Simple SPI mode claims a 3-pin
pairing -- P411 (TXD0), P410 (RXD0), P102 (SCK0) -- as a group, independent
of whether an SCI0 stack appears in the module list. The original patch
only removed the P411 side of that pairing (`sci0.txd0.p411`), leaving
the mode itself and the other two pins' peripheral assignments intact --
enough for RASC's conflict checker to still flag P411 as contested.

**Applied to both deliveries** (confirmed FPB-RA4E2 had the identical
latent gap before fixing -- same original patch, never triggered there
since the user never attempted a RASC regeneration on that board):
- `configuration.xml` in both `BitBangedUART_TX_FPB_RA6E2/` and
  `BitBangedUART_TX_FPB_RA4E2/`: the 10 lines above removed, verified
  exactly 1 match each before applying, both files re-validated as
  well-formed XML after.
- `ra_gen/pin_data.c` in `BitBangedUART_TX_FPB_RA6E2/` only: P102
  (`BSP_IO_PORT_01_PIN_02`) and P410 (`BSP_IO_PORT_04_PIN_10`) changed
  from `IOPORT_CFG_DRIVE_HIGH | IOPORT_CFG_PERIPHERAL_PIN |
  IOPORT_PERIPHERAL_SCI0_2_4_6_8` to `IOPORT_CFG_PORT_DIRECTION_INPUT`
  (matching the pattern used elsewhere in that same file for genuinely
  unconfigured pins), with a comment explaining why. **FPB-RA4E2's
  `pin_data.c` needed no change** -- it was already a minimal custom table
  containing only the P411 entry (written that way since Cycle 4), so it
  never carried the P410/P102 peripheral assignments this fix removes.
- `README.md` in both projects: "GPT Interrupt Setup" section gained a
  3rd numbered item explaining the SCI0 pairing mechanism and that this
  fix is confirmed via real hardware bring-up, not just inferred.
- `docs/README.md` in both projects: new Section 4 (RA6E2) / Section 3 (RA4E2) with the
  full technical write-up for the eventual Phase 4 documentation pass.
  Also caught and fixed two stale details found while touching these
  files: FPB-RA4E2's `docs/README.md` still described the ring-buffer
  default as 256 (it's been 512 since Cycle 7 -- the doc just never got
  updated when the code did), and RA6E2's "porting to a new board" note's
  claim of "the 5-property patch" was corrected to reflect the actual
  15-property patch now that the full fix is known.

**Structural QA:** both projects re-checked (brace/paren balance across
all `.c`/`.h` files, XML well-formedness) after all changes -- clean.

**Status:** this closes out the SCI0 conflict as a genuinely fixed,
verified item in both deliveries -- not a documented workaround. A fresh
"Generate Project Content" from either delivered `configuration.xml`
should no longer flag this conflict at all.

## 13. Cycle 9 -- port to FPB-RA2E3 (R7FA2E3073CFL), TX pin P109

User uploaded `BitBangedUART_TX_FPB_RA2E3_Blinky_03.zip`: a real e2
studio bare-metal + GPT reference project for FPB-RA2E3, with explicit
instruction that the TX pin should be **P109** (not the P411 used on the
other two boards). New delivery directory: `BitBangedUART_TX_FPB_RA2E3/`.

**This board turned out to differ from FPB-RA4E2/FPB-RA6E2 on every
single hardware fact checked** -- a useful confirmation that the Cycle 8
"verify per board, don't assume" discipline was correct, not excessive
caution:

| Fact | FPB-RA4E2 / FPB-RA6E2 | FPB-RA2E3 |
|---|---|---|
| Device | R7FA4E2B9 / R7FA6E2BB3CFM | R7FA2E3073CFL |
| PCLKD | ~100 MHz (HOCO 20 MHz x PLL 10.0, /2) | **48 MHz** (HOCO 48 MHz direct, no PLL, /1) |
| GPT ch.0 32-bit? | No (`0x00`) | **Yes** (`0x01`) |
| GPT divider needed | Yes, `TIMER_SOURCE_DIV_4` | **No**, `TIMER_SOURCE_DIV_1` |
| Resolution at 115200 baud | ~217 counts | **~417 counts** (better) |
| Default TX pin pairing | SCI0, Simple SPI, 3-pin (TXD0/RXD0/SCK0) | **SCI9, Simple I2C, 2-pin** (TXD9/RXD9) |
| Pairing-fix leftover property | 1 kept (`sci0.pairing.b`) | **0 kept** (no pairing selector exists for this mode) |
| `ELC_EVENT_GPT0_COUNTER_OVERFLOW` | `0x0C6` | **`0x04A`** (harmless -- referenced symbolically) |
| LED1 pin | P207 | **P213** |

**GPT clock divider -- correct engineering decision, not a copy-paste:**
computed fresh for this board's actual clock tree rather than reusing
`TIMER_SOURCE_DIV_4`. At 48 MHz undivided, even 1200 baud needs only
~40,000 counts -- comfortably within 16-bit range (let alone the 32-bit
channel available here) -- so no divider is needed at all, and leaving it
undivided gives strictly better timing resolution than the other two
boards. Applied to both the board config data file's preset instance and
`ra_gen/hal_data.c`'s Configurator-style instance, with the reasoning
documented in-place in both, not just asserted.

**SCI-pairing conflict -- applied the fix proactively and correctly the
first time**, learning directly from the Cycle 8 mistake (where only
`sci0.txd0.p411` was removed initially, missing 9 other lines that also
needed removing, discovered only after the user hit a build conflict).
This time, searched `configuration.xml` for the *complete* `p109.*`/
`p110.*`/`sci9.*` block before writing any patch:
```
<configSetting altId="p109.sci9.txd9" configurationId="p109"/>
<configSetting altId="p109.gpio_mode.gpio_mode_peripheral" configurationId="p109.gpio_mode"/>
<configSetting altId="p110.sci9.rxd9" configurationId="p110"/>
<configSetting altId="p110.gpio_mode.gpio_mode_peripheral" configurationId="p110.gpio_mode"/>
<configSetting altId="sci9.mode.simplei2c.free" configurationId="sci9.mode"/>
<configSetting altId="sci9.rxd9.p110" configurationId="sci9.rxd9"/>
<configSetting altId="sci9.txd9.p109" configurationId="sci9.txd9"/>
```
7 lines removed (P109's own 2 lines *replaced* with the GPIO-output-high
equivalent, not just removed), 0 kept -- genuinely different from the
SCI0 fix's 10-removed/1-kept shape, confirming this really was
independently derived from this board's own file rather than adapted by
pattern-matching the RA6E2 fix's line count. Applied to both
`configuration.xml` and `ra_gen/pin_data.c` (P110 -> unconfigured input,
matching the established pattern) in the same pass -- not left as a
"discovered after a build failure" fix this time.

**Mechanical execution notes:**
- Same CRLF/LF line-ending pitfall hit again when patching
  `configuration.xml` (this board's file uses LF, not CRLF, for the
  section being patched) -- caught immediately via the same "verify match
  count == 1 before trusting the patch" discipline established in Cycle
  8, not a repeat of a real mistake, just the same known gotcha requiring
  the same careful byte-level check every time.
- `common_data.c/.h` and `src/hal_warmstart.c` confirmed identical
  (diffed) to the FPB-RA6E2 versions -- no board-specific content, no
  changes needed, consistent with Cycle 8's finding for that board pair.
- `README.md` required substantial content rewriting, not just name
  substitution -- the GPT Clock Divider and SCI pairing sections describe
  fundamentally different facts for this board (correctly rewritten, not
  sed-substituted, given how much genuinely differs).
- `docs/README.md` initially generated via naive `sed` substitution
  contained real errors worth flagging as a process note: it described
  the SCI0/P411 fix as if it had happened on FPB-RA2E3 (wrong pins, wrong
  SCI channel) and contained a self-referential nonsense line ("worth
  remembering for the next port (FPB-RA2E3)" inside the FPB-RA2E3 doc
  itself). Caught and rewritten by hand before delivery -- a good example
  of why `sed`-substituted documentation still needs a real read-through,
  not just a grep for leftover board names.

**Structural QA:** 35 C files balanced, `configuration.xml` valid XML,
checked after every patching step, not just once at the end.

**Not yet done -- explicitly NOT claimed as working:** this port has
**not been built or flashed on physical FPB-RA2E3 hardware yet**. Unlike
the SCI0/P411 fix (verified against the user's actual working,
regenerated `configuration.xml`), the SCI9/P109 pairing fix here is
applied based on the same proven mechanism and careful reading of this
board's own reference file, but has **not** been independently confirmed
via a real build or RASC regeneration on this specific board -- treat as
"should be correct, following a proven pattern" rather than "confirmed
correct" until tested. Also still open: the VCOM sub-9600-baud limitation
has not been tested on this board either (2-for-2 on the other boards,
not yet 3-for-3).

**Addendum, same session -- two real bugs found in follow-up, both fixed
before this was ever built:** the user asked which GPT interrupt to
enable in the FSP Configurator and reported the tool only offered
priorities 0-3 (0 highest), not the wider range assumed. Investigated
rather than just picking a number that fit:

1. **Confirmed the real cause, not just the symptom:** FPB-RA2E3 uses a
   **Cortex-M23** core (ARMv8-M *Baseline* profile) --
   `ra/arm/CMSIS_6/CMSIS/Core/Include/core_cm23.h` has
   `__NVIC_PRIO_BITS == 2` (4 levels: 0-3), confirmed directly against
   this board's own vendored CMSIS header. FPB-RA4E2/FPB-RA6E2 are
   **Cortex-M33** (ARMv8-M *Mainline*), confirmed `__NVIC_PRIO_BITS == 4`
   (16 levels, 0-15) in their own headers -- priority 6 was genuinely
   valid there, this isn't a case of the earlier boards being wrong.
   **Fixed:** every place `RS_BB_UART_TX_CFG_GPT_IPL` (=6, the shared
   PES-core default) was used to configure real hardware or
   board-specific status metadata for FPB-RA2E3 -- `ra_gen/hal_data.c`'s
   `g_timer0_cfg.cycle_end_ipl`, the board data file's
   `g_timer0_cfg_preset.cycle_end_ipl`, and `ConfigGen()`'s informational
   `p_cfg->irq_priority` field (which must match the real configured
   value or diagnostics would be misleading) -- all hardcoded to `1U`
   instead, each with a comment explaining why the shared macro isn't
   used here. `configuration.xml`'s `module.driver.timer.ipl` property
   changed from `board.icu.common.irq.priority6` to `...priority1` to
   match (single match verified before applying, XML re-validated after).
   Chose 1 (not 0) to preserve the original "medium-high, not the
   absolute maximum" design intent from Architecture & Design Section
   14.3 on this narrower range, leaving priority 0 available for
   anything more critical.

2. **Found independently while investigating the core difference, not
   reported by the user -- a second, more severe bug:** `fault_handler.c`
   (copied verbatim from the Cortex-M33 boards) reads `SCB->CFSR`,
   `SCB->HFSR`, `SCB->MMFAR`, `SCB->BFAR` directly. Checked
   `core_cm23.h`'s actual `SCB_Type` struct: it has only
   `CPUID/ICSR/VTOR/AIRCR/SCR/CCR/SHPR/SHCSR` -- **none of those four
   registers exist on this core** (they're ARMv8-M Mainline-only). This
   would have been a **compile error**, not a runtime issue -- worse than
   the priority mismatch, since the priority issue would likely have only
   caused a Configurator validation warning or a wrong-but-harmless
   runtime value, whereas this would have blocked the build entirely.
   **Fixed:** rewrote `fault_handler.c` for this board -- captures
   `SCB->SHCSR` (System Handler Control and State Register, the closest
   available substitute that does exist on this core) plus the PSP
   (unaffected -- `MRS`-based register read, not an SCB struct member,
   identical on every Cortex-M variant) instead of the four missing
   registers. Kept all 5 handler overrides
   (HardFault/MemManage/BusFault/UsageFault/SecureFault) since the shared
   FSP startup code still declares vector slots for all of them on this
   board too (confirmed via `startup.c`), even though Cortex-M23's
   simplified Baseline exception model is expected to route essentially
   everything through `HardFault_Handler` alone in practice -- documented
   this expectation clearly in the file's comments rather than silently
   dropping the other four overrides.

Both fixes applied directly to the delivered project (not just
documented) before this response. `README.md` updated (GPT Interrupt
Setup section now explains the 0-3 range and why; repository layout
description of `fault_handler.c` corrected from "Cortex-M33" to
"Cortex-M23, adapted"). Structural QA re-run after all four file edits
(`ra_gen/hal_data.c`, the board data file, `configuration.xml`,
`fault_handler.c`) -- balanced, XML valid.

**Still not built or flashed on real hardware** -- this addendum fixes
two real defects found through investigation and reading this board's
actual headers, not through a hardware build failure, so neither fix has
had the equivalent of the FPB-RA6E2 SCI0 fix's "verified against a real
working, regenerated file" confirmation yet.

## 14. Cycle 9 addendum 2 -- FPB-RA2E3 CONFIRMED WORKING (mostly), one real
    finding: startup-only garbled characters, fixed with a host-attach delay

User did a quick build/test of the RA2E3 delivery from the previous
addendum: deleted `fault_handler.c` (their own workaround, before the
Cortex-M23-compatible rewrite in this delivery existed) and manually set
the GPT interrupt priority to 1 in the Configurator (matching the fix
already applied). Result: **the project works** -- heartbeat and status
lines print cleanly -- but the startup banner shows garbled/corrupted
characters before clean output begins at `[000001] Heartbeat OK`.

**Investigated via the real driver source rather than guessed:**
- Checked `R_GPT_PeriodSet()`: when the counter is stopped (`CST == 0`,
  true during `rs_bb_uart_tx_hal_gpt_open()`, called well before the
  first `Start()`), it writes both the buffer (`GTPBR`) and the active
  register (`GTPR`) directly, plus clears the counter (`GTCLR`). Ruled
  out "double-buffered period register only takes effect after first
  overflow" as the cause -- the driver already handles this correctly for
  a stopped counter.
- Checked `R_GPT_Reset()`: only writes `GTCLR` (clears the counter),
  never touches period registers. Ruled out "Reset() clobbers the
  previously-correct period" as the cause.
- **Key observation from the user's own capture:** every heartbeat also
  triggers a fresh Reset()+Start() cycle (the ring buffer drains to empty
  between each 1-second heartbeat, returning to READY, then the next
  `WriteString()` re-triggers READY->TRANSMITTING) -- yet only the very
  first transmission (the banner) is corrupted; all 24 heartbeats and 5
  status lines in the user's capture are clean. A GPT register-sequencing
  bug would affect every restart, not just the first.

**Conclusion: USB VCOM host-side attach-timing artifact, not a PES or
driver bug.** The banner is sent within milliseconds of reset, likely
before the host's USB-CDC stack and Tera Term have fully attached to the
newly-enumerated COM port -- a well-known class of issue with USB virtual
COM ports generally, independent of target hardware correctness. Every
heartbeat after that happens a full second later, well after the USB
connection has settled, which matches the observed pattern exactly.

**Fix applied to all three deliveries** (observed and confirmed on
FPB-RA2E3; applied proactively to FPB-RA4E2/FPB-RA6E2 for consistency
since the same generic USB VCOM mechanism applies to any board using the
on-board VCOM path, not independently observed as broken on those two):
`rs_bb_uart_tx_demo_run()` in `src/rs_bb_uart_tx_demo.c` now calls
`R_BSP_SoftwareDelay(1000, BSP_DELAY_UNITS_MILLISECONDS)` after
`ConfigCheck()` succeeds, before the first `WriteString()` (the banner) --
giving the host time to attach before real data starts flowing. This is
a pure demo-application-level change (not a PES core change) -- delays
when the first byte is sent by 1 second, does not affect actual bit
timing/baud rate accuracy. Documented in all three `README.md`s'
Troubleshooting section, with clear framing on which board this was
actually observed on vs. applied proactively.

**Structural QA:** all three `rs_bb_uart_tx_demo.c` files re-checked for
brace/paren balance after the edit -- clean.

**Not fully closed out:** if the delay doesn't fully resolve this on
re-test, the next diagnostic step would be a scope/logic analyzer
directly on the TX pin during boot -- if the corruption appears on the
wire itself (not just in Tera Term), that would rule out the USB
VCOM-side explanation and point back to something in the GPT/clock path
after all. Not yet needed, but worth keeping in mind if the symptom
persists after this fix.

## 15. Cycle 9 addendum 3 -- REAL root cause found (the delay was wrong):
    enqueue's critical section duration scales with request length, not a
    fixed constant

The 1-second delay from addendum 2 **did not fix the problem** -- user
retested and reported the same garbled banner, ruling out the USB
VCOM-attach-timing theory (1 second is far more than any realistic
attach delay would need). This forced a real re-diagnosis rather than a
bigger delay.

**Re-read the actual `r_gpt.c` driver source a second time, more
thoroughly** -- traced `R_GPT_Open()` (including `gpt_hardware_initialize()`,
not previously inspected), `R_GPT_PeriodSet()`, `R_GPT_Reset()`,
`R_GPT_Start()`, `R_GPT_Stop()`. Found that `Open()` briefly writes a
placeholder period value directly to hardware before the PES's own
`PeriodSet()` call corrects it -- interesting, but traced through
`PeriodSet()`'s logic again and confirmed it does correctly overwrite
both the buffer and active period registers while the counter is
stopped. No clear register-sequencing bug found through this path a
second time either.

**Decisive experiment, proposed and run:** added a disposable "warm-up"
string sent before the real banner, with explicit polling via
`StatusGet()` to confirm it fully drained first. Result reported by
user: **warm-up string printed clean; the real banner (sent immediately
after) was still garbled.** This is the key piece of evidence -- it
definitively rules out "the very first GPT Start() ever is inherently
glitchy," since the warm-up string *was* the actual first Start(), and
it worked fine. Something specific to the banner itself, not to being
first, was the trigger.

**Identified what's actually unique about the banner:** it's the only
place in the demo making ~10 rapid, back-to-back `WriteString()` calls
with zero gap between them. The first call starts the GPT timer; every
call after it enqueues more data *while that timer is already running
and actively transmitting*. Neither the warm-up string nor any
individual heartbeat print does this (each is one isolated call).

**Found the actual mechanism** by reading
`rs_bb_uart_tx_ring_buffer_enqueue()` closely: the entire `memcpy()` of
the requested length happens *inside* the PRIMASK-based critical section
(`__disable_irq()`/`__enable_irq()`), not just index/count bookkeeping.
Critical section duration therefore scales with request length, not a
fixed small constant -- contradicting that file's own header comment at
the time, which claimed critical sections were "intentionally tiny... to
keep worst-case interrupt latency impact negligible." That claim was
true for metadata-only updates but not for the copy itself -- a real gap
between documented intent and actual implementation that had gone
unnoticed until this investigation.

**Why this only manifested on FPB-RA2E3:** its Cortex-M23 CPU runs at
48 MHz (no PLL; HOCO 48 MHz feeds ICLK directly at /1) -- more than 2x
slower than FPB-RA4E2/FPB-RA6E2's Cortex-M33 @ 100 MHz (via PLL). A
single bit period at 115200 baud is ~8.68 us regardless of board (baud
rate defines real time universally, independent of implementation
clock) -- but the *same* critical-section code (same instruction count,
same byte count for a given string) takes more than twice as long in
real time on the slower CPU. A `memcpy()` of a few dozen bytes with
wrap-around handling on a modest Cortex-M23 core can plausibly approach
or exceed 8.68 us; the same code on the 100 MHz Cortex-M33 boards stays
comfortably under it.

**Fix applied to all three deliveries** (not just documented):
- `demo_print_banner()` in all three `src/rs_bb_uart_tx_demo.c` files
  now builds the entire banner into one local buffer via `snprintf()`
  and sends it with a **single** `rs_bb_uart_tx_WriteString()` call,
  instead of ~10 separate calls -- eliminating the specific pattern that
  triggers the bug (a later enqueue landing while an earlier one is
  still transmitting). Applied to FPB-RA2E3 (confirmed necessary) and
  proactively to FPB-RA4E2/FPB-RA6E2 (same underlying mechanism, just
  not independently observed as broken there -- more CPU timing margin).
- The now-known-ineffective 1-second startup delay from addendum 2 was
  **left in place**, not removed -- harmless, possibly still marginally
  useful for genuine USB attach timing in some environment, but
  documented clearly as not having been the actual fix, to avoid
  confusing a future reader into thinking it did something it didn't.
- `rs_bb_uart_tx_ring_buffer.c`'s file-level comment corrected in all
  three deliveries -- replaced the inaccurate "intentionally tiny...
  negligible" claim with an accurate description of the actual scaling
  behavior, a pointer to this finding, and an explicit note that the
  dequeue side (`rs_bb_uart_tx_hal_gpt_callback()`, one byte per ISR
  call) was never affected -- only the enqueue side scales with request
  length.
- `README.md` in all three deliveries: replaced the disproven USB
  VCOM-attach-timing Troubleshooting entry with the correct root cause,
  the fix, and an explicit note that this is a PES-core characteristic
  (not demo-specific) that a real application should be aware of.
- `docs/README.md`: full diagnostic write-up (including the
  disproven-theory trail, since it's a good template for the next
  timing bug) added to FPB-RA2E3 as the primary source; FPB-RA4E2/
  FPB-RA6E2 get a shorter cross-referencing note rather than a full
  duplicate, since this is one PES-core-level finding, not three
  separate per-board ones.

**Explicitly NOT fixed -- flagged clearly, not swept under the rug:** the
PES core itself (`rs_bb_uart_tx_ring_buffer_enqueue()`) still has this
characteristic. Only the demo application was changed to avoid
triggering it. A real customer application making a large single
`Write()` call, or several rapid calls while a previous transmission may
still be in progress, especially on a slower CPU or at a high baud rate,
could still hit this. The recommended real fix -- restructuring the
enqueue path to "reserve space (short critical section) -> copy data (no
lock needed, single-producer/single-consumer, safe since the consumer
never reads unpublished slots) -> publish new head/count (short critical
section)" so critical-section duration becomes constant regardless of
request length -- was deliberately **not implemented in this delivery**.
Reasoning: this is a change to shared, already-hardware-verified PES
core logic (working correctly on two boards, newly confirmed working on
a third), and making it correctly requires careful reasoning about
memory ordering/visibility between producer and consumer that deserves
dedicated attention, not a rushed change alongside everything else in
this session. Recorded here explicitly as a known, real, not-yet-closed
gap for a future session, not quietly deferred.

**Structural QA:** all three projects re-checked (brace/paren balance
across `src/`/`ra_gen/`) after every edit in this addendum -- clean.

**CONFIRMED FIXED on real FPB-RA2E3 hardware (same session, user
follow-up):** user retested with the single-`WriteString()` banner fix
and reported the startup corruption is resolved. This closes out the
addendum 2/3 investigation with a genuine confirmed fix, not just a
plausible theory -- root cause (critical section duration scaling with
enqueue length) and fix (consolidate the banner into one call) both
verified against real hardware behavior, not just static code analysis.

**Diagnostic `WARMUP` probe removed** from
`src/rs_bb_uart_tx_demo.c` (was only ever meant to test the "first
Start() ever" theory in addendum 2, which the single-`WriteString()`
banner fix superseded -- the theory that fix actually confirmed was the
critical-section one from addendum 3, not the one `WARMUP` was testing).
Removed cleanly: the `WriteString("WARMUP\r\n")` call and its
`StatusGet()`-polling drain-wait block, restoring
`rs_bb_uart_tx_demo_run()` to call `demo_print_banner()` directly after
`ConfigCheck()` succeeds, matching the structure of the FPB-RA4E2/
FPB-RA6E2 deliveries. Structural balance re-verified after removal --
clean. **FPB-RA2E3 is now confirmed working end-to-end on real
hardware, matching FPB-RA4E2 and FPB-RA6E2's status.**

## 16. Cycle 9 addendum 4 -- real timing validation via logic analyzer capture (Architecture & Design Section 21.2)

User captured P109 with a logic analyzer during 4 heartbeat prints + 1
status line at 115200 baud (`FPB-RA2E3_P109_115200Baud_Capture.csv`, 954
edge-transition timestamps at effectively nanosecond precision) and asked
for jitter/baud-rate-error confirmation -- this is the first real Section
21.2-style validation performed with actual captured hardware data on any
of the three boards, not inferred from source review.

**Analysis methodology:** wrote a from-scratch UART decoder against the
raw edge-transition CSV (not a pre-decoded/annotated export) using exact
nanosecond-precision timestamps (confirmed Python's `datetime` truncates
to microsecond precision -- re-parsed via manual string parsing to avoid
losing sub-microsecond precision that matters at ~8.68 us bit periods).
Hit and fixed two real decoder bugs before trusting any output -- a good
discipline to repeat next time: (1) initially treated the start bit's own
segment as if it were the first data bit (off-by-one), producing garbage
decoded text; (2) when a start bit merges seamlessly into several
same-level leading data bits (no intervening edge), initially discarded
the whole merged segment instead of splitting off just the start bit's
own 1-bit contribution. Verified the fix by decoding actual byte content
and confirming it matches expected demo output exactly (100% correct,
zero decode errors across all 160 transmitted bytes) before trusting any
timing statistics -- decoding correctly first is what makes the timing
analysis trustworthy, not the other way around.

**Results:**
- **Effective baud rate (throughput-relevant metric, byte-start to
  next-byte-start across 155 back-to-back transitions):** mean -0.20%
  error vs. nominal 115200 baud, stdev 0.03% -- excellent, well within
  the +/-3% target.
- **Per-bit-position pattern (new finding, not random jitter):** start
  bit consistently ~8.8% short; stop bit consistently ~13.8% long with
  remarkably tight stdev (18.8 ns, 0.2%) across 155 of 159 possible byte
  boundaries; data bits close to nominal with small progressive drift.
  The tightness of the stop-bit measurement (not a wide/random spread)
  is itself the evidence this is deterministic, not electrical jitter.
- **Cumulative drift within a byte:** builds to about -16.6% of one bit
  period by the last data bit (bit 7), then the elongated stop bit pulls
  it back to near zero (-1.9%) by the end of the 10-bit frame -- bounded
  and self-correcting, consistent with the 100% correct decode achieved.
- **Precise Architecture & Design Section 5.6 spec check** (user asked
  directly "what is the jitter spec from this measured data?" as a
  follow-up -- this required going back to the exact spec wording, not
  the earlier looser "deviation from nominal" framing this addendum
  originally reported): Section 5.6's actual metric is **bit-to-bit**
  jitter, `|T[n+1]-T[n]|` between temporally adjacent bit periods, target
  `<= +/-10%`. Computed precisely against all 1439 adjacent bit-period
  pairs in the capture:
  - **Overall (literal, unfiltered): max 21.89%, mean 4.40% -- exceeds
    the +/-10% target.** 310/1439 pairs (21.5%) exceed it.
  - **Decomposed -- within-byte pairs only (data bit -> data bit, 1120
    pairs): max 9.91%, mean 1.02%, ZERO pairs exceed +/-10%.** The
    PES's actual bit-generation timing meets spec, right at the edge but
    compliant.
  - **Decomposed -- pairs touching the stop bit (319 pairs, i.e. the
    same mechanism as the bullet above): mean 16.28%, max 21.89%,
    97.2% of these specific transitions exceed +/-10%.**
  - **Conclusion: the Section 5.6 spec violation is real, precisely
    quantified, and entirely concentrated at one specific, deterministic,
    already-identified transition per byte** (the stop-bit/next-byte
    dequeue boundary) -- not a diffuse general timing-accuracy problem.
    This is a materially more precise and less favorable finding than
    this addendum's own earlier framing ("does not appear to threaten
    data correctness... not an urgent defect") -- that framing was about
    receiver tolerance/decode correctness (still true, confirmed by the
    zero-error decode), not about literal Section 5.6 compliance, which
    this board's captured data does not currently meet.

**Best explanation, not yet independently confirmed at the code level:**
likely the bit-engine's per-byte "was that the last bit? do I need to
dequeue the next byte?" logic in `rs_bb_uart_tx_hal_gpt_callback()`/
`rs_bb_uart_tx_engine_tick()`, adding a consistent ~1.2 us at that specific
state transition. **Distinct from the Cycle 9 addendum 3 finding** -- that
one was about *multiple rapid Write() calls* landing while a transmission
is already active (specifically the old multi-call banner); this pattern
is present on every byte of *single, isolated* `WriteString()` calls
(heartbeats/status lines each make exactly one call), so it's a separate,
more fundamental per-byte-boundary characteristic, not the same bug
resurfacing. Not yet traced to a specific line of code -- the ~1.2 us
figure is an empirical measurement, not yet connected to a specific
instruction count in the ISR.

**Practical implication -- two separate questions, not one:**
- **Decode correctness (does data arrive intact?):** yes, confirmed --
  the bounded, self-correcting drift pattern combined with the zero-error
  160-byte decode shows real UART receivers (including this on-board VCOM
  path) tolerate this comfortably in practice.
- **Architecture & Design Section 5.6 spec compliance (is bit-to-bit
  jitter within +/-10%?):** **no, not currently** -- see the precise
  breakdown above. This is a real, measured spec miss, narrowly and
  precisely localized to one already-identified transition, not a vague
  or diffuse finding. Recorded here as a genuine, now-quantified PES-core
  finding for the Phase 4 write-up -- worth treating as an actual open
  item against the stated design target, not just a "nice to know"
  characteristic, even though it hasn't been shown to cause practical
  decode failures on this board's on-board VCOM path specifically.

**FPB-RA4E2 capture obtained and analyzed same session, see Section 17 --
hypothesis confirmed:** same mechanism present on both boards, smaller
absolute magnitude on the faster CPU as predicted, and RA4E2 passes
Section 5.6 cleanly at 115200 baud where RA2E3 does not. Still not done:
FPB-RA6E2 capture (user has stated intent to capture it).

## 17. Cycle 9 addendum 5 -- FPB-RA4E2 logic analyzer capture: confirms the stop-bit mechanism is cross-board, and that FPB-RA4E2 PASSES Section 5.6 at 115200 baud

User captured P411 on FPB-RA4E2 (`FPB_RA4E2_P411_115200Baud_Capture.csv`,
920 edge-transition timestamps, same format/precision as the RA2E3
capture) specifically to compare against RA2E3's worst-case result and
determine whether the stop-bit/dequeue-overhead mechanism found there is
board-specific or general to the PES core. Same decoder (already
validated against RA2E3's known content) applied unchanged -- decoded
158 bytes, zero errors, zero bad stop bits, content confirmed sensible
(`[000027] Heartbeat OK` ... `[000030] Status ...` ... `[000031] Heartbeat OK`)
before trusting any timing statistics, same discipline as before.

**Results, with RA2E3 shown for direct comparison:**

| Metric | FPB-RA4E2 (Cortex-M33 @ 100 MHz) | FPB-RA2E3 (Cortex-M23 @ 48 MHz) |
|---|---|---|
| Effective baud rate error | -0.50% (stdev 0.15%) | -0.20% (stdev 0.03%) |
| Bit-to-bit jitter, overall max | **7.60%** | 21.89% |
| Bit-to-bit jitter, overall violations of +/-10% | **0/1421 (0%)** | 310/1439 (21.5%) |
| Within-byte jitter max | 2.53% | 9.91% |
| Stop-bit-boundary jitter max | 7.60% | 21.89% |
| Stop bit deviation from nominal (mean) | +4.70% (stdev 76.5 ns) | +13.8% (stdev 18.8 ns) |

**Confirms the Section 5.6 finding is cross-board, not RA2E3-specific:**
the stop bit runs measurably long on FPB-RA4E2 too (+4.70%), not just
RA2E3 -- this was a real open question (could have been an RA2E3-only
quirk) and is now settled: it's a genuine PES-core characteristic present
on both boards, matching the "same ISR code, per-byte dequeue overhead"
explanation from addendum 4, not something specific to Cortex-M23 or to
this board's clock tree.

**Confirms the CPU-speed-scaling hypothesis, with a real (not perfectly
linear) number attached:** RA4E2's 100 MHz CPU is ~2.1x faster than
RA2E3's 48 MHz. The stop-bit deviation dropped from +13.8% to +4.70% --
roughly a 2.9x reduction, in the right direction and rough magnitude for
a fixed-instruction-count ISR cost that takes proportionally less real
time on a faster CPU, though not an exact 1:1 match to the clock ratio
(plausible given Cortex-M23 Baseline vs Cortex-M33 Mainline have
different instruction encodings/cycle costs for the same C code, not
purely a clock-speed difference) -- reported as directionally confirmed,
not claimed as precisely predicted.

**FPB-RA4E2 cleanly PASSES Architecture & Design Section 5.6 at 115200
baud:** max bit-to-bit jitter 7.60%, zero violations across 1421 measured
adjacent-bit-period pairs. This is a genuine pass with margin, not a
near-miss -- worth stating plainly since RA2E3's result could otherwise
leave an impression that the whole PES has a jitter problem, when the
data now shows it's specifically an RA2E3-at-high-baud-rate finding.

**FPB-RA6E2 capture obtained and analyzed same session -- see Section 18
for the complete three-board picture.** The "smaller absolute delay on
faster CPU" prediction held; the specific RA4E2-vs-RA6E2 comparison
turned up a genuinely unexpected additional wrinkle (RA6E2 measurably
tighter than RA4E2 despite nominally identical core/clock) -- see
Section 18 for full detail rather than speculating further here.

## 18. Cycle 9 addendum 6 -- FPB-RA6E2 logic analyzer capture: completes the three-board Section 5.6 picture, plus an unexplained RA4E2-vs-RA6E2 difference

User captured P411 on FPB-RA6E2
(`FPB_RA6E2_P411_115200Baud_Capture.csv`, 924 edge-transition timestamps,
same format/precision as the other two captures), completing the
three-board dataset for Architecture & Design Section 21.2 validation.
Same decoder (unchanged since RA2E3, already validated three times now)
applied -- decoded 158 bytes, zero errors, zero bad stop bits, content
confirmed sensible (`[000036] Heartbeat OK` ... `[000040] Status ...`)
before trusting timing statistics, same discipline as the prior two
captures.

**Complete three-board comparison, all at 115200 baud (the worst-case
baud rate -- shortest bit period, least absolute timing margin):**

| Metric | FPB-RA2E3 (Cortex-M23 @ 48 MHz) | FPB-RA4E2 (Cortex-M33 @ 100 MHz) | FPB-RA6E2 (Cortex-M33 @ 100 MHz) |
|---|---|---|---|
| Effective baud rate error | -0.20% (stdev 0.03%) | -0.50% (stdev 0.15%) | -0.65% (stdev 0.13%) |
| Overall bit-to-bit jitter max | 21.89% -- FAILS +/-10% | 7.60% -- PASSES | **4.84% -- PASSES, tightest of all three** |
| Overall violations of +/-10% | 310/1439 (21.5%) | 0/1421 (0%) | 0/1421 (0%) |
| Within-byte jitter max | 9.91% | 2.53% | 1.61% |
| Stop-bit-boundary jitter max | 21.89% | 7.60% | 4.84% |
| Stop bit deviation (mean) | +13.8% | +4.70% | +2.34% |

**Overall Section 5.6 status, all three boards, confirmed by real
captured data (not inference):** FPB-RA4E2 and FPB-RA6E2 both cleanly
PASS the +/-10% bit-to-bit jitter target at 115200 baud, with real
margin, zero violations across 1421 measured adjacent-bit-period pairs
each. FPB-RA2E3 does not meet the target at this baud rate (21.5% of
measured pairs violate it), entirely due to the same identified,
localized stop-bit/dequeue-overhead mechanism running proportionally
longer on its slower CPU.

**New, unexplained finding: FPB-RA6E2 is measurably tighter than
FPB-RA4E2** despite both being Cortex-M33 @ nominally 100 MHz running
identical PES code with the identical PCLKD/4 clock-divider scheme --
stop-bit deviation is roughly half (+2.34% vs +4.70%), and overall max
jitter is nearly half (4.84% vs 7.60%). This was not predicted by the
"CPU speed explains it" theory from addendum 5, since these two boards
have the same CPU speed. Possible explanations, none confirmed:
different flash access/wait-state characteristics between the RA4 and
RA6 silicon families (the surrounding memory system is Renesas's own
implementation, not part of the licensed ARM Cortex-M33 core IP, and can
differ by product line even with an identical core), or simply
chip-to-chip/environmental measurement variance. **Not investigated
further this session** -- flagged honestly as an open question rather
than a confident explanation, since the available evidence (one capture
per board) isn't enough to distinguish a systematic silicon difference
from measurement noise. Would need repeated captures on each board to
determine whether this specific RA4E2-vs-RA6E2 gap is consistent/real or
just this-one-capture noise, before drawing a firm conclusion.

**Bottom line for the Phase 4 write-up:** the PES core's Section 5.6
jitter target is met on two of three reference boards at the worst-case
(115200) baud rate, with the one exception precisely characterized (not
a vague "sometimes fails") and traced to a specific, already-described
mechanism. This is a solid, evidence-based validation result -- real
captured data from all three boards, not architectural inference -- and
should be presented in the eventual documentation exactly this
specifically, including the honest RA2E3 miss and the unexplained
RA4E2-vs-RA6E2 gap, rather than a blanket "PES meets timing spec" claim
that the RA2E3 data would contradict.

**New methodological variable identified, same session, not yet
resolved:** user noted the RA6E2 capture was taken with the board running
standalone (disconnected from/not actively monitored by the debug
probe), which may differ from how the other captures were taken, and
proposed re-capturing RA2E3 under the same standalone condition to test
whether debugger-attachment state affects measured timing. Checked the
original Architecture & Design document's validation section (Section
5.5, "idle conditions / representative application load / representative
interrupt load") -- **debugger-attached vs. standalone is not among the
validation conditions it specifies.** This is a real gap in the original
validation criteria, not something this project failed to follow --
worth carrying forward into whatever supersedes this document for Phase
4, regardless of what the new capture shows, since debug probes can
plausibly affect timing via power delivery differences, CPU debug-mode
overhead, or other mechanisms. **Not yet resolved:** awaiting the
standalone FPB-RA2E3 recapture. If it shows a meaningful difference from
the original capture, the cleanest way to confirm the debugger-attached
variable is the actual cause (rather than ordinary capture-to-capture
noise, the same caution already flagged for the RA4E2-vs-RA6E2 gap)
would be a same-board, same-condition repeat -- two standalone captures
compared to each other, not just standalone-vs-original single captures.

**RESOLVED, same session: debugger-attached vs. standalone does NOT
explain FPB-RA2E3's Section 5.6 miss.** User provided a second FPB-RA2E3
capture (`FPB_RA2E3_P109_115200_Capture_02.csv`), board running
standalone. Same decoder applied, same validation discipline (decoded
158 bytes, zero errors, zero bad stop bits, content sensible --
`[000032] Heartbeat OK` ... `[000035] Status ...` ... `[000036] Heartbeat OK`,
confirms this was a fresh boot/reflash relative to the original capture's
`[002461]`-range counter values, not a continuation of the same run --
before trusting timing statistics).

| Metric | Original (debugger-attached) | Standalone |
|---|---|---|
| Effective baud rate error | -0.20% | -0.22% |
| Overall jitter max | 21.89% | **21.89% (identical to 2 decimal places)** |
| Overall violations of +/-10% | 310/1439 (21.5%) | 306/1421 (21.5%) |
| Within-byte max | 9.91% | 9.91% (identical) |
| Stop-bit-boundary max | 21.89% | 21.89% (identical) |
| Stop bit deviation (mean) | +13.8% | +13.32% |
| Stop bit stdev | 18.8 ns | 210.8 ns |

**Conclusion:** the max jitter value matching to two decimal places
across debugger-attached and standalone conditions is about as clean a
confirmation as a single before/after comparison can provide that
FPB-RA2E3's Section 5.6 miss is a genuine, repeatable, deterministic
characteristic of the board/PES combination -- not a debugger-connection
artifact. This is real, not a test-setup confound; it should be reported
at face value in the Phase 4 write-up, not caveated as "possibly due to
debug probe interference."

**One real secondary finding, not decisive for pass/fail but worth
recording:** stop-bit timing *spread* (stdev) is over 10x wider standalone
(210.8 ns vs. 18.8 ns) even though the mean and max are essentially
unchanged. Genuinely different observation from "does the mean/max
change" -- flagged rather than glossed over, though since it doesn't
push the max above what was already measured, it doesn't change the
Section 5.6 conclusion above.

**Explicit scope limit -- do not over-extend this conclusion:** this
result rules out the debugger-attached variable specifically for
FPB-RA2E3. It says nothing about whether debugger-attachment state
affected the FPB-RA4E2-vs-FPB-RA6E2 comparison in Section 18, since the
control was only run on FPB-RA2E3. That comparison's cause remains an
open question, not resolved by this result.

## 19. How to resume this task in a new conversation

**Current state as of the end of this session:** all three boards
(FPB-RA4E2, FPB-RA6E2, FPB-RA2E3) are ported, built, flashed, and
confirmed working on real hardware. All three have been logic-analyzer
captured and precisely analyzed at 115200 baud (the worst-case/shortest
bit period). Result: FPB-RA4E2 and FPB-RA6E2 both cleanly pass
Architecture & Design Section 5.6's +/-10% bit-to-bit jitter target
(max 7.60% and 4.84% respectively, zero violations). FPB-RA2E3 does not
(max 21.89%, 21.5% of measured transitions violate), and this has been
confirmed real and repeatable (not a debugger-attachment artifact --
see Section 18's standalone-vs-debugger-attached comparison, which
matched to two decimal places).

**Explicitly stated next task (not yet started):** investigate what can
be improved to bring FPB-RA2E3's jitter within spec. User will resume
this tomorrow once their session allowance resets.

**To resume, upload:** the three project zips
(`BitBangedUART_TX_FPB_RA2E3.zip`, `BitBangedUART_TX_FPB_RA4E2.zip`,
`BitBangedUART_TX_FPB_RA6E2.zip`) and this `PhasePlan.md`. The original 7
reference documents are not needed again -- their content has long since
been absorbed into this file and the actual project deliverables. Say
something like "Resume the Bit-Banged UART TX PES project from
PhasePlan.md -- investigating the FPB-RA2E3 jitter improvement task
described at the end of Section 19."

**Before proposing or making any change, re-read:**
- **Section 15** (Cycle 9 addendum 3) -- the actual root-cause finding
  and the specific, already-scoped-but-not-implemented fix: restructuring
  `rs_bb_uart_tx_ring_buffer_enqueue()` from "hold the critical section for
  the whole `memcpy()`" to a "reserve space (short critical section) ->
  copy data (no lock needed, single-producer/single-consumer) -> publish
  new head/count (short critical section)" pattern. This was deliberately
  NOT implemented in Cycle 9 because it's a change to shared,
  already-hardware-verified PES core logic that deserved dedicated
  attention rather than a rushed change alongside everything else that
  session -- this is very likely where tomorrow's investigation should
  start, though re-verify that reasoning still holds rather than assuming
  it's still the right call.
- **Section 16-18** -- the full measured jitter data for all three boards,
  including the precise within-byte vs. stop-bit-boundary decomposition
  that localizes the FPB-RA2E3 problem to one specific transition per
  byte (not diffuse timing inaccuracy), and the exact numbers to compare
  any proposed fix against (baseline to beat: FPB-RA2E3 max jitter
  21.89%, stop-bit deviation +13.8%).
- **Section 18's unresolved RA4E2-vs-RA6E2 gap** -- worth keeping in mind
  since it's still an open question that could turn out to be related
  (or could turn out to be unrelated noise; genuinely unknown).

**Important framing for the investigation itself:** the goal is a fix to
the PES *core* (`rs_bb_uart_tx_ring_buffer.c`, and possibly
`rs_bb_uart_tx_hal.c`'s ISR/dequeue path), not a per-board or demo-level
workaround -- the underlying mechanism is confirmed present on all three
boards (Section 17-18), just with enough margin on the two Cortex-M33
boards to stay under spec. A real fix should improve all three boards'
numbers, not just paper over FPB-RA2E3 specifically. Any fix will need
the same rigor already established in this session: read the actual
driver/PES source before proposing changes (not guess), verify structural
balance after edits, and ideally get a fresh logic analyzer capture on at
least FPB-RA2E3 (and ideally all three boards, to confirm no regression)
before considering it confirmed rather than theoretically sound.
