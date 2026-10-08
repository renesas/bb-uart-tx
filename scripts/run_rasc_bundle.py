#!/usr/bin/env python3

import argparse
import filecmp
import shutil
import subprocess
import sys
from pathlib import Path


def copy_if_changed(src: Path, dst: Path):
    dst.parent.mkdir(parents=True, exist_ok=True)

    if dst.exists():
        try:
            if filecmp.cmp(src, dst, shallow=False):
                return False
        except OSError:
            pass

    shutil.copy2(src, dst)
    return True


def find_bundle_outputs(elf_path: Path):
    """
    RASC typically emits <stem>.rpd and <stem>.sbd next to the ELF.
    """
    stem = elf_path.stem
    rpd = elf_path.with_suffix('.rpd')
    sbd = elf_path.with_suffix('.sbd')
    return rpd, sbd


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rasc', required=True)
    parser.add_argument('--config', required=True)
    parser.add_argument('--elf', required=True)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--devicefamily', required=True)
    parser.add_argument('--outdir', required=True)
    parser.add_argument('--rpd-name', required=True)
    parser.add_argument('--sbd-name', required=True)
    args = parser.parse_args()

    rasc = args.rasc
    config_xml = Path(args.config).resolve()
    elf_path = Path(args.elf).resolve()
    outdir = Path(args.outdir).resolve()
    outdir.mkdir(parents=True, exist_ok=True)

    stdout_file = outdir / 'rasc_bundle_stdout.txt'
    stderr_file = outdir / 'rasc_bundle_stderr.txt'

    cmd = [
        rasc,
        '-nosplash',
        '--launcher.suppressErrors',
        '--gensmartbundleandpartition',
        '--compiler', args.compiler,
        '--devicefamily', args.devicefamily,
        str(config_xml),
        str(elf_path),
        str(stdout_file),
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)

    stdout_file.write_text(result.stdout, encoding='utf-8')
    stderr_file.write_text(result.stderr, encoding='utf-8')

    generated_rpd, generated_sbd = find_bundle_outputs(elf_path)

    missing = []
    if not generated_rpd.exists():
        missing.append(str(generated_rpd))
    if not generated_sbd.exists():
        missing.append(str(generated_sbd))

    if missing:
        print("RASC bundle generation did not produce expected outputs:", file=sys.stderr)
        for m in missing:
            print("  " + m, file=sys.stderr)
        print("\n--- RASC STDOUT ---\n", result.stdout)
        print("\n--- RASC STDERR ---\n", result.stderr, file=sys.stderr)
        sys.exit(result.returncode if result.returncode != 0 else 1)

    changed = []
    dst_rpd = outdir / args.rpd_name
    dst_sbd = outdir / args.sbd_name

    if copy_if_changed(generated_rpd, dst_rpd):
        changed.append(str(dst_rpd))
    if copy_if_changed(generated_sbd, dst_sbd):
        changed.append(str(dst_sbd))

    if result.returncode != 0:
        print("RASC returned non-zero, but .rpd/.sbd were generated. Treating as success.")

    if changed:
        print("Updated bundle outputs:")
        for c in changed:
            print("  " + c)
    else:
        print("Bundle outputs unchanged.")

    sys.exit(0)


if __name__ == '__main__':
    main()