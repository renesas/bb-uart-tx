#!/usr/bin/env python3
"""
run_rasc.py — Invoke RASC headlessly and copy generated outputs.

Runs the Renesas RA Smart Configurator (RASC) in headless mode to regenerate
ra_gen / ra_cfg files from configuration.xml.  After a successful run it
copies only the files that firmware/generated/meson.build expects, detecting
changes by content-compare so Meson incremental builds are not disturbed.

The Bit-Banged UART TX projects are bare-metal (no RTOS) and configure only
r_gpt + r_ioport, so — unlike the RTC Event Logger's version of this script —
there are no FreeRTOS kernel sources or RTOS thread files to collect.

Usage (called by Meson custom_target):
    python3 run_rasc.py
        --rasc    <path-to-rasc-executable>
        --config  <path-to-configuration.xml>
        --outdir  <destination directory for generated files>
        --projectdir  <working directory for RASC output>
        --periph-cfg-header  <peripheral cfg header, e.g. r_gpt_cfg.h>
"""

import argparse
import filecmp
import shutil
import subprocess
import sys
from pathlib import Path


def copy_if_changed(src: Path, dst: Path) -> bool:
    """Copy src to dst only when content differs.  Returns True if copied."""
    dst.parent.mkdir(parents=True, exist_ok=True)
    if dst.exists():
        try:
            if filecmp.cmp(src, dst, shallow=False):
                return False
        except OSError:
            pass
    shutil.copy2(src, dst)
    return True


def copy_required(src_root: Path, outdir: Path, periph_cfg_header: str):
    """Copy the mandatory RASC outputs to outdir.  Raises on missing files.

    RASC (FSP v6.5.0, this project's configuration.xml) writes only a template
    src/hal_entry.c into its project dir — no hal_warmstart.c.  Both files are
    APPLICATION-owned and come from
    examples/VSCode/BitBangedUART_TX_FPB_<board>/src (see firmware/meson.build),
    so neither is collected here.
    """
    required = {
        # ra_gen — generated C and headers
        'ra_gen/common_data.c':       'common_data.c',
        'ra_gen/common_data.h':       'common_data.h',
        'ra_gen/hal_data.c':          'hal_data.c',
        'ra_gen/hal_data.h':          'hal_data.h',
        'ra_gen/main.c':              'main.c',
        'ra_gen/pin_data.c':          'pin_data.c',
        'ra_gen/vector_data.c':       'vector_data.c',
        'ra_gen/vector_data.h':       'vector_data.h',
        'ra_gen/bsp_clock_cfg.h':     'bsp_clock_cfg.h',

        # ra_cfg — peripheral config headers
        'ra_cfg/fsp_cfg/r_ioport_cfg.h': 'r_ioport_cfg.h',
        f'ra_cfg/fsp_cfg/{periph_cfg_header}': periph_cfg_header,

        # ra_cfg — BSP board / device config headers
        'ra_cfg/fsp_cfg/bsp/board_cfg.h':            'board_cfg.h',
        'ra_cfg/fsp_cfg/bsp/bsp_cfg.h':              'bsp_cfg.h',
        'bsp_linker_info.h':                         'bsp_linker_info.h',
        'ra_cfg/fsp_cfg/bsp/bsp_mcu_device_cfg.h':   'bsp_mcu_device_cfg.h',
        'ra_cfg/fsp_cfg/bsp/bsp_mcu_device_pn_cfg.h':'bsp_mcu_device_pn_cfg.h',
        'ra_cfg/fsp_cfg/bsp/bsp_mcu_family_cfg.h':   'bsp_mcu_family_cfg.h',
        'ra_cfg/fsp_cfg/bsp/bsp_mcu_ofs_cfg.h':      'bsp_mcu_ofs_cfg.h',
        'ra_cfg/fsp_cfg/bsp/bsp_pin_cfg.h':          'bsp_pin_cfg.h',

        # Linker scripts (LLVM LLD format)
        'fsp_gen.lld':        'fsp_gen.lld',
        'memory_regions.lld': 'memory_regions.lld',
        'script/fsp.lld':     'fsp.lld',
    }

    missing = []
    changed = []

    for rel_src, out_name in required.items():
        src = src_root / rel_src
        dst = outdir / out_name
        if not src.exists():
            missing.append(str(src))
            continue
        if copy_if_changed(src, dst):
            changed.append(str(dst))

    if missing:
        raise FileNotFoundError(
            "RASC expected output(s) missing:\n  " + "\n  ".join(missing)
        )

    # Copy subdirectories needed for include paths.  These are passed to the
    # compiler as -I flags by firmware/meson.build (Meson cannot validate
    # include_directories() for directories that only exist after RASC ran).
    subdirs_to_copy = [
        'ra_cfg/fsp_cfg',
        'ra_cfg/fsp_cfg/bsp',
        'ra_gen',
    ]
    for subdir in subdirs_to_copy:
        src_subdir = src_root / subdir
        dst_subdir = outdir / subdir
        if src_subdir.is_dir():
            dst_subdir.mkdir(parents=True, exist_ok=True)
            for f in src_subdir.iterdir():
                if f.is_file():
                    if copy_if_changed(f, dst_subdir / f.name):
                        changed.append(str(dst_subdir / f.name))

    return changed


def main():
    parser = argparse.ArgumentParser(description="Run RASC headlessly and copy outputs.")
    parser.add_argument('--rasc',        required=True,  help='Path to RASC executable')
    parser.add_argument('--config',      required=True,  help='Path to configuration.xml')
    parser.add_argument('--outdir',      required=True,  help='Destination for generated files')
    parser.add_argument('--projectdir',  required=True,  help='RASC working directory')
    parser.add_argument('--periph-cfg-header', required=True,
                        help='Peripheral config header filename (e.g. r_gpt_cfg.h)')
    args = parser.parse_args()

    outdir      = Path(args.outdir).resolve()
    config_xml  = Path(args.config).resolve()
    projectdir  = Path(args.projectdir).resolve()

    outdir.mkdir(parents=True, exist_ok=True)
    projectdir.mkdir(parents=True, exist_ok=True)

    # Standalone rasc.exe from the separate RASC installer
    # (setup_fsp_v6_5_0_rasc_v2026-04.2.exe from github.com/renesas/fsp/releases)
    cmd = [
        args.rasc,
        '-nosplash',
        '--launcher.suppressErrors',
        '--generate',
        str(config_xml),
        '--projectdir',
        str(projectdir),
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)

    (outdir / 'rasc_stdout.txt').write_text(result.stdout, encoding='utf-8')
    (outdir / 'rasc_stderr.txt').write_text(result.stderr, encoding='utf-8')

    # Debug: list what RASC actually generated so we can diagnose path issues
    print("\n--- RASC projectdir contents ---")
    for p in sorted(projectdir.rglob('*')):
        if p.is_file():
            print(f"  {p.relative_to(projectdir)}")
    print("--- end ---\n")

    try:
        changed = copy_required(projectdir, outdir, args.periph_cfg_header)

        if result.returncode != 0:
            print("RASC returned non-zero but all expected outputs were generated — treating as success.")

        if changed:
            print("RASC updated files:")
            for p in changed:
                print(f"  {p}")
        else:
            print("RASC outputs unchanged.")

        sys.exit(0)

    except Exception as exc:
        print("RASC output validation failed.", file=sys.stderr)
        print(str(exc), file=sys.stderr)
        print("\n--- RASC STDOUT ---\n", result.stdout)
        print("\n--- RASC STDERR ---\n", result.stderr, file=sys.stderr)
        sys.exit(result.returncode if result.returncode != 0 else 1)


if __name__ == '__main__':
    main()
