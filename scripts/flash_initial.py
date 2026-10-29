#!/usr/bin/env python3
"""Initial USB provisioning only; use BLE SMP for subsequent update tests."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time

import serial
import serial.tools.list_ports
import yaml


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--port', required=True)
    parser.add_argument('--backup-info', type=Path, required=True)
    parser.add_argument('--log-prefix', type=Path, required=True)
    args = parser.parse_args()
    metadata = json.loads(args.backup_info.read_text())
    backup = args.backup_info.parent / metadata['backup_file']
    if backup.stat().st_size != metadata['flash_bytes'] or \
            hashlib.sha256(backup.read_bytes()).hexdigest() != metadata['sha256']:
        parser.error('Original flash backup size/hash did not match.')
    files = []
    for domain in ['mcuboot', 'firmware']:
        zdir = args.build_dir.resolve() / domain / 'zephyr'
        runners = yaml.safe_load((zdir / 'runners.yaml').read_text())
        image = zdir / runners['config']['bin_file']
        address = next(x.split('=', 1)[1] for x in runners['args']['esp32']
                       if x.startswith('--esp-app-address='))
        if not image.is_file():
            parser.error('Missing build image: ' + str(image))
        files.extend([address, str(image)])
    args.log_prefix.parent.mkdir(parents=True, exist_ok=True)
    command = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3',
               '--port', args.port, '--before', 'usb-reset', '--after', 'watchdog-reset',
               '--baud', '115200', '--connect-attempts', '2', 'write-flash',
               '--erase-all', '--no-progress', '--flash-mode', 'dio',
               '--flash-freq', '80m', '--flash-size', '8MB', *files]
    result = subprocess.run(command, capture_output=True, text=True, timeout=90)
    args.log_prefix.with_suffix('.flash.log').write_text(result.stdout + result.stderr)
    print('\n'.join(line for line in result.stdout.splitlines()
                    if not line.startswith('MAC:')), flush=True)
    if result.returncode:
        print(result.stderr, file=sys.stderr)
        return result.returncode
    print('Capturing application startup and image confirmation for 30 seconds.', flush=True)
    captured = bytearray()
    connection = None
    deadline = time.monotonic() + 30
    try:
        while time.monotonic() < deadline:
            if connection is None:
                ports = [p for p in serial.tools.list_ports.comports()
                         if p.device == args.port]
                if not ports:
                    time.sleep(0.1)
                    continue
                try:
                    connection = serial.Serial()
                    connection.port = args.port
                    connection.baudrate = 115200
                    connection.timeout = 0.2
                    connection.dtr = True
                    connection.rts = False
                    connection.open()
                except (serial.SerialException, OSError):
                    connection = None
                    time.sleep(0.1)
                    continue
            try:
                captured.extend(connection.read(max(1, min(connection.in_waiting, 4096))))
            except (serial.SerialException, OSError):
                connection.close()
                connection = None
    finally:
        if connection is not None:
            connection.close()
    raw = bytes(captured)
    args.log_prefix.with_suffix('.boot.raw').write_bytes(raw)
    clean = re.sub(r'\x1b\[[0-9;?]*[A-Za-z]', '', raw.decode(errors='replace'))
    args.log_prefix.with_suffix('.boot.log').write_text(clean)
    print(clean, flush=True)
    confirmed = 'confirm result=0' in clean or 'confirmed=1' in clean
    if not confirmed or 'Advertising started:' not in clean:
        print('Startup/confirmation not fully verified; inspect the boot log.', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
