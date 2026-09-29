# tests/meson.build

# Unity subproject — used for both host and firmware builds
# '-Dnoreturn=' redefines the C11 noreturn macro to nothing, preventing a
# clash between LLVM 22's stdnoreturn.h (#define noreturn _Noreturn) and the
# Windows SDK stdlib.h (__declspec(noreturn)) when building with clang-cl.
unity_sp = subproject('unity', default_options: [
  'extension_fixture=true',
  'werror=false',
])

unity_dep = unity_sp.get_variable('unity_dep')
unity_inc = unity_sp.get_variable('unity_inc')
unity_src = unity_sp.get_variable('unity_src')

# Host unit tests only — skip for cross (firmware) builds
if meson.is_cross_build()
  subdir_done()
endif

# ── Host-build compile flags ────────────────────────────────────────────────
# Defined HERE (not in a native file) so every host toolchain file builds the
# tests identically:
#
# RS_BB_UART_TX_TEST_HOST_BUILD=1
#   Redirects rs_bb_uart_tx_types.h to tests/stubs/rs_bb_uart_tx_test_types.h
#   instead of bsp_api.h / r_timer_api.h, and turns the ring-buffer barrier
#   into a no-op.
#
# RS_BB_UART_TX_CFG_PRESET_CONFIG_USE=0
#   Compiles out the GPT preset instance in the board data file (it needs
#   r_gpt.h and gpt_extended_cfg_t).  ConfigGen()/ConfigCheck(), which live
#   in the same file, stay compiled and are what the tests exercise.
host_test_cargs = [
  '-DRS_BB_UART_TX_TEST_HOST_BUILD=1',
  '-DRS_BB_UART_TX_CFG_PRESET_CONFIG_USE=0',
]

# ── Source groups ──────────────────────────────────────────────────────────

# Pure logic — no FSP dependency
pure_sources = files(
  '../src/rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.c',
  '../src/rs_bb_uart_tx/rs_bb_uart_tx_engine.c',
)

# Core + board data file (ConfigGen/ConfigCheck).  The board data file is the
# FPB-RA2E3 one; ConfigGen/ConfigCheck logic is identical across boards
# (only the preset GPT instance and default irq_priority differ).
core_sources = files(
  '../src/rs_bb_uart_tx/rs_bb_uart_tx.c',
  '../src/rs_bb_uart_tx/config/rs_bb_uart_tx_data_fpb_ra2e3.c',
)

# Real HAL shim — linked ONLY into the HAL executable
hal_sources = files(
  '../src/rs_bb_uart_tx/rs_bb_uart_tx_hal.c',
)

# ── Executable 1: unittest_on_pc  (the same tests also run on target) ───────
# Core tests use the mock HAL defined inside test_rs_bb_uart_tx_core.c, so
# rs_bb_uart_tx_hal.c is NOT linked here.
test_sources = files(
  'test_main.c',
  'test_rs_bb_uart_tx_ring_buffer.c',
  'test_rs_bb_uart_tx_engine.c',
  'test_rs_bb_uart_tx_core.c',
)

unittest_on_pc_exe = executable(
  'unittest_on_pc',
  sources: [
    test_sources,
    pure_sources,
    core_sources,
    # Note: rs_bb_uart_tx_hal.c is NOT included here; test_rs_bb_uart_tx_core.c
    # provides mock implementations of every rs_bb_uart_tx_hal_* function.
  ],
  include_directories: [
    unity_inc,
    include_directories('../src'),
    include_directories('stubs'),
    include_directories('.'),
  ],
  c_args: host_test_cargs,
  dependencies: [
    unity_dep,
  ],
)

test(
  meson.project_name() + '_unittests',
  unittest_on_pc_exe,
)

# ── Executable 2: unittest_hal_on_pc  (host only) ───────────────────────────
# Links the REAL rs_bb_uart_tx_hal.c (including the GPT ISR callback) against a
# fake FSP layer (fake timer driver + fake R_BSP_Pin*).  Host-only because the
# real R_BSP_Pin* are __STATIC_INLINE and cannot be replaced on target.
hal_test_sources = files(
  'test_main_hal.c',
  'test_rs_bb_uart_tx_hal.c',
  'test_rs_bb_uart_tx_waveform.c',
  'stubs/rs_bb_uart_tx_fake_fsp.c',
)

unittest_hal_on_pc_exe = executable(
  'unittest_hal_on_pc',
  sources: [
    hal_test_sources,
    pure_sources,
    core_sources,
    hal_sources,
  ],
  include_directories: [
    unity_inc,
    include_directories('../src'),
    include_directories('stubs'),
    include_directories('.'),
  ],
  c_args: host_test_cargs,
  dependencies: [
    unity_dep,
  ],
)

test(
  meson.project_name() + '_hal_unittests',
  unittest_hal_on_pc_exe,
)
