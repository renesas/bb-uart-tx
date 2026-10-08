project('fsp', 'c')

fsp_mcu   = get_option('fsp_mcu')
fsp_board = get_option('fsp_board')

fsp_src = 'ra/fsp/src'

bsp_all  = fsp_src + '/bsp/mcu/all'
bsp_mcu  = fsp_src + '/bsp/mcu/' + fsp_mcu
board_dir = 'ra/board/' + fsp_board

fsp_sources = files(
  # modules — the Bit-Banged UART TX uses exactly two: GPIO port + GPT timer
  fsp_src + '/r_ioport/r_ioport.c',
  fsp_src + '/r_gpt/r_gpt.c',

  # BSP common
  bsp_all + '/bsp_clocks.c',
  bsp_all + '/bsp_common.c',
  bsp_all + '/bsp_delay.c',
  bsp_all + '/bsp_group_irq.c',
  bsp_all + '/bsp_guard.c',
  bsp_all + '/bsp_ipc.c',
  bsp_all + '/bsp_io.c',
  bsp_all + '/bsp_irq.c',
  bsp_all + '/bsp_macl.c',
  bsp_all + '/bsp_ospi_b.c',
  bsp_all + '/bsp_register_protection.c',
  bsp_all + '/bsp_sbrk.c',
  bsp_all + '/bsp_sdram.c',
  bsp_all + '/bsp_security.c',

  # MCU specific
  bsp_mcu + '/bsp_linker.c',

  # CMSIS device
  fsp_src + '/bsp/cmsis/Device/RENESAS/Source/startup.c',
  fsp_src + '/bsp/cmsis/Device/RENESAS/Source/system.c',

  # board
  board_dir + '/board_init.c',
  board_dir + '/board_leds.c',
)

fsp_inc = include_directories(
  'ra/fsp/inc',
  'ra/fsp/inc/api',
  'ra/fsp/inc/instances',
)

fsp_dep = declare_dependency(
  sources: fsp_sources,
  include_directories: fsp_inc,
)

fsp_headers_dep = declare_dependency(
  include_directories: fsp_inc,
)
