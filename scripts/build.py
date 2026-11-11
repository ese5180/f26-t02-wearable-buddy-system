#!/usr/bin/env python3
"""Build role-specific, signed firmware against the pinned Zephyr workspace.
构建入口：固定依赖与角色，生成配置，签名并验证产物。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

BOARD = 'xiao_esp32s3/esp32s3/procpu'
ROOT = Path(__file__).resolve().parents[1]


def apply_dependency_patch(module, patch):
    """Apply a recorded prerequisite once; reject conflicting local changes."""
    command = ['git', '-C', str(module), 'apply']
    if subprocess.run(command + ['--reverse', '--check', str(patch)], capture_output=True).returncode == 0:
        return
    check = subprocess.run(command + ['--check', str(patch)], capture_output=True, text=True)
    if check.returncode:
        raise RuntimeError('Dependency patch cannot be applied; preserve local edits and inspect: ' + check.stderr)
    subprocess.run(command + [str(patch)], check=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir', required=True, type=Path)
    p.add_argument('--workspace', type=Path, help='West workspace containing zephyr/ and .venv/')
    p.add_argument('--key', type=Path, default=Path.home() / '.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem')
    p.add_argument('--role', choices=('tag', 'counselor'))
    p.add_argument('--counselor', action='store_true', help='Compatibility alias for --role counselor')
    p.add_argument('--tag-id', type=int, choices=range(1, 4), default=None)
    p.add_argument('--imu', action='store_true', help='Tag only: enable SparkFun LSM6DSO on I2C D4/D5')
    p.add_argument('--fleet', action='store_true', help='Enable Memfault diagnostics via the USB host gateway')
    p.add_argument('--wifi', action='store_true', help='Enable direct Wi-Fi HTTPS telemetry/OTA (implies --fleet)')
    p.add_argument('--heartbeat-seconds', type=int, choices=(15, 60),
                   help='Fleet A/B metric collection interval: A=60 (default), B=15')
    p.add_argument('--serial-recovery', action='store_true', help='Build MCUboot with UART0 recovery (requires separate USB bootloader installation)')
    p.add_argument('--low-power', action='store_true', help='Closed-loop low-power profile: CPU 80 MHz')
    p.add_argument('--fault', action='store_true', help='Rollback test ONLY: stop feeding WDT before confirmation')
    p.add_argument('--version', help='Isolated major.minor.patch override; does not edit source VERSION')
    args = p.parse_args()
    args.fleet = args.fleet or args.wifi
    if args.heartbeat_seconds is not None and not args.fleet:
        p.error('--heartbeat-seconds requires --fleet or --wifi')
    if args.counselor and args.role == 'tag':
        p.error('--counselor conflicts with --role tag')
    role = args.role or ('counselor' if args.counselor else 'tag')
    if role == 'counselor' and args.tag_id is not None:
        p.error('--tag-id applies only to the tag role')
    if role == 'counselor' and args.imu:
        p.error('--imu applies only to the tag role')
    tag = 200 if role == 'counselor' else (args.tag_id or 1)
    workspace = args.workspace or os.environ.get('BUDDYGUARD_ZEPHYR_WORKSPACE')
    if workspace is None:
        workspace = ROOT.parent if (ROOT.parent / '.west').is_dir() else Path.home() / 'zephyrproject'
    workspace = Path(workspace).expanduser().resolve()
    west = workspace / '.venv/bin/west'
    if not west.is_file() or not (workspace / 'zephyr').is_dir():
        p.error('Workspace needs zephyr/ and .venv/bin/west; see docs/en/GETTING_STARTED.md')
    key = args.key.expanduser().resolve()
    if not key.is_file() or ROOT in key.parents:
        p.error('Signing key must exist OUTSIDE the repository; never put a private key in Git.')
    build = args.build_dir.expanduser().resolve()
    if any(' ' in str(x) for x in (ROOT, workspace, build, key)):
        p.error('Use source, workspace, build and signing-key paths without spaces.')
    if build == ROOT or ROOT in build.parents:
        p.error('Keep build directories outside the source repository.')
    pinned = re.search(r'revision:\s*(\w+)', (ROOT / 'west.yml').read_text()).group(1)
    actual = subprocess.check_output(['git', '-C', str(workspace / 'zephyr'), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != pinned:
        p.error(f'Zephyr HEAD {actual} differs from west.yml {pinned}; run west update in the documented workspace.')
    if args.serial_recovery:
        apply_dependency_patch(workspace / 'bootloader/mcuboot', ROOT / 'patches/mcuboot-esp32-uart-threading.patch')
    if args.wifi:
        apply_dependency_patch(workspace / 'zephyr', ROOT / 'patches/zephyr-shared-heap-lock.patch')
    memfault = workspace / 'modules/lib/memfault-firmware-sdk'
    if args.fleet:
        expected = '65f578b3bc767ffd6cbcb0cc6828992780761b84'
        if not (memfault / 'zephyr/module.yml').is_file():
            p.error('Memfault SDK is missing; update dependencies using this branch west.yml (see FLEET_MANAGEMENT.md).')
        revision = subprocess.check_output(['git', '-C', str(memfault), 'rev-parse', 'HEAD'], text=True).strip()
        if revision != expected:
            p.error(f'Memfault SDK must be pinned to {expected}; found {revision}')
    profile = {'board': BOARD, 'role': role, 'tag_id': tag, 'fault': args.fault, 'imu': args.imu,
               'serial_recovery': args.serial_recovery, 'key': str(key)}
    if args.fleet:
        profile['fleet'] = True
        profile['heartbeat_seconds'] = args.heartbeat_seconds or 60
    if args.wifi:
        profile['wifi'] = True
    if args.low_power:
        profile['low_power'] = True
    marker = build.with_name(build.name + '.buddyguard.json')
    if marker.exists() and json.loads(marker.read_text()) != profile:
        p.error('Build directory belongs to another role/ID/key/fault profile. Choose a new build directory.')
    if build.exists() and any(build.iterdir()) and not marker.exists():
        p.error('Existing build directory has no BuddyGuard profile. Choose a new, empty build directory.')
    build.parent.mkdir(parents=True, exist_ok=True)
    application = ROOT / 'firmware'
    if args.version:
        if not re.fullmatch(r'\d{1,3}\.\d{1,3}\.\d{1,3}', args.version) or max(map(int, args.version.split('.'))) > 255:
            p.error('--version must be major.minor.patch with components in 0..255')
        snapshot = build.with_name(build.name + '-source')
        owned = snapshot / '.buddyguard-generated'
        if snapshot.exists() and not owned.exists():
            p.error(f'Refusing to overwrite an unowned source snapshot: {snapshot}')
        application = snapshot / 'firmware'
        if application.exists():
            shutil.rmtree(application)
        shutil.copytree(ROOT / 'firmware', application)
        owned.touch()
        major, minor, patch = map(int, args.version.split('.'))
        (application / 'VERSION').write_text(f'VERSION_MAJOR = {major}\nVERSION_MINOR = {minor}\nPATCHLEVEL = {patch}\nVERSION_TWEAK = 0\nEXTRAVERSION =\n')
    elif marker.exists() and (build / 'CMakeCache.txt').exists():
        cache = (build / 'CMakeCache.txt').read_text()
        if str(build.with_name(build.name + '-source')) in cache:
            p.error('This build uses an isolated version snapshot; keep --version or choose a fresh directory.')
    config = build.with_name(build.name + '.profile.conf')
    config.write_text(f'CONFIG_BUDDYGUARD_COUNSELOR={"y" if role == "counselor" else "n"}\n'
        f'CONFIG_BT_OBSERVER={"y" if role == "counselor" else "n"}\n'
        f'CONFIG_BUDDYGUARD_TAG_ID={tag}\n'
        f'CONFIG_BT_DEVICE_NAME="BG-{ "C01" if role == "counselor" else f"T{tag:02d}" }"\n'
        f'CONFIG_BUDDYGUARD_FAULT_ON_BOOT={"y" if args.fault else "n"}\n'
        f'CONFIG_BUDDYGUARD_IMU={"y" if args.imu else "n"}\n')
    marker.write_text(json.dumps(profile, indent=2) + '\n')
    env = os.environ.copy()
    env['PATH'] = str(west.parent) + os.pathsep + env['PATH']
    overlays = [str(application / 'conf/lsm6dso.overlay')] if args.imu else []
    if args.wifi:
        overlays.append(str(application / 'conf/wifi.overlay'))
    if args.low_power:
        overlays.append(str(application / 'conf/lowpower.overlay'))
    # The generated role profile is always applied last.
    base_confs = ([str(application / 'conf/lowpower.conf')] if args.low_power else []) + [str(config)]
    cmd = [str(west), 'build', '-b', BOARD, '--sysbuild', str(application), '-d', str(build), '--',
           '-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE="' + str(key) + '"', '-DEXTRA_CONF_FILE=' + ';'.join(base_confs),
           '-Dfirmware_EXTRA_DTC_OVERLAY_FILE=' + ';'.join(overlays)]
    if args.fleet:
        serial = 'BG-C01' if role == 'counselor' else f'BG-T{tag:02d}'
        with config.open('a') as stream:
            stream.write(f'CONFIG_BUDDYGUARD_DEVICE_SERIAL="{serial}"\n')
            stream.write(f'CONFIG_MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS={profile["heartbeat_seconds"]}\n')
        fleet_configs = [str(application / 'conf/fleet.conf')]
        if args.wifi:
            fleet_configs.append(str(application / 'conf/wifi.conf'))
        if args.low_power:
            fleet_configs.append(str(application / 'conf/lowpower.conf'))
        # Apply the generated role/experiment profile last, after overlay defaults.
        fleet_configs.append(str(config))
        cmd.extend(['-DEXTRA_CONF_FILE=' + ';'.join(fleet_configs),
                    '-DZEPHYR_EXTRA_MODULES=' + str(memfault)])
    boot_confs = [str(application / 'sysbuild/mcuboot.conf')]
    if args.serial_recovery:
        boot_confs.append(str(application / 'conf/mcuboot-serial.conf'))
    cmd.extend(['-Dmcuboot_EXTRA_CONF_FILE=' + ';'.join(boot_confs),
                '-Dmcuboot_EXTRA_DTC_OVERLAY_FILE=' +
                (str(application / 'conf/mcuboot-serial.overlay') if args.serial_recovery else '')])
    rc = subprocess.call(cmd, cwd=workspace, env=env)
    if rc:
        return rc
    image = build / 'firmware/zephyr/zephyr.signed.bin'
    public = subprocess.check_output([str(west.parent / 'imgtool'), 'getpub', '-k', str(key), '-e', 'pem'])
    public_path = build / 'signing-public.pem'
    public_path.write_bytes(public)
    subprocess.run([str(west.parent / 'imgtool'), 'verify', '-k', str(public_path), str(image)], check=True)
    report = {k: v for k, v in profile.items() if k != 'key'}
    report.update(zephyr_revision=actual, version_file=(application / 'VERSION').read_text(),
        image=str(image), signed_bytes=image.stat().st_size,
        sha256=hashlib.sha256(image.read_bytes()).hexdigest(),
        public_key_sha256=hashlib.sha256(public).hexdigest())
    boot = build / 'mcuboot/zephyr/zephyr.bin'
    boot_config = (build / 'mcuboot/zephyr/.config').read_text()
    if args.serial_recovery and not all(flag in boot_config for flag in
            ('CONFIG_MCUBOOT_SERIAL=y', 'CONFIG_BOOT_SERIAL_UART=y', 'CONFIG_BOOT_VALIDATE_SLOT0=y',
             'CONFIG_MULTITHREADING=y')):
        raise RuntimeError('Recovery/primary verification flags missing from MCUboot output')
    if boot.stat().st_size > 0x10000:
        raise RuntimeError('MCUboot exceeds the unchanged 64 KiB boot partition; do not flash')
    report.update(bootloader_bytes=boot.stat().st_size,
                  bootloader_sha256=hashlib.sha256(boot.read_bytes()).hexdigest())
    if args.fleet:
        # Reserve the final 128 KiB + 64 KiB trailer of secondary for diagnostics.
        # Keep images below this bound so a stored update/revert image is not damaged.
        if image.stat().st_size > 0x150000 - 0x20000 - 0x10000:
            raise RuntimeError('Fleet image overlaps the secondary coredump reserve; do not flash')
        report.update(memfault_revision=revision,
                      symbols=str(build / 'firmware/zephyr/zephyr.elf'))
    if args.serial_recovery:
        report['recovery_patch_sha256'] = hashlib.sha256(
            (ROOT / 'patches/mcuboot-esp32-uart-threading.patch').read_bytes()).hexdigest()
    if args.wifi:
        report['wifi_heap_patch_sha256'] = hashlib.sha256(
            (ROOT / 'patches/zephyr-shared-heap-lock.patch').read_bytes()).hexdigest()
    (build / 'buddyguard-build.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
