# Running the Example Projects and Expected Output

> Part of the [`rs_bb_uart_tx` documentation set](BIT_BANGED_UART_TX-USER_MANUAL.md#documentation-map).
> For command-line CMake/Ninja builds, tool installation, and troubleshooting, see [BUILD.md](BUILD.md).

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Opening and Building in VS Code](#2-opening-and-building-in-vs-code)
3. [Flashing and Debugging](#3-flashing-and-debugging)
4. [Expected Output — FPB-RA2E3](#4-expected-output--fpb-ra2e3)
5. [Expected Output — FPB-RA4E2](#5-expected-output--fpb-ra4e2)
6. [Expected Output — FPB-RA6E2](#6-expected-output--fpb-ra6e2)
7. [Interaction Notes](#7-interaction-notes)

---

## 1. Prerequisites

- One of: FPB-RA2E3, FPB-RA4E2, or FPB-RA6E2, connected via USB (provides both power and the on-board J-Link virtual COM port)
- Visual Studio Code with the CMake Tools extension and the Renesas RA extensions (device family support, `renesas-hardware` debug type)
- The LLVM Embedded Toolchain for Arm (ATfE), with `ARM_LLVM_TOOLCHAIN_PATH` (or `ARM_TOOLCHAIN_PATH`) set — see [BUILD.md](BUILD.md) for exact tool installation and environment setup
- A serial terminal (Tera Term or equivalent) connected to the on-board J-Link virtual COM port at **115200 baud, 8N1, no flow control**

## 2. Opening and Building in VS Code

Each board's project lives under `examples/VSCode/BitBangedUART_TX_FPB_<board>/` (`RA2E3`, `RA4E2`, or `RA6E2`). **There is no e2studio-style "Import" step for VS Code projects** — you open the project directly by opening its workspace file, the same way you'd open any other VS Code project:

1. **Open:** double-click the board's `.code-workspace` file — `BitBangedUART_FPB_RA2E3.code-workspace`, `BitBangedUART_FPB_RA4E2.code-workspace`, or `BitBangedUART_FPB_RA6E2.code-workspace` — or in VS Code use **File → Open Workspace from File...** and select it. Opening the board's folder directly (**File → Open Folder...**) also works, since each board's project is fully self-contained. Each board is a separate project; there is no multi-board workspace in this delivery. VS Code's CMake Tools extension detects `CMakeLists.txt`/`CMakePresets.json` automatically on open — no import or project-recognition step is required.
2. Ensure `ARM_LLVM_TOOLCHAIN_PATH` is set in your environment before starting VS Code (or set `ARM_TOOLCHAIN_PATH` directly in `.vscode/cmake-kits.json`).
3. In the VS Code status bar, select the **"ARM LLVM - Ninja"** CMake kit (or **"ARM LLVM - MinGW Makefiles"** on Windows without Ninja).
4. Select a configure preset — **Debug** or **Release** (`CMakePresets.json`).
5. Build via the CMake Tools status-bar **Build** button, or run the **Build Project** task (**Terminal → Run Task → Build Project**). This also runs **Configure Project** first, which invokes RASC as a pre-build step if `configuration.xml` has changed (or if `RASC_EXE_PATH` resolves to something other than the `Config.cmake` default of `echo`) — see [BUILD.md](BUILD.md) for details.
6. The project is functional as delivered — `configuration.xml` and the checked-in `ra_gen`/`ra_cfg` content are pre-configured for the preset configuration path (`RS_BB_UART_TX_CFG_PRESET_CONFIG_USE = 1`, the default). No FSP Smart Configurator re-run is required for a first build.

> To edit the FSP configuration interactively instead, run the **"FSP Smart Configurator"** task (**Terminal → Run Task → FSP Smart Configurator**), which opens `configuration.xml` in the Smart Configurator GUI. See [CONFIGURATION_GUIDE.md](CONFIGURATION_GUIDE.md#3-required-non-default-fsp-settings) for the settings that must be preserved per board.

## 3. Flashing and Debugging

Each board project includes a `.vscode/launch.json` configured for the Renesas hardware debug extension, targeting the on-board SEGGER J-Link:

| Board | `device` |
|---|---|
| FPB-RA2E3 | `R7FA2E307` |
| FPB-RA4E2 | `R7FA4E2B9` |
| FPB-RA6E2 | `R7FA6E2BB` |

Start debugging via **Run and Debug → Renesas GDB Debugging** (or **F5**). This flashes the built image and starts a debug session. Once running, open your serial terminal on the board's VCOM port at 115200 8N1.

## 4. Expected Output — FPB-RA2E3

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
[000005] Status       : queued=0 free=511 tx=304 ok=304 full=0
[000006] Heartbeat OK
...
```

## 5. Expected Output — FPB-RA4E2

```
================================
Renesas Bit-Banged UART Demo
================================

Board        : FPB-RA4E2
Project      : VS Code bare-metal example
UART Mode    : Software TX only
Baud Rate    : 115200
Frame Format : 8N1
Transport    : On-board J-Link VCOM

System Ready

[000001] Heartbeat OK
[000002] Heartbeat OK
[000003] Heartbeat OK
[000004] Heartbeat OK
[000005] Status       : queued=0 free=511 tx=304 ok=304 full=0
[000006] Heartbeat OK
...
```

## 6. Expected Output — FPB-RA6E2

Identical to FPB-RA4E2 above, with `Board        : FPB-RA6E2` in the banner.

## 7. Interaction Notes

- **Heartbeat cadence:** one line per second (`DEMO_HEARTBEAT_PERIOD_MS = 1000`), driven by `R_BSP_SoftwareDelay()` in a bare-metal main loop — no RTOS is used anywhere in this module or its reference demo.
- **Status line cadence:** every 5th heartbeat (`DEMO_STATUS_EVERY_N_HEARTBEATS = 5`) prints a `StatusGet()` snapshot instead of the plain "Heartbeat OK" line: `queued` (bytes waiting in the ring buffer), `free` (bytes of ring-buffer space remaining), `tx` (total bytes fully transmitted since `ConfigCheck()`), `ok` (total bytes accepted into the queue), `full` (count of rejected buffer-full enqueue attempts). In steady-state operation with no application backlog, `queued` reads 0 and `full` reads 0.
- **Exact `tx`/`ok` byte counts** in the examples above depend on the precise banner text and line lengths in your build and are shown as representative values, not a guaranteed exact count — read your own terminal output for the authoritative numbers.
- **Reconnecting the terminal:** if you open your terminal application after the board has already booted, you will only see output starting from that point — heartbeats already sent are not replayed. Power-cycle or reset the board after connecting your terminal to see the banner.

---

*Return to the [User Manual](BIT_BANGED_UART_TX-USER_MANUAL.md).*
