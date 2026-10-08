project(
  'bit_banged_uart_tx',
  'c',
  version: '0.1.0',
  default_options: [
    'c_std=c99',
    'warning_level=2',
  ]
)

# ------------------------------------------------------------
# Pure logic — no FSP dependency.
# Compiles on host (used by host unit tests) and on target.
# ------------------------------------------------------------

bb_uart_pure_sources = files(
  'src/rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.c',
  'src/rs_bb_uart_tx/rs_bb_uart_tx_engine.c',
)

bb_uart_src_inc = include_directories('src')

# Stub include dir for host unit tests
stub_inc = include_directories('tests/stubs')

# Host-testable dep: pure logic + stubs (no HAL, no FSP)
bb_uart_pure_host_dep = declare_dependency(
  sources: bb_uart_pure_sources,
  include_directories: [bb_uart_src_inc, stub_inc],
)

# Full source set for firmware builds (core + HAL; the board data file is
# added per-board in firmware/meson.build).
bb_uart_core_sources = files(
  'src/rs_bb_uart_tx/rs_bb_uart_tx.c',
  'src/rs_bb_uart_tx/rs_bb_uart_tx_engine.c',
  'src/rs_bb_uart_tx/rs_bb_uart_tx_ring_buffer.c',
  'src/rs_bb_uart_tx/rs_bb_uart_tx_hal.c',
)

# ------------------------------------------------------------
# Optional firmware build (cross builds only; not subproject)
# ------------------------------------------------------------

if (meson.is_cross_build()) and (false == meson.is_subproject())
  subdir('firmware')
endif

# ------------------------------------------------------------
# Tests (host only; cross builds skip inside subdir)
# ------------------------------------------------------------

subdir('tests')
