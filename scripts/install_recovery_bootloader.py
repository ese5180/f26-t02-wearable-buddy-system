#!/usr/bin/env python3
"""Install only MCUboot after verifying a current full backup; preserve all bytes above 64 KiB.
核对备份后只安装引导程序；默认预检查，显式 --write 才写入。
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import serial.tools.list_ports

ROOT = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def preflight(build, backup_info):
    metadata = json.loads(backup_info.read_text())
    original = (backup_info.parent / metadata['backup_file']).read_bytes()
    if metadata['flash_bytes'] != 0x800000 or len(original) != 0x800000 or digest(original) != metadata['sha256']:
        raise ValueError('Full 8 MiB backup size/hash mismatch')
    if not metadata.get('device_usb_serial'):
        raise ValueError('A fresh backup with USB device identity is required')
    report = json.loads((build / 'buddyguard-build.json').read_text())
    if report['board'] != 'xiao_esp32s3/esp32s3/procpu' or not report.get('serial_recovery'):
        raise ValueError('Expected the XIAO ESP32S3 --serial-recovery build')
    boot_dir = build / 'mcuboot/zephyr'
    config = (boot_dir / '.config').read_text()
    for flag in ('CONFIG_MCUBOOT_SERIAL=y', 'CONFIG_BOOT_SERIAL_UART=y',
                 'CONFIG_BOOT_VALIDATE_SLOT0=y', 'CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256=y',
                 'CONFIG_MULTITHREADING=y'):
        if flag not in config:
            raise ValueError('Missing bootloader safeguard: ' + flag)
    public = (build / 'signing-public.pem').read_bytes()
    if digest(public) != digest((ROOT / 'keys/team2-signing-public.pem').read_bytes()):
        raise ValueError('Build signing identity differs from the existing Team2 root')
    image = (boot_dir / 'zephyr.bin').read_bytes()
    if not 0 < len(image) <= 0x10000 or image[0] != 0xe9:
        raise ValueError('Invalid/oversized ESP32S3 bootloader binary')
    if digest(image) != report['bootloader_sha256']:
        raise ValueError('Bootloader differs from its build report')
    if image[2:4] != original[2:4]:
        raise ValueError('Flash mode/frequency/size header differs from the known-good bootloader')
    return metadata, original, image


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build-dir', type=Path, required=True)
    p.add_argument('--backup-info', type=Path, required=True)
    p.add_argument('--port', required=True, help='Native XIAO USB, NOT the TTL adapter')
    p.add_argument('--output-dir', type=Path, required=True)
    p.add_argument('--write', action='store_true', help='Actually write; omission performs preflight only')
    a = p.parse_args()
    try:
        meta, original, image = preflight(a.build_dir.resolve(), a.backup_info.resolve())
    except (ValueError, KeyError, OSError) as error:
        p.error(str(error))
    ports = [x for x in serial.tools.list_ports.comports() if x.device == a.port]
    if len(ports) != 1 or ports[0].vid != 0x303a or ports[0].pid != 0x1001:
        p.error('Select the native ESP32S3 USB JTAG/serial port')
    if (ports[0].serial_number or '').lower() != meta['device_usb_serial'].lower():
        p.error('Backup identity does not match the target USB device')
    output = a.output_dir.expanduser().resolve()
    if output == ROOT or ROOT in output.parents:
        p.error('Keep flash captures outside the repository')
    if output.exists():
        p.error('Output directory already exists; preserve previous evidence and choose a new one')
    print(json.dumps({'preflight': 'passed', 'offset': '0x000000',
                      'bootloader_bytes': len(image), 'partition_bytes': 65536,
                      'bootloader_sha256': digest(image), 'write': a.write}), flush=True)
    if not a.write:
        return 0
    output.mkdir(parents=True)
    (output / 'old-mcuboot-64k.bin').write_bytes(original[:0x10000])
    base = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', a.port,
            '--before', 'usb-reset', '--baud', '115200', '--connect-attempts', '2']
    boot = a.build_dir.resolve() / 'mcuboot/zephyr/zephyr.bin'
    command = base + ['--after', 'no-reset', 'write-flash', '--no-progress',
                     '--flash-mode', 'keep', '--flash-freq', 'keep', '--flash-size', 'keep',
                     '0x0', str(boot)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    (output / 'bootloader-write.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        print('Write failed; target may remain in ROM loader. Inspect external logs.', file=sys.stderr)
        return result.returncode
    readback = output / 'flash-after-bootloader.bin'
    result = subprocess.run(base + ['--after', 'watchdog-reset', 'read-flash',
        '0', '0x800000', str(readback), '--no-progress'], capture_output=True, text=True, timeout=240)
    (output / 'readback.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        print('Readback failed; do not continue UART tests before verification.', file=sys.stderr)
        return result.returncode
    actual = readback.read_bytes()
    verification = dict(bootloader_matches=actual[:len(image)] == image,
        remaining_flash_preserved=actual[0x10000:] == original[0x10000:],
        original_sha256=digest(original), installed_sha256=digest(actual),
        bootloader_sha256=digest(image), bootloader_bytes=len(image))
    (output / 'verification.json').write_text(json.dumps(verification, indent=2) + '\n')
    print(json.dumps(verification), flush=True)
    return 0 if verification['bootloader_matches'] and verification['remaining_flash_preserved'] else 1


if __name__ == '__main__':
    sys.exit(main())
