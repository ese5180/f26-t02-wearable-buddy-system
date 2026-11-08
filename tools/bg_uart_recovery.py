#!/usr/bin/env python3
"""MCUboot UART recovery only: image list, direct-primary upload, reset. No test/confirm commands.
MCUboot 串口恢复客户端；直接写 primary，不具备 BLE 候选槽保护。
"""
import argparse
import asyncio
import json
import logging
import os
from pathlib import Path
import subprocess
import time

from smpclient import SMPClient
from smpclient.requests import image_management, os_management
from smpclient.transport.serial import SMPSerialTransport
from smpclient.transport.serial.encoded import BufferParams

ROOT = Path(__file__).resolve().parents[1]


def show(response):
    print(json.dumps(response.model_dump(exclude={'header', 'smp_data'}),
                     default=lambda x: x.hex() if isinstance(x, bytes) else str(x)), flush=True)
    if getattr(response, 'rc', 0) or getattr(response, 'err', None):
        raise RuntimeError('MCUboot returned an SMP error')


def verify_image(args):
    workspace = Path(os.environ.get('BUDDYGUARD_ZEPHYR_WORKSPACE', Path.home() / 'zephyrproject'))
    imgtool = args.imgtool or workspace / '.venv/bin/imgtool'
    result = subprocess.run([str(imgtool), 'verify', '-k', str(args.public_key), str(args.image)],
                            capture_output=True, text=True)
    print(result.stdout + result.stderr, flush=True)
    if args.allow_invalid_test:
        if result.returncode == 0:
            raise RuntimeError('--allow-invalid-test requires an image that fails verification with this root')
        print('NEGATIVE TEST: deliberately overwriting primary with an invalid candidate; retain a valid recovery image.', flush=True)
    elif result.returncode:
        raise RuntimeError('Image verification failed; refusing normal recovery upload')


async def connect(args):
    deadline = time.monotonic() + args.wait
    print('Waiting for MCUboot UART recovery. Reset the target into its 3-second window if needed.', flush=True)
    last_error = None
    while time.monotonic() < deadline:
        # Fixed conservative framing works even when MCUboot lacks the params group.
        transport = SMPSerialTransport(fragmentation_strategy=BufferParams(line_length=128, line_buffers=4),
            baudrate=115200, timeout=0, rtscts=False, dsrdtr=False)
        client = SMPClient(transport, args.port, timeout_s=0.7)
        try:
            # SMPClient.connect() swallows its parameter-query timeout. A slow
            # image-list request immediately afterwards could miss the entire
            # reset entry window. Probe quickly until an actual reply arrives;
            # ENOTSUP is expected for MCUboot's disabled parameters group.
            await transport.connect(args.port, timeout_s=1)
            logger = logging.getLogger('smpclient')
            previous_disabled = logger.disabled
            logger.disabled = True  # Expected while waiting for the target reset.
            try:
                await client.request(os_management.MCUMgrParametersRead(), timeout_s=0.35)
            finally:
                logger.disabled = previous_disabled
            # MCUboot hashes both slots synchronously; this is slower than an
            # ordinary SMP request on the ESP32S3 SPI NOR flash.
            response = await client.request(image_management.ImageStatesRead(), timeout_s=15)
            show(response)
            print(json.dumps({'event': 'recovery_connected', 'port': args.port,
                              'max_unencoded_size': transport.max_unencoded_size}), flush=True)
            return client
        except Exception as error:
            last_error = error
            await client.disconnect()
            await asyncio.sleep(0.15)
    raise TimeoutError(f'MCUboot recovery did not respond: {last_error}')


async def run(args):
    client = await connect(args)
    try:
        if args.command == 'images':
            return
        if args.command == 'reset':
            show(await client.request(os_management.ResetWrite(), timeout_s=2))
            return
        data = args.image.read_bytes()
        print(json.dumps({'event': 'upload_start', 'target': 'PRIMARY', 'bytes': len(data)}), flush=True)
        progress = -1
        async for offset in client.upload(data, slot=0, upgrade=False,
                                          first_timeout_s=60, subsequent_timeout_s=10):
            percent = offset * 100 // len(data)
            if percent // 5 != progress:
                progress = percent // 5
                print(json.dumps({'event': 'upload', 'bytes': offset, 'total': len(data),
                                  'percent': percent}), flush=True)
        print(json.dumps({'event': 'upload_complete', 'note': 'Signature acceptance must be checked at boot.'}), flush=True)
        show(await client.request(image_management.ImageStatesRead(), timeout_s=15))
        if args.reset:
            show(await client.request(os_management.ResetWrite(), timeout_s=3))
            print(json.dumps({'event': 'reset_requested', 'note': 'Check USB boot logs and the running application version.'}), flush=True)
    finally:
        await client.disconnect()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', required=True, help='USB-TTL adapter, NOT the native XIAO USB port')
    p.add_argument('--wait', type=float, default=60)
    sub = p.add_subparsers(dest='command', required=True)
    sub.add_parser('images')
    sub.add_parser('reset')
    upload = sub.add_parser('upload')
    upload.add_argument('image', type=Path)
    upload.add_argument('--reset', action='store_true')
    upload.add_argument('--allow-invalid-test', action='store_true')
    upload.add_argument('--public-key', type=Path, default=ROOT / 'keys/team2-signing-public.pem')
    upload.add_argument('--imgtool', type=Path)
    args = p.parse_args()
    if args.wait <= 0:
        p.error('--wait must be positive')
    if args.command == 'upload':
        verify_image(args)
    logging.getLogger('smpclient').setLevel(logging.ERROR)
    asyncio.run(run(args))


if __name__ == '__main__':
    main()
