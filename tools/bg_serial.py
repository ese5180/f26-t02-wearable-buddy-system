#!/usr/bin/env python3
"""Read BuddyGuard logs or issue its USB-shell development commands."""
import argparse
from pathlib import Path
import re
import time

import serial
import serial.tools.list_ports


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port')
    parser.add_argument('--log', type=Path)
    sub = parser.add_subparsers(dest='operation', required=True)
    monitor = sub.add_parser('monitor')
    monitor.add_argument('--seconds', type=float, default=20)
    monitor.add_argument('--until', help='Stop after this text appears in new serial output')
    command = sub.add_parser('command')
    command.add_argument('--no-read', action='store_true',
                         help='Write only while a separate monitor owns the RX stream')
    command.add_argument('words', nargs='+')
    args = parser.parse_args()
    if args.port is None:
        ports = [p.device for p in serial.tools.list_ports.comports()
                 if p.vid == 0x303A and p.pid == 0x1001]
        if len(ports) != 1:
            parser.error('Specify --port when zero or multiple Espressif boards are attached.')
        args.port = ports[0]
    connection = serial.Serial()
    connection.port = args.port
    connection.baudrate = 115200
    connection.timeout = 0.2
    connection.dtr = True
    connection.rts = False
    connection.open()
    captured = bytearray()
    log_stream = None
    if args.log:
        args.log.parent.mkdir(parents=True, exist_ok=True)
        log_stream = args.log.open('w')

    def capture(chunk):
        captured.extend(chunk)
        if log_stream:
            # Keep long OTA captures readable while the monitor is running.
            # The complete file is normalized once the capture finishes.
            log_stream.write(chunk.decode(errors='replace'))
            log_stream.flush()
    if args.operation == 'command':
        if not args.no_read:
            connection.reset_input_buffer()
        connection.write(('\x03' + ' '.join(args.words) + '\r\n').encode())
        connection.flush()
        seconds = 0.0 if args.no_read else 2.0
    else:
        connection.reset_input_buffer()
        seconds = args.seconds
    deadline = time.monotonic() + seconds
    try:
        while time.monotonic() < deadline:
            try:
                capture(connection.read(max(1, min(connection.in_waiting, 4096))))
                if args.operation == 'monitor' and args.until and \
                        args.until.encode() in captured:
                    time.sleep(0.1)
                    capture(connection.read(connection.in_waiting or 1))
                    break
            except (serial.SerialException, OSError):
                connection.close()
                time.sleep(0.2)
                try:
                    connection.open()
                except (serial.SerialException, OSError):
                    time.sleep(0.2)
    finally:
        connection.close()
        if log_stream:
            log_stream.close()
    clean = re.sub(r'\x1b\[[0-9;?]*[A-Za-z]', '', captured.decode(errors='replace'))
    if args.log:
        args.log.parent.mkdir(parents=True, exist_ok=True)
        args.log.write_text(clean)
    print(clean)


if __name__ == '__main__':
    main()
