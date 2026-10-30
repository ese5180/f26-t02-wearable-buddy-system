#!/usr/bin/env python3
"""Read the complete 8 MiB flash before FIRST USB provisioning; never overwrites."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

import serial.tools.list_ports


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True)
    p.add_argument('--output-dir', required=True, type=Path)
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    directory = args.output_dir.expanduser().resolve()
    if directory == root or root in directory.parents:
        p.error('Store full-flash backups outside the repository.')
    if directory.exists():
        p.error('Output directory already exists; choose a NEW directory to preserve previous backups.')
    ports = [port for port in serial.tools.list_ports.comports() if port.device == args.port]
    if len(ports) != 1:
        p.error('Selected USB port is not available.')
    directory.mkdir(parents=True)
    image = directory / 'original-flash-8mb.bin'
    command = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', args.port,
               '--before', 'usb-reset', '--after', 'watchdog-reset', '--baud', '460800',
               '--connect-attempts', '2', 'read-flash', '0', '0x800000', str(image), '--no-progress']
    result = subprocess.run(command, capture_output=True, text=True, timeout=240)
    log = result.stdout + result.stderr
    (directory / 'backup.log').write_text(log)
    if result.returncode or not image.exists() or image.stat().st_size != 0x800000:
        print('Backup failed; do NOT flash. Inspect ' + str(directory / 'backup.log'), file=sys.stderr)
        return 1
    mac = re.search(r'MAC:\s*([0-9a-fA-F:]{17})', log)
    metadata = {'backup_file': image.name, 'flash_bytes': image.stat().st_size,
                'sha256': hashlib.sha256(image.read_bytes()).hexdigest(),
                'device_usb_serial': ports[0].serial_number}
    if mac:
        metadata['device_mac'] = mac.group(1).lower()
    (directory / 'backup-info.json').write_text(json.dumps(metadata, indent=2) + '\n')
    print('Backup verified: ' + str(directory / 'backup-info.json'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
