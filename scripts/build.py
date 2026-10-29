#!/usr/bin/env python3
"""Build the pinned Zephyr app; the private signing key stays outside the repo."""
import argparse
import os
import re
import shutil
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', required=True, type=Path)
    parser.add_argument('--key', type=Path,
                        default=Path.home() / '.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem')
    parser.add_argument('--fault', action='store_true')
    parser.add_argument('--counselor', action='store_true')
    parser.add_argument('--version', help='Build an isolated test version, e.g. 1.2.0')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    zephyr_workspace = Path(os.environ.get('BUDDYGUARD_ZEPHYR_WORKSPACE', Path.home() / 'zephyrproject'))
    west = zephyr_workspace / '.venv/bin/west'
    if not args.key.is_file():
        parser.error('Signing key missing; generate a team key outside the repository first.')
    if root == args.key.resolve() or root in args.key.resolve().parents:
        parser.error('The private signing key must be outside the repository.')
    build_dir = args.build_dir.resolve()
    if ' ' in str(build_dir) or ' ' in str(root):
        parser.error('Use paths without spaces with this toolchain.')
    env = os.environ.copy()
    env['PATH'] = str(west.parent) + os.pathsep + env['PATH']
    # The pinned SPI bitbang driver dereferences NULL dummy TX buffers, which
    # SX126x register reads legitimately use. Apply only the checked patch;
    # preserve other workspace changes and accept an already-applied patch.
    patch = root / 'patches/zephyr-spi-bitbang-null-buffer.patch'
    zephyr_source = zephyr_workspace / 'zephyr'
    check = subprocess.run(['git', 'apply', '--unidiff-zero', '--check', str(patch)],
                           cwd=zephyr_source, capture_output=True)
    if check.returncode == 0:
        subprocess.run(['git', 'apply', '--unidiff-zero', str(patch)], cwd=zephyr_source, check=True)
    else:
        applied = subprocess.run(['git', 'apply', '--unidiff-zero', '--reverse', '--check', str(patch)],
                                 cwd=zephyr_source, capture_output=True)
        if applied.returncode != 0:
            parser.error('SPI compatibility patch does not match this Zephyr checkout; resolve it before building.')
    application = root / 'firmware'
    if args.version:
        if not re.fullmatch(r'[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}', args.version):
            parser.error('--version must be major.minor.patch')
        major, minor, patch = map(int, args.version.split('.'))
        if max(major, minor, patch) > 255:
            parser.error('Version components must fit in 0..255')
        application = build_dir.parent / (build_dir.name + '-source') / 'firmware'
        shutil.copytree(root / 'firmware', application, dirs_exist_ok=True)
        version_file = application / 'VERSION'
        version_file.write_text(f'VERSION_MAJOR = {major}\nVERSION_MINOR = {minor}\n'
                                f'PATCHLEVEL = {patch}\nVERSION_TWEAK = 0\nEXTRAVERSION =\n')
    cmd = [str(west), 'build', '-b', 'xiao_esp32s3/esp32s3/procpu', '--sysbuild',
           str(application), '-d', str(build_dir), '--',
           '-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE="' + str(args.key.resolve()) + '"']
    overlays = []
    if args.fault:
        overlays.append('conf/fault.conf')
    if args.counselor:
        overlays.append('conf/counselor.conf')
    if overlays:
        cmd.append('-DEXTRA_CONF_FILE=' + ';'.join(overlays))
    return subprocess.call(cmd, cwd=zephyr_workspace, env=env)


if __name__ == '__main__':
    sys.exit(main())
