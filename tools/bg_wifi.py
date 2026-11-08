#!/usr/bin/env python3
"""Provision Wi-Fi and the cloud key over USB without echoing credentials.

Run once per board. NVS persists the profile across Wi-Fi OTA and reboot.
密码只从本机私有文件读取；不放入命令行参数、不输出原始串口响应。
"""
import argparse
import json
import os
from pathlib import Path
import re
import time

import serial
from bg_fleet import ANSI, verify_device


def read_profile(path):
    if os.name == 'posix' and path.stat().st_mode & 0o077:
        raise ValueError('Wi-Fi profile must be private: chmod 600 ' + str(path))
    config = json.loads(path.read_text())
    ssid, password = config.get('ssid', ''), config.get('password', '')
    if config.get('security') != 'psk' or not isinstance(ssid, str) or \
            not isinstance(password, str) or not 1 <= len(ssid.encode()) <= 32 or \
            not 8 <= len(password.encode()) <= 63 or '\x00' in ssid + password:
        raise ValueError('Need security=psk, SSID of 1..32 bytes and password of 8..63 bytes')
    return ssid, password


def exchange(connection, command, required=None):
    connection.reset_input_buffer()
    # The deployed USB shell has a 64-byte RX ring; pace long provisioning lines.
    wire = (command + '\r\n').encode()
    for offset in range(0, len(wire), 16):
        connection.write(wire[offset:offset + 16])
        connection.flush()
        time.sleep(0.02)
    connection.flush()
    output, last_data = b'', time.monotonic()
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        data = connection.read(max(1, min(connection.in_waiting, 4096)))
        if data:
            output += data
            last_data = time.monotonic()
        clean = ANSI.sub(b'', output)
        complete = required in clean if required else clean.rstrip().endswith(b'uart:~$')
        if complete and time.monotonic() - last_data >= 0.35:
            return clean.decode(errors='replace')
    # Do not include the command or response: they may contain a credential.
    label = ' '.join(command.split()[:2])
    raise RuntimeError(f'USB command did not complete ({label}); check firmware and other serial readers')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True)
    p.add_argument('--device', required=True, choices=('BG-T01', 'BG-T02', 'BG-T03', 'BG-C01'))
    p.add_argument('--profile', type=Path, default=Path.home() / '.config/buddyguard/wifi.json')
    p.add_argument('--key-file', type=Path, default=Path.home() / '.config/buddyguard/memfault-project-key')
    p.add_argument('--status', action='store_true', help='Read Wi-Fi MAC and provisioning status only')
    args = p.parse_args()
    if not args.status:
        ssid, password = read_profile(args.profile)
        key = args.key_file.read_text().strip()
        if not re.fullmatch(r'[A-Za-z0-9]{32}', key):
            p.error('Expected a 32-character Memfault Project Key')
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    connection.port, connection.dtr, connection.rts = args.port, True, False
    connection.exclusive = True
    with connection:
        exchange(connection, '\r\nshell echo on')  # Finish a partial line, then recover echo.
        verify_device(connection, args.device)
        status = exchange(connection, 'bg_wifi status', b'wifi configured=')
        match = re.search(r'wifi configured=[01] MAC=[0-9A-F:]{17}', status)
        if not match:
            raise RuntimeError('Wi-Fi firmware not installed; no credentials sent')
        print(args.device + ': ' + match[0], flush=True)
        if args.status:
            return
        print('Checking image confirmation...', flush=True)
        if 'confirmed=1' not in exchange(connection, 'bg status', b'confirmed='):
            raise RuntimeError('Wait for the new image to confirm before provisioning/rebooting')
        try:
            print('Disabling console echo...', flush=True)
            # With echo off Zephyr may omit its prompt; require an explicit status ACK.
            if 'Echo status: off' not in exchange(connection, 'shell echo off\r\nshell echo', b'Echo status: off'):
                raise RuntimeError('Could not disable echo; no credentials sent')
            print('Saving Wi-Fi profile...', flush=True)
            result = exchange(connection, 'bg_wifi provision ' + ssid.encode().hex() + ' ' +
                              password.encode().hex() + ' ' + key, b'Wi-Fi provision result=')
            if 'Wi-Fi provision result=0;' not in result:
                raise RuntimeError('Board rejected provisioning; no credential values printed')
        finally:
            exchange(connection, '\r\nshell echo on')
        print(f'{args.device}: Wi-Fi profile saved; rebooting to connect', flush=True)
        connection.write(b'bg reboot\r\n')
        connection.flush()


if __name__ == '__main__':
    try:
        main()
    except (ValueError, RuntimeError, OSError, serial.SerialException) as exc:
        raise SystemExit(str(exc))
