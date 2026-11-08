#!/usr/bin/env python3
"""Forward real Memfault USB-shell chunks to the cloud, with a durable local queue.

Only one process may own this serial port. No network credentials enter firmware.
Export consumes device-side data; each received chunk is committed before upload.
"""
import argparse
import base64
import fcntl
import hashlib
import os
from pathlib import Path
import re
import sqlite3
import threading
import time
from urllib import error, parse, request
import uuid

import serial

CHUNKS_URL = 'https://chunks.memfault.com/api/v0/chunks/'
ANSI = re.compile(rb'\x1b\[[0-9;?]*[A-Za-z]')
CHUNK = re.compile(rb'MC:([A-Za-z0-9+/=]+):')


class Queue:
    def __init__(self, path, device, project_key=None):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(path, timeout=10)
        os.chmod(path, 0o600)
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.execute('PRAGMA synchronous=FULL')
        self.db.execute('CREATE TABLE IF NOT EXISTS metadata (name TEXT PRIMARY KEY, value TEXT)')
        self.db.execute('CREATE TABLE IF NOT EXISTS chunks (id INTEGER PRIMARY KEY, data BLOB NOT NULL)')
        self._bind('device', device)
        if project_key:
            self._bind('project', hashlib.sha256(project_key.encode()).hexdigest())
        self.db.commit()

    def _bind(self, name, value):
        row = self.db.execute('SELECT value FROM metadata WHERE name=?', (name,)).fetchone()
        if row and row[0] != value:
            raise ValueError(f'Queue belongs to a different {name}; use a separate --queue path')
        self.db.execute('INSERT OR IGNORE INTO metadata VALUES (?, ?)', (name, value))

    def append(self, data):
        self.db.execute('INSERT INTO chunks(data) VALUES (?)', (data,))
        self.db.commit()

    def batch(self, limit=32):
        return self.db.execute('SELECT id, data FROM chunks ORDER BY id LIMIT ?', (limit,)).fetchall()

    def acknowledge(self, batch):
        self.db.executemany('DELETE FROM chunks WHERE id=?', [(row[0],) for row in batch])
        self.db.commit()

    def count(self):
        return self.db.execute('SELECT COUNT(*) FROM chunks').fetchone()[0]

    def close(self):
        self.db.close()


class Decoder:
    """Handle markers split across serial reads and interleaved ordinary logs."""
    def __init__(self):
        self.pending = b''

    def feed(self, data):
        self.pending += data
        clean = ANSI.sub(b'', self.pending)
        chunks = []
        end = 0
        for match in CHUNK.finditer(clean):
            chunks.append(base64.b64decode(match[1], validate=True))
            end = match.end()
        self.pending = clean[end:]
        # Retain incomplete markers, but bound unrelated application logging.
        if len(self.pending) > 65536:
            marker = self.pending.rfind(b'MC:')
            if marker >= 0 and len(self.pending) - marker > 65536:
                raise ValueError('Oversized/incomplete Memfault chunk; stop and preserve serial data')
            self.pending = self.pending[marker:] if marker >= 0 else self.pending[-2:]
        return chunks


class NoRedirect(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise error.HTTPError(req.full_url, code, 'Unexpected redirect refused', headers, fp)


def upload_batch(device, key, batch, opener=None):
    boundary = 'buddyguard-' + uuid.uuid4().hex
    parts = []
    for _, payload in batch:
        parts.append((f'--{boundary}\r\nContent-Type: application/octet-stream\r\n'
                      f'Content-Length: {len(payload)}\r\n\r\n').encode() + payload + b'\r\n')
    body = b''.join(parts) + f'--{boundary}--\r\n'.encode()
    req = request.Request(CHUNKS_URL + parse.quote(device, safe=''), data=body,
                          headers={'Memfault-Project-Key': key,
                                   'Content-Type': f'multipart/mixed; boundary={boundary}',
                                   'Content-Length': str(len(body))}, method='POST')
    opener = opener or request.build_opener(NoRedirect())
    with opener.open(req, timeout=15) as response:
        if response.status != 202:
            raise RuntimeError(f'Unexpected upload status {response.status}')


def sender(path, device, key, stop):
    queue = Queue(path, device, key)
    delay = 1
    try:
        while not stop.is_set():
            batch = queue.batch()
            if not batch:
                stop.wait(0.25)
                continue
            try:
                upload_batch(device, key, batch)
            except error.HTTPError as exc:
                # Never print request headers, the key, or the response body.
                print(f'Upload HTTP {exc.code}; {queue.count()} chunks retained locally', flush=True)
                retry = exc.headers.get('Retry-After', '') if exc.headers else ''
                delay = min(60, max(delay * 2, int(retry) if retry.isdigit() else 1))
                stop.wait(delay)
            except (error.URLError, OSError, RuntimeError):
                print(f'Upload unavailable; {queue.count()} chunks retained locally', flush=True)
                delay = min(60, delay * 2)
                stop.wait(delay)
            else:
                queue.acknowledge(batch)
                print(f'Cloud accepted {len(batch)} chunks; pending={queue.count()}', flush=True)
                delay = 1
    finally:
        queue.close()


def verify_device(connection, expected):
    connection.write(b'\x03mflt get_device_info\r\n')
    connection.flush()
    captured = b''
    deadline = time.monotonic() + 6
    while time.monotonic() < deadline:
        captured += connection.read(max(1, min(connection.in_waiting, 4096)))
        match = re.search(rb'S/N:\s*([A-Za-z0-9_-]+)', ANSI.sub(b'', captured))
        if match and ANSI.sub(b'', captured).rstrip().endswith(b'uart:~$'):
            actual = match[1].decode()
            if actual != expected:
                raise ValueError(f'Attached device is {actual}, expected {expected}; no export started')
            return
    raise ValueError('No Memfault device identity received; check firmware, port, and other serial readers')


def first_heartbeat(connection):
    # Capture real current readings immediately; later SDK heartbeats run every 60 s.
    connection.write(b'mflt test heartbeat\r\n')
    connection.flush()
    captured = b''
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
        captured += connection.read(max(1, min(connection.in_waiting, 4096)))
        if ANSI.sub(b'', captured).rstrip().endswith(b'uart:~$'):
            return
    raise ValueError('Heartbeat command did not complete; no export started')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--device', required=True, help='BG-T01/BG-T02/BG-T03/BG-C01; checked against firmware')
    parser.add_argument('--key-file', type=Path, help='Project Key file; otherwise MEMFAULT_PROJECT_KEY')
    parser.add_argument('--queue', type=Path, help='Persistent per-device queue outside Git')
    parser.add_argument('--seconds', type=float, default=120)
    parser.add_argument('--interval', type=float, default=15)
    parser.add_argument('--offline', action='store_true', help='Capture into queue without any cloud requests')
    args = parser.parse_args()
    if not re.fullmatch(r'BG-(?:T0[1-3]|C01)', args.device):
        parser.error('Use a configured BuddyGuard device serial')
    if args.seconds <= 0 or args.interval < 5:
        parser.error('--seconds must be positive and --interval at least 5')
    key = args.key_file.read_text().strip() if args.key_file else os.environ.get('MEMFAULT_PROJECT_KEY', '').strip()
    if not args.offline and not key:
        parser.error('Set MEMFAULT_PROJECT_KEY / --key-file, or use --offline')
    if key and not re.fullmatch(r'[A-Za-z0-9_-]+', key):
        parser.error('Project Key contains unexpected characters')
    path = args.queue or Path.home() / '.local/state/buddyguard/fleet' / f'{args.device}.sqlite3'
    path.parent.mkdir(parents=True, exist_ok=True)
    lock = path.with_suffix('.lock').open('a')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        parser.error('Another gateway owns this queue; stop it first')
    queue = Queue(path, args.device, key if not args.offline else None)
    stop = threading.Event()
    worker = None
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    connection.port, connection.dtr, connection.rts = args.port, True, False
    connection.exclusive = True
    try:
        connection.open()
        verify_device(connection, args.device)
        first_heartbeat(connection)
        print(f'Identity verified: {args.device}; queue={path}', flush=True)
        if not args.offline:
            worker = threading.Thread(target=sender, args=(path, args.device, key, stop), daemon=True)
            worker.start()
        decoder = Decoder()
        deadline = time.monotonic() + args.seconds
        next_export = 0
        exporting = False
        output_tail = b''
        while time.monotonic() < deadline:
            if not exporting and time.monotonic() >= next_export:
                connection.write(b'mflt export\r\n')
                connection.flush()
                exporting = True
                output_tail = b''
            data = connection.read(max(1, min(connection.in_waiting, 4096)))
            for chunk in decoder.feed(data):
                queue.append(chunk)
                print(f'Captured {len(chunk)} bytes; pending={queue.count()}', flush=True)
            output_tail = (output_tail + ANSI.sub(b'', data))[-256:]
            # The last command's shell prompt marks the end of the export.
            if exporting and output_tail.rstrip().endswith(b'uart:~$'):
                exporting = False
                next_export = time.monotonic() + args.interval
    except KeyboardInterrupt:
        pass
    finally:
        connection.close()
        stop.set()
        if worker:
            worker.join(timeout=16)
        print(f'Pending chunks retained: {queue.count()} ({path})', flush=True)
        queue.close()
        lock.close()


if __name__ == '__main__':
    main()
