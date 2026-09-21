# Build & Test Instructions — Bit-Banged UART TX (Meson + LLVM/Clang)

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).
> The CMake/VS Code build of the example projects is documented in [BUILD.md](BUILD.md);
> this document covers the **Meson** flow: host unit tests, firmware, and on-target unit tests.

- ✅ Host-based **unit tests** (Windows + Linux, no hardware)
- ✅ Cross-built application firmware for FPB-RA2E3 / RA4E2 / RA6E2
- ✅ On-target Unity test firmware for the same boards

---

## Host unit tests

No hardware, no FSP, no RASC. Needs only Meson, Ninja, a C compiler (clang/clang-cl), and the
Unity subproject (`subprojects/unity.wrap`, fetched by Meson).

```powershell
# Windows
meson setup build-clangcl --wipe --native-file .\meson\llvm-clangcl-msvc.ini
meson compile -C build-clangcl
meson test -C build-clangcl -v
```

```bash
# Linux
meson setup build-clang --wipe --native-file ./meson/llvm-clangcl-ubuntu2404-ci.ini
meson compile -C build-clang
meson test -C build-clang -v
```

`RS_BB_UART_TX_TEST_HOST_BUILD=1` and `RS_BB_UART_TX_CFG_PRESET_CONFIG_USE=0` are set in
`tests/meson.build` (`host_test_cargs`), **not** in the native files, so every native toolchain
file builds the tests identically.

### Two host executables

| Executable | Links | Runs on target? |
|---|---|---|
| `unittest_on_pc` | pure logic + core + board data file, **mock HAL** defined in `test_rs_bb_uart_tx_core.c` | ✅ yes |
| `unittest_hal_on_pc` | the same, plus the **real `rs_bb_uart_tx_hal.c`** against a fake FSP timer/pin layer (`tests/stubs/rs_bb_uart_tx_fake_fsp.*`) | ❌ host only |

The split exists because the real `R_BSP_PinAccessEnable/PinWrite/...` are `__STATIC_INLINE` in the
BSP and cannot be replaced on target, while the GPT ISR callback (which glues the engine to the pin)
lives in `hal.c` and must be tested.

### Test groups

| Group | Tests | Executable | What is covered |
|---|---|---|---|
| `BbUartRingBuffer` | 18 | `unittest_on_pc` | reserved slot / capacity, FIFO, all-or-nothing, wrap-around, accounting |
| `BbUartEngine` | 16 | `unittest_on_pc` | per-state FSM transitions, pipeline register, the NEXT_BYTE race window, no ring access in START/DATA |
| `BbUartEngineFrame` | 11 | `unittest_on_pc` | 8N1 frame structure, LSB-first, exact 10-tick spacing, stop bit, launch latency, all 256 byte values |
| `BbUartConfig` | 22 | `unittest_on_pc` | `ConfigGen` / `ConfigCheck` / `ConfigGetSize`, baud bounds, open order, error propagation |
| `BbUartLifecycle` | 12 | `unittest_on_pc` | UNCONFIGURED → READY → TRANSMITTING, ERROR handling, `StatusGet` |
| `BbUartWrite` | 15 | `unittest_on_pc` | `Write`/`WriteString`, buffer-full policy, diagnostics counters |
| `BbUartHal` | 20 | `unittest_hal_on_pc` | GPT open/start/stop/close, period formula, baud accuracy ≤ 1 %, PFS access bracketing |
| `BbUartIsr` | 9 | `unittest_hal_on_pc` | callback state gating, pin-write-before-advance, byte accounting, timer stop |
| `BbUartWaveform` | 15 | `unittest_hal_on_pc` | public API → decoded pin waveform, burst duration = 10·N + 2 ticks, no gaps |

Expected output:

```
Unity test run 1 of 2   ->   94 Tests 0 Failures 0 Ignored   OK
Unity test run 2 of 2   ->   44 Tests 0 Failures 0 Ignored   OK
```

### Relation to the jitter specification

`tests/Bit_Banged_UART_TX_Jitter_Specification.txt` is an **analogue** requirement (edge timing in
nanoseconds, measured with a logic analyser — see the FPB-RA2E3 verification PDF). It cannot be
replayed on a host. The host tests prove the deterministic *logical* contract it depends on: every
frame is exactly 10 bit periods start-to-start, the stop bit is a full bit period, the only extra
latency is one idle bit period before the first start bit, and the timer period matches the baud
rate to within 1 %.

---

## Firmware build (cross)

Requires RASC (standalone), the ATfE LLVM toolchain, and the `fsp.wrap` / `cmsis.wrap` files
(git-ignored; keep them local as with the other PES projects). One-time:

```powershell
. .\scripts\env.ps1
meson subprojects download
```

```powershell
# FPB-RA2E3 (Cortex-M23)  — edit meson\llvm-cortex-m23.ini first (ATfE paths + multilib, see below)
meson setup build-ra2e3 --wipe --cross-file meson\llvm-cortex-m23.ini `
  -Dgen_board=ra2e3-fpb -Dboard=ra2e3_fpb -Dmcu=ra2e3 -Ddevice=RA2E3
meson compile -C build-ra2e3

# FPB-RA4E2 / FPB-RA6E2 (Cortex-M33)
meson setup build-ra4e2 --wipe --cross-file meson\llvm-cortex-m33.ini `
  -Dgen_board=ra4e2-fpb -Dboard=ra4e2_fpb -Dmcu=ra4e2 -Ddevice=RA4E2
```

> **Use the `meson\` cross files, not `toolchains\`.** The `meson\llvm-cortex-m*.ini` files pass
> `--sysroot` (needed for `setjmp.h`, `string.h`, ...) and the matching multilib. The generic
> `toolchains\*.ini` files do not, and fail with `'setjmp.h' file not found`.
>
> `meson\llvm-cortex-m23.ini` ships with placeholder ATfE paths. Copy the paths from your working
> `llvm-cortex-m33.ini`, and set the multilib folder to the output of
> `clang --target=arm-none-eabi -mcpu=cortex-m23 -mthumb -mfloat-abi=soft -Os -print-multi-directory`.

Outputs per board: `bit_banged_uart_tx_on_<board>_clang.srec` (application) and
`bit_banged_uart_tx_unittest_on_<board>_clang.srec` (Unity test firmware).

Board glue (`hal_entry.c`, `fault_handler.c`, demo, and the RA4E2/RA6E2 board data file) is taken from
`examples/VSCode/BitBangedUART_TX_FPB_<BOARD>/src`, so nothing is duplicated.

## On-target unit tests

```powershell
meson compile -C build-ra2e3 rfp_program_ut
```

The UT image runs `BbUartRingBuffer`, `BbUartEngine`, `BbUartEngineFrame`, `BbUartConfig`,
`BbUartLifecycle`, and `BbUartWrite` (mock HAL) on the real Cortex-M core with the real compiler flags.

**Output transport.** The example projects configure only `r_gpt` + `r_ioport` (no SCI, no RTT), and the
VCOM TXD pin belongs to the PES itself. The UT image mocks the HAL, so that pin is free; Unity output is
sent by a small polled software UART (`tests/target/unity_port_polled_gpio.c`) at **9600 baud, 8N1** on the
board's VCOM TXD pin (`-Dunity_tx_pin=` overrides the per-board default). Open a terminal at 9600 8N1 on the
USB-VCOM port, or use `tests/run_ut.robot`.

### TDD workflow

```
1. meson test -C build-clangcl        # host tests: fast, no hardware
2. flash unittest SREC -> read VCOM   # on-target logic tests, real compiler/core
3. logic-analyser jitter check        # the analogue timing spec (see tests/*Jitter*)
4. flash app SREC                     # demo / production firmware
```

---

## Troubleshooting

| Symptom | Fix |
|---|---|
| `'setjmp.h' file not found` in firmware build | wrong cross file: use `meson\llvm-cortex-m23.ini` / `llvm-cortex-m33.ini` (with `--sysroot`), not `toolchains\*.ini` |
| `RASC expected output(s) missing` | RASC ran but wrote a different file set; the message lists what is missing — compare with `firmware\generated\meson.build` |
| `bsp_api.h: No such file` on host | `host_test_cargs` in `tests/meson.build` lost `-DRS_BB_UART_TX_TEST_HOST_BUILD=1` |
| `r_gpt.h: No such file` on host | `-DRS_BB_UART_TX_CFG_PRESET_CONFIG_USE=0` missing from `host_test_cargs` |
| `undefined reference to rs_bb_uart_tx_hal_*` in `unittest_on_pc` | `rs_bb_uart_tx_hal.c` must be excluded there; the mock in `test_rs_bb_uart_tx_core.c` provides the symbols |
| `multiple definition of R_BSP_PinWrite` on target | `tests/stubs` leaked onto the firmware include path / fake FSP linked into the UT image |
| `__declspec(noreturn)` errors in Unity (clang-cl) | ensure `/clang:-D_Noreturn=` is in `c_args` of `meson/llvm-clangcl-msvc.ini` |
| Garbled Unity output on target | terminal must be 9600 8N1 (not 115200) |
| Hard fault while running on-target tests | main stack too small: raise `BSP_CFG_STACK_MAIN_BYTES` (RASC default is ~1 KB); the tests keep large buffers static to stay within it |
| A test group never runs on target | add it to **both** `test_main.c`/`test_main_hal.c` and `test_main_firmware.c` (host-only groups excluded) |
