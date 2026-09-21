# firmware/generated/meson.build
#
# Runs RASC headless to generate ra_gen / ra_cfg output for the selected board,
# then copies the required files into the Meson build directory so that
# subsequent firmware targets can depend on them as generated sources.
#
# Bare-metal project (r_gpt + r_ioport only): no FreeRTOS kernel sources and
# no RTOS thread files, unlike the RTC Event Logger's equivalent file.

if not meson.is_cross_build()
  subdir_done()
endif

py   = find_program('python3',   required: true)
rasc = find_program('rasc', required: true)

gen_board   = get_option('gen_board')
board       = get_option('board')
mcu         = get_option('mcu')
ra_ordinal  = get_option('ra_ordinal')
ldscript_opt = get_option('ldscript')

# _RA_CORE: empty option = auto (Cortex-M23 on RA2E3, Cortex-M33 on RA4E2/RA6E2)
ra_core = get_option('ra_core')
if ra_core == ''
  ra_core = (mcu == 'ra2e3') ? 'CM23' : 'CM33'
endif

config_xml  = join_paths(meson.project_source_root(), 'firmware', gen_board, 'configuration.xml')
run_rasc_py = join_paths(meson.project_source_root(), 'scripts', 'run_rasc.py')

# Peripheral config header generated for the GPT stack
periph_cfg_header = 'r_gpt_cfg.h'

# ── RASC-generated output list ────────────────────────────────────────────
# Update this list to match what run_rasc.py copies from the RASC projectdir.
# hal_entry.c / hal_warmstart.c are application-owned (taken from the example
# project), not RASC outputs, for this configuration.xml.

rasc_outputs = [
  'common_data.c',      # [0]
  'common_data.h',      # [1]
  'hal_data.c',         # [2]
  'hal_data.h',         # [3]
  'main.c',             # [4]
  'pin_data.c',         # [5]
  'vector_data.c',      # [6]
  'vector_data.h',      # [7]
  'bsp_clock_cfg.h',    # [8]

  'r_ioport_cfg.h',     # [9]
  periph_cfg_header,    # [10]  r_gpt_cfg.h

  'board_cfg.h',        # [11]
  'bsp_cfg.h',          # [12]
  'bsp_linker_info.h',  # [13]
  'bsp_mcu_device_cfg.h',    # [14]
  'bsp_mcu_device_pn_cfg.h', # [15]
  'bsp_mcu_family_cfg.h',    # [16]
  'bsp_mcu_ofs_cfg.h',       # [17]
  'bsp_pin_cfg.h',           # [18]

  'fsp_gen.lld',        # [19]
  'memory_regions.lld', # [20]
  'fsp.lld',            # [21]
]

rasc_gen = custom_target(
  'generate_rasc_' + gen_board,
  input: config_xml,
  output: rasc_outputs,
  command: [
    py,
    run_rasc_py,
    '--rasc', rasc,
    '--config', '@INPUT@',
    '--outdir', '@OUTDIR@',
    '--projectdir', '@PRIVATE_DIR@',
    '--periph-cfg-header', periph_cfg_header,
  ],
  depend_files: run_rasc_py,
  build_by_default: false,
)

# ── Convenience sub-lists referenced by firmware/meson.build ─────────────

# rasc_headers_dep exposes the flat generated directory (board_cfg.h etc.).
# Additional RASC-generated subdirectory include paths are passed as -I c_args
# on the firmware executables in firmware/meson.build, because Meson validates
# include_directories() paths at setup time before RASC has run to create them.
rasc_headers_dep = declare_dependency(
  include_directories: include_directories('.'),
  sources: [
    rasc_gen[1],   # common_data.h
    rasc_gen[3],   # hal_data.h
    rasc_gen[7],   # vector_data.h
    rasc_gen[8],   # bsp_clock_cfg.h
    rasc_gen[9],   # r_ioport_cfg.h
    rasc_gen[10],  # r_gpt_cfg.h
    rasc_gen[11],  # board_cfg.h
    rasc_gen[12],  # bsp_cfg.h
    rasc_gen[13],  # bsp_linker_info.h
    rasc_gen[14],  # bsp_mcu_device_cfg.h
    rasc_gen[15],  # bsp_mcu_device_pn_cfg.h
    rasc_gen[16],  # bsp_mcu_family_cfg.h
    rasc_gen[17],  # bsp_mcu_ofs_cfg.h
    rasc_gen[18],  # bsp_pin_cfg.h
  ],
)

# ── Manual re-run target (useful during development) ─────────────────────

run_target(
  'rasc_generate',
  command: [
    py,
    run_rasc_py,
    '--rasc', rasc,
    '--config', config_xml,
    '--outdir', meson.current_build_dir(),
    '--projectdir', join_paths(meson.current_build_dir(), 'rasc_manual'),
    '--periph-cfg-header', periph_cfg_header,
  ],
)
