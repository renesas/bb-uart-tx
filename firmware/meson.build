# firmware/meson.build
#
# Builds two ELF targets per board:
#   bit_banged_uart_tx_on_<board>_<compiler>.elf          — application firmware (demo)
#   bit_banged_uart_tx_unittest_on_<board>_<compiler>.elf — Unity test firmware
#
# Supports: ra2e3-fpb (Cortex-M23), ra4e2-fpb (Cortex-M33), ra6e2-fpb (Cortex-M33)
#
# Bare-metal (no RTOS).  Board glue that is not part of the reusable PES
# (hal_entry.c, hal_warmstart.c, fault_handler.c, the demo, and — for RA4E2/RA6E2 — the board
# data file) is taken from the matching example project under
# examples/VSCode/BitBangedUART_TX_FPB_<BOARD>/src so nothing is duplicated.
#
# STATUS: adapted from the RTC Event Logger's firmware/meson.build.  Verified
# by inspection only — it needs RASC, the FSP/CMSIS wraps and the ATfE
# toolchain, none of which were available when it was written.

fs = import('fs')

if not meson.is_cross_build()
  subdir_done()
endif

subdir('generated')

# Variables from firmware/generated/meson.build:
#   gen_board, board, mcu, ra_core, ra_ordinal, ldscript_opt
#   rasc_gen[], rasc_headers_dep

# ── Per-board example glue ────────────────────────────────────────────────

board_upper = mcu.to_upper()      # ra2e3 -> RA2E3
example_dir = '../examples/VSCode/BitBangedUART_TX_FPB_' + board_upper + '/src'

# Board data file: FPB-RA2E3 lives in src/, the others in their example project.
if mcu == 'ra2e3'
  board_config_src = files('../src/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.c')
  board_config_inc = []
else
  board_config_src = files(example_dir + '/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_' + mcu + '.c')
  # The board header is included as <rs_bb_uart_tx/config/...>; core headers
  # still resolve from ../src first (include order below).
  board_config_inc = [include_directories(example_dir)]
endif

# hal_warmstart.c — application-owned BSP hook (R_BSP_WarmStart).  RASC does NOT
# generate it for this project (it only writes a template src/hal_entry.c), so
# it is taken from the example project and linked into BOTH ELFs.
hal_warmstart_src = files(example_dir + '/hal_warmstart.c')

# Application-only board glue
hal_entry_src = files(example_dir + '/hal_entry.c')
app_glue_src  = files(
  example_dir + '/fault_handler.c',
  example_dir + '/rs_bb_uart_tx_demo.c',
)

# crt0_stub.c — weak _start stub to satisfy picolibc libc.a's reference to
# crt0.o. The FSP uses Reset_Handler as the true entry point; _start is never
# called. Without this stub, lld reports "cannot open crt0.o".
crt0_stub_src = files('../src/crt0_stub.c')

# ── PES sources ───────────────────────────────────────────────────────────
# bb_uart_core_sources (root meson.build): core + engine + ring buffer + HAL.

# UT build excludes rs_bb_uart_tx_hal.c — test_rs_bb_uart_tx_core.c provides a
# mock HAL that replaces the real HAL for on-target testing (the real one
# needs the un-fakeable __STATIC_INLINE R_BSP_Pin* functions).
bb_uart_core_sources_ut = files(
  '../src/rs_bb_uart_tx/rs_bb_uart_tx.c',
  '../src/rs_bb_uart_tx/rs_bb_uart_tx_engine.c',
  '../src/rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.c',
)

# On-target Unity test sources (replace ra_gen/main.c).  Only the groups that
# run on target: tests/stubs/* and the HAL/ISR/waveform tests are host-only.
test_sources = files(
  '../tests/test_main_firmware.c',
  '../tests/test_rs_bb_uart_tx_ring_buffer.c',
  '../tests/test_rs_bb_uart_tx_engine.c',
  '../tests/test_rs_bb_uart_tx_core.c',
)

unity_target_port_src = files(
  '../tests/target/unity_port_polled_gpio.c',
  '../tests/target/syscalls_min.c',
)

# NOTE: tests/stubs must NOT be on the firmware include path — its r_ioport.h
# shim would shadow the real FSP header.
inc_dirs = [
  include_directories('../src'),
  include_directories('../tests'),
] + board_config_inc

unity_target_cfg_inc = include_directories('../tests/target')

# ── Dependencies ──────────────────────────────────────────────────────────

unity_sp = subproject('unity', default_options: [
  'extension_fixture=true',
  'werror=false',
])

unity_src = unity_sp.get_variable('unity_src')
unity_dep = unity_sp.get_variable('unity_dep')
unity_inc = unity_sp.get_variable('unity_inc')

fsp_sp = subproject('fsp', default_options: [
  'fsp_mcu=' + mcu,
  'fsp_board=' + board,
])
fsp_dep = fsp_sp.get_variable('fsp_dep')

cmsis_sp = subproject('cmsis')
cmsis_dep = cmsis_sp.get_variable('cmsis_dep')

common_defines = [
  '-D_RENESAS_RA_',
  '-D_RA_CORE=' + ra_core,
  '-D_RA_ORDINAL=' + ra_ordinal.to_string(),
]

# TX pin used by the on-target Unity output port (VCOM TXD of each board).
unity_tx_pin = get_option('unity_tx_pin')
if unity_tx_pin == ''
  unity_tx_pin = (mcu == 'ra2e3') ? 'BSP_IO_PORT_01_PIN_09' : 'BSP_IO_PORT_04_PIN_11'
endif

cc = meson.get_compiler('c')
compiler_id = cc.get_id()

project_name     = meson.project_name()
firmware_name    = project_name + '_on_' + board + '_' + compiler_id
firmware_ut_name = project_name + '_unittest_on_' + board + '_' + compiler_id

# ── RASC generated sources ────────────────────────────────────────────────
# rasc_gen indices (see firmware/generated/meson.build):
# [0]=common_data.c [1]=common_data.h [2]=hal_data.c [3]=hal_data.h
# [4]=main.c [5]=pin_data.c [6]=vector_data.c [7]=vector_data.h
rasc_core_sources = [
  rasc_gen[0],   # common_data.c
  rasc_gen[2],   # hal_data.c
  rasc_gen[5],   # pin_data.c
  rasc_gen[6],   # vector_data.c
]

rasc_generated_main = rasc_gen[4]   # main.c (app build only; UT uses test_main_firmware.c)

# ── Linker script selection ───────────────────────────────────────────────

script_dir = join_paths(meson.current_build_dir(), 'generated')

# Include paths for RASC-generated subdirectories.
# These cannot use include_directories() because the dirs are created by RASC
# at compile time (after setup). Passed as -I c_args directly on executables.
rasc_gen_base = join_paths(meson.current_build_dir(), 'generated')
rasc_gen_c_args = [
  '-I' + rasc_gen_base + '/ra_cfg/fsp_cfg',
  '-I' + rasc_gen_base + '/ra_cfg/fsp_cfg/bsp',
  '-I' + rasc_gen_base + '/ra_gen',
]

map_file_app = join_paths(meson.current_build_dir(), firmware_name    + '.map')
map_file_ut  = join_paths(meson.current_build_dir(), firmware_ut_name + '.map')

if compiler_id != 'clang'
  error('Unsupported compiler: ' + compiler_id + ' — RASC generates LLVM (.lld) linker scripts only')
endif

if ldscript_opt != ''
  script = join_paths(meson.project_source_root(), 'firmware', gen_board, ldscript_opt)
else
  script = join_paths(script_dir, 'fsp.lld')
endif

link_args_common = [
  '-T' + script,
  '-Wl,-L' + script_dir,
  '-Wl,-eReset_Handler',
]

link_args_app = link_args_common + ['-Wl,-Map=' + map_file_app]
link_args_ut  = link_args_common + ['-Wl,-Map=' + map_file_ut]

# ── Application firmware ELF ──────────────────────────────────────────────

firmware_elf = executable(
  firmware_name + '.elf',
  native: false,
  sources: hal_entry_src
           + hal_warmstart_src
           + crt0_stub_src
           + rasc_core_sources
           + [rasc_generated_main]
           + bb_uart_core_sources
           + board_config_src
           + app_glue_src,
  include_directories: inc_dirs,
  c_args: common_defines + rasc_gen_c_args,
  link_args: link_args_app,
  dependencies: [
    fsp_dep,
    cmsis_dep,
    rasc_headers_dep,
  ],
  install: false,
)

# ── Unity-on-target firmware ELF ─────────────────────────────────────────
# rs_bb_uart_tx_hal.c is NOT included: test_rs_bb_uart_tx_core.c provides the
# mock HAL (including rs_bb_uart_tx_hal_gpt_callback, which the RASC-generated
# hal_data.c references).  The demo/hal_entry/main.c are not linked either.

firmware_ut_elf = executable(
  firmware_ut_name + '.elf',
  native: false,
  sources: hal_warmstart_src
           + crt0_stub_src
           + rasc_core_sources
           + bb_uart_core_sources_ut
           + board_config_src
           + test_sources
           + unity_src
           + unity_target_port_src,
  include_directories: inc_dirs + [
    unity_inc,
    unity_target_cfg_inc,
  ],
  c_args: common_defines + rasc_gen_c_args + [
    '-DUNITY_INCLUDE_CONFIG_H',
    '-DUNITY_FIXTURE_NO_EXTRAS',
    '-DUNITY_PORT_TX_PIN=' + unity_tx_pin,
  ],
  link_args: link_args_ut + ['-lc'],
  dependencies: [
    fsp_dep,
    cmsis_dep,
    rasc_headers_dep,
  ],
  install: false,
)

# ── Objcopy / SREC ────────────────────────────────────────────────────────

objcopy     = find_program('llvm-objcopy')
objcopy_cmd = [objcopy, '-O', 'srec', '@INPUT@', '@OUTPUT@']

firmware_srec = custom_target(
  'srec_' + firmware_name,
  input: firmware_elf,
  output: firmware_name + '.srec',
  command: objcopy_cmd,
  build_by_default: true,
  install: true,
  install_dir: 'firmware/' + gen_board,
)

firmware_ut_srec = custom_target(
  'srec_' + firmware_ut_name,
  input: firmware_ut_elf,
  output: firmware_ut_name + '.srec',
  command: objcopy_cmd,
  build_by_default: true,
  install: true,
  install_dir: 'firmware/' + gen_board,
)

# ── RFP flash programmer targets ──────────────────────────────────────────
# RA2E3 / RA4E2 / RA6E2 use flat-memory programming — no TrustZone smart
# bundle (scripts/run_rasc_bundle.py) is needed.  It is kept for MCUs that
# require it (RA4M2, RA6M4, RA8D1).
#
# rfp-cli.exe is only needed for the rfp_program / rfp_reset flash targets.
# required: false allows 'meson setup' and 'meson compile' to succeed even
# when Renesas Flash Programmer is not installed.

rfp_cli = find_program('rfp-cli.exe', required: false)

if rfp_cli.found()

  run_target(
    'rfp_program',
    command: [
      rfp_cli, '-d', 'RA', '-t', 'jlink', '-if', 'swd',
      '-file', firmware_srec[0], '-auto', '-run',
    ],
  )
  run_target(
    'rfp_program_ut',
    command: [
      rfp_cli, '-d', 'RA', '-t', 'jlink', '-if', 'swd',
      '-file', firmware_ut_srec[0], '-auto', '-run',
    ],
  )

  run_target('rfp_reset',
    command: [rfp_cli, '-d', 'RA', '-t', 'jlink', '-if', 'swd', '-reset'])

  run_target('rfp_erase_halt',
    command: [rfp_cli, '-d', 'RA', '-t', 'jlink', '-if', 'swd', '-erase', '-halt'])

  run_target('rfp_check_blank',
    command: [rfp_cli, '-d', 'RA', '-t', 'jlink', '-if', 'swd', '-blankcheck'])

endif # rfp_cli.found()
