# env.ps1 — Set up PATH for Bit-Banged UART TX Meson builds on Windows.
# Run this at the start of every build session: . .\scripts\env.ps1
#
# PATH prepend order matters: last prepended = first searched.
# ATfE is prepended first (lower priority) so generic LLVM wins for clang-cl.exe.
# Generic LLVM is prepended last (higher priority) so clang-cl.exe resolves to
# x86_64-pc-windows-msvc (needed for host unit tests), not the ATfE Arm build.
#
# EDIT THE PATHS BELOW to match your installation.

# LLVM ATfE (Arm Toolchain for Embedded) — bundled with e2studio FSP v6.4.0.
# Provides clang.exe + bare-metal sysroot for Cortex-M23/M33 firmware cross builds.
$env:PATH = "C:\Users\MyPC\AppData\Local\Programs\Renesas\RA\e2studio_v2025-12_fsp_v6.4.0\toolchains\llvm_arm\ATfE-21.1.1-Windows-x86_64\bin;" + $env:PATH

# LLVM generic — provides clang-cl.exe (x86_64) for host unit tests.
$env:PATH = "C:\Program Files\LLVM\bin;" + $env:PATH

# Python Scripts — provides meson.exe and ninja.exe (installed via pip).
$env:PATH = "C:\Users\MyPC\AppData\Local\Programs\Python\Python311\Scripts;" + $env:PATH
$env:PATH = "C:\Users\MyPC\AppData\Local\Programs\Python\Python311;" + $env:PATH

# Standalone RASC — provides rasc.exe for headless FSP code generation (firmware builds only).
$env:PATH = "C:\Renesas\RA\sc_v2026-04.2_fsp_v6.5.0\eclipse;" + $env:PATH

# Renesas Flash Programmer CLI — provides rfp-cli.exe for flashing over JLink/SWD.
$env:PATH = "C:\Program Files (x86)\Renesas Electronics\Programming Tools\Renesas Flash Programmer V3.23;" + $env:PATH
