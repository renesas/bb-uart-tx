# Build Instructions — CMake + Ninja + LLVM (ATfE)

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).
> Note: for opening/building/debugging through the **VS Code UI**, see [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md) instead — this document is the command-line path plus the tooling details behind it.

Supports:
- ✅ Embedded firmware for **FPB-RA2E3** (Cortex-M23, R7FA2E3073CFL)
- ✅ Embedded firmware for **FPB-RA4E2** (Cortex-M33, R7FA4E2B9)
- ✅ Embedded firmware for **FPB-RA6E2** (Cortex-M33, R7FA6E2BB)
- ✅ Debug and Release build configurations (CMake presets)
- ✅ Flash/debug via **VS Code + Renesas hardware debug extension** (SEGGER J-Link)

Each board is an **independent, self-contained CMake project**, living under `examples/VSCode/BitBangedUART_TX_FPB_<board>/` — there is no shared top-level build across boards in this delivery. The instructions below apply identically to all three; substitute the board's folder name.

> **No e2studio-style "Import" step.** These are plain VS Code / CMake projects — you open a board's project by opening its `.code-workspace` file or folder directly (see [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md#2-opening-and-building-in-vs-code)), not by importing it into an IDE.

---

## 📁 Project Structure (per board)

```
examples/VSCode/BitBangedUART_TX_FPB_<board>/
│   CMakeLists.txt              ← top-level project definition
│   CMakePresets.json           ← Debug / Release configure+build presets (Ninja)
│   CMakeProject.json           ← device/core/toolchain metadata (Renesas VS Code extensions)
│   Config.cmake                ← user hook: RASC_EXE_PATH resolution
│   configuration.xml           ← FSP Smart Configurator project (GPT + GPIO configured)
│   ra_cfg.txt                  ← human-readable FSP config dump (FPB-RA2E3 only)
│
├── .vscode/
│       cmake-kits.json         ← "ARM LLVM - Ninja" / "ARM LLVM - MinGW Makefiles" kits
│       tasks.json              ← FSP Smart Configurator / Regenerate / Configure / Build tasks
│       launch.json             ← Renesas GDB Debugging (SEGGER J-Link) launch config
│       c_cpp_properties.json   ← IntelliSense include paths
│       settings.json
│
├── cmake/
│       GeneratedCfg.cmake      ← RASC-generated compiler flags (do not hand-edit)
│       GeneratedSrc.cmake      ← RASC-generated source globs: ra_gen/*, src/*, ra/* (do not hand-edit)
│       llvm.cmake              ← ATfE toolchain file (resolves ARM_TOOLCHAIN_PATH)
│       prebuild.cmake          ← backing script for "Regenerate Project Content"
│
├── ra/                         ← vendored FSP: bsp, arm/CMSIS, board, r_ioport, r_gpt (r_gpt.c genuinely vendored)
├── ra_cfg/                     ← FSP/BSP/driver build-time configuration headers
├── ra_gen/                     ← FSP Smart Configurator-generated pin/common/HAL data, vector table, main.c
├── script/                     ← fsp.lld, memory_regions.lld, bsp_linker_info.h (linker inputs)
│
└── src/
    │   hal_entry.c              ← application entry point
    │   hal_warmstart.c          ← BSP warm-start boilerplate
    │   fault_handler.c          ← Cortex-M23 or Cortex-M33 fault handler (board-specific — see FILE_GUIDE.md)
    │   rs_bb_uart_tx_demo.c     ← reference application (NOT reusable core)
    └── rs_bb_uart_tx/            ← *** the reusable core ***
        │   rs_bb_uart_tx.h / .c
        │   rs_bb_uart_tx_types.h
        │   rs_bb_uart_tx_ctrl.h
        │   rs_bb_uart_tx_cfg.h
        │   rs_bb_uart_tx_ring_buffer.h / .c
        │   rs_bb_uart_tx_engine.h / .c
        │   rs_bb_uart_tx_hal.h / .c
        └── config/
                rs_bb_uart_tx_data_fpb_<board>.h / .c
```

---

## 🛠️ Tools to Install

**1. LLVM Embedded Toolchain for Arm (ATfE)**
Provides `clang`/`clang++` targeting `arm-none-eabi` with the correct bare-metal sysroot. The e2studio FSP v6.5.0 bundle includes ATfE — install e2studio even if you plan to build entirely from the command line / VS Code, as this is the toolchain source these projects are configured against.
Download: https://www.renesas.com/en/software-tool/e2studio

Set the environment variable used by `.vscode/cmake-kits.json` and `cmake/llvm.cmake`:

```powershell
# Windows (PowerShell)
$env:ARM_LLVM_TOOLCHAIN_PATH = "C:\Users\<username>\AppData\Local\Programs\Renesas\RA\e2studio_v*_fsp_v6.5.0\toolchains\llvm_arm\ATfE-*\bin"
```
```bash
# Linux/macOS
export ARM_LLVM_TOOLCHAIN_PATH=/path/to/ATfE/bin
```

`cmake/llvm.cmake` also accepts `ARM_TOOLCHAIN_PATH` (as a CMake cache variable via `-D`, or as an environment variable) if you prefer to set it per-invocation instead of globally.

**2. CMake (≥ 3.16.4, CMakePresets require ≥ 3.23) and Ninja**
```powershell
pip install cmake ninja
```
or install via your platform's package manager. Verify:
```powershell
cmake --version
ninja --version
```

**3. RASC (standalone)** *(only required if you will modify `configuration.xml` and rebuild — the delivered projects already have `ra_gen`/`ra_cfg` content checked in)*
The standalone RASC installer provides a headless-capable `rasc` executable. `Config.cmake` defaults `RASC_EXE_PATH` to `echo` (a harmless no-op) unless you set it, so a first build with an unmodified `configuration.xml` does not require RASC to be installed at all.
Download: https://github.com/renesas/fsp/releases (look for the FSP v6.5.0-compatible RASC installer)

Set `RASC_EXE_PATH` if you need real regeneration:
```powershell
$env:RASC_EXE_PATH = "C:\Renesas\RA\sc_v*_fsp_v6.5.0\eclipse\rasc.exe"
```
or pass it on the CMake configure command line: `-DRASC_EXE_PATH=/path/to/rasc`.

**4. VS Code + extensions**
- **CMake Tools** (Microsoft) — kit selection, configure/build presets, status-bar build
- **Renesas RA extensions** — provides the `renesas-hardware` debug type used by `.vscode/launch.json`
- **clangd** (optional, recommended) — `.vscode/settings.json` and the project's `.clangd` file are already configured for it (`--query-driver=${env:ARM_LLVM_TOOLCHAIN_PATH}*`)

**5. SEGGER J-Link software** *(flashing/debugging only)*
The on-board J-Link OB debug probe on all three FPB boards is supported directly by the Renesas hardware debug extension; no separate J-Link software install is required beyond USB drivers on most platforms.

---

## ⚙️ Building from the Command Line

```bash
cd examples/VSCode/BitBangedUART_TX_FPB_RA4E2   # or _RA2E3 / _RA6E2

# Configure (Debug)
cmake --preset Debug

# Build
cmake --build --preset Debug

# Or, equivalently, without presets:
cmake -DARM_TOOLCHAIN_PATH="/path/to/ATfE/bin" -DCMAKE_TOOLCHAIN_FILE=cmake/llvm.cmake -G Ninja -B build/Debug
cmake --build build/Debug
```

For a Release build, substitute `Release` for `Debug` in the preset name (`CMakePresets.json` defines both).

**What happens during configure:** `CMakeLists.txt` includes `Config.cmake` (resolves `RASC_EXE_PATH`), then `cmake/GeneratedCfg.cmake` and `cmake/GeneratedSrc.cmake` (RASC-generated compiler flags and `file(GLOB_RECURSE ...)` source lists covering `ra_gen/`, `src/`, and `ra/`). If `RASC_EXE_PATH` resolves to a real `rasc` executable (not the `Config.cmake` default of `echo`), a pre-build `execute_process()` step invokes it with `--generate --devicefamily ra --compiler LLVMARM ... configuration.xml` to regenerate FSP content before the glob runs. Because `src/rs_bb_uart_tx/` is picked up by the recursive glob automatically, **no CMakeLists.txt edit is needed when you add files under `src/`.**

**What happens at flash time:** flashing is performed through VS Code's Renesas hardware debug extension (`.vscode/launch.json`, `renesas-hardware` debug type), not through a separate command-line flash step in this delivery — see [EXAMPLES_GUIDE.md](EXAMPLES_GUIDE.md#3-flashing-and-debugging).

---

## 🔧 Regenerating FSP Project Content

If you edit `configuration.xml` (e.g. via the FSP Smart Configurator, opened through the **"FSP Smart Configurator"** VS Code task), regenerate before rebuilding:

- **Automatic:** the next `cmake --build` (or **Configure Project** task) re-runs the RASC pre-build step automatically if `configuration.xml`'s modification time has changed and `RASC_EXE_PATH` is set to a real executable.
- **Manual:** run the **"Regenerate Project Content"** VS Code task (`Terminal → Run Task → Regenerate Project Content`), which runs `cmake -P cmake/prebuild.cmake` directly.

See [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#3-required-non-default-fsp-settings) for the exact GPT/GPIO/SCI settings that must be preserved per board before you click **Generate Project Content** in the Smart Configurator GUI.

---

## 🔧 Troubleshooting

| Symptom | Fix |
|---|---|
| `Toolchain path not defined` at configure time | Set `ARM_LLVM_TOOLCHAIN_PATH` (or `ARM_TOOLCHAIN_PATH`) before invoking `cmake` — see [Tools to Install](#-tools-to-install) |
| `Toolchain path does not exists: ...` | The path in `ARM_TOOLCHAIN_PATH`/`ARM_LLVM_TOOLCHAIN_PATH` does not point at a real directory containing `clang`/`clang++` — verify the ATfE install location |
| `Please specify the CMAKE_BUILD_TYPE with -DCMAKE_BUILD_TYPE=` | You configured with a single-config generator (e.g. Ninja without a preset) and did not supply `CMAKE_BUILD_TYPE` or `BUILD_TYPE` — use a preset (`cmake --preset Debug`) or add `-DCMAKE_BUILD_TYPE=Debug` |
| `RASC Execution is failed: <code>` | `RASC_EXE_PATH` points at a real executable, but it failed — check `rasc_conf_cmd_err.txt` in the build directory for the RASC error output; often a `configuration.xml` FSP-version mismatch |
| Build succeeds but GPT ISR never fires / `rs_bb_uart_tx_hal_gpt_open()` returns `TIMER_ERROR` | GPT0's counter-overflow interrupt was disabled or its callback reverted to `NULL` in `configuration.xml` — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#3-required-non-default-fsp-settings) |
| No output on the VCOM terminal at all | Confirm the terminal is on the correct COM port, at 115200 8N1; confirm the TX pin is actually configured as GPIO output and not still claimed by SCI9 (FPB-RA2E3) / SCI0 (FPB-RA4E2/FPB-RA6E2) — see [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#3-required-non-default-fsp-settings) |
| No output below 9600 baud on the VCOM terminal, but the TX pin signal (scope/logic analyzer) looks correct | On-board J-Link VCOM bridge firmware does not reliably relay baud rates below 9600 — see [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md#7-on-board-vcom-baud-range-caveat) |
| `StatusGet()`'s `full` counter is non-zero every boot | Ring buffer is undersized for your traffic burst — see [BEHAVIOR_AND_PERFORMANCE.md](BEHAVIOR_AND_PERFORMANCE.md#5-ram-consumption) and increase `RS_BB_UART_TX_CFG_BUFFER_SIZE_BYTES` |
| Clean rebuild needed | Delete the `build/Debug` or `build/Release` directory and re-run `cmake --preset <name>` |
| clangd shows spurious errors in VS Code but the CMake build succeeds | Confirm `ARM_LLVM_TOOLCHAIN_PATH` is set in the same shell VS Code was launched from — `.clangd`'s `--query-driver` argument depends on it being resolvable |

---

## ✅ Summary

| Build | Toolchain file | Preset | Device | `launch.json` device |
|---|---|---|---|---|
| FPB-RA2E3 firmware | `cmake/llvm.cmake` | `Debug` / `Release` | R7FA2E3073CFL (Cortex-M23) | `R7FA2E307` |
| FPB-RA4E2 firmware | `cmake/llvm.cmake` | `Debug` / `Release` | R7FA4E2B9 (Cortex-M33) | `R7FA4E2B9` |
| FPB-RA6E2 firmware | `cmake/llvm.cmake` | `Debug` / `Release` | R7FA6E2BB (Cortex-M33, TrustZone) | `R7FA6E2BB` |

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
