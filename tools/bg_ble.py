#!/usr/bin/env python3
"""Scan BuddyGuard laboratory presence frames and use the standard SMP client.
电脑端 BLE 发现与 SMP 更新客户端；镜像由 MCUboot 验签、应用自行确认。
"""
import argparse
import asyncio
import json
from pathlib import Path
import time

from bleak import BleakScanner
from smpclient import SMPClient
from smpclient.transport.ble import SMPBLETransport
from smpclient.transport import SMPTransportDisconnected
from smpclient.requests import image_management, os_management

COMPANY_ID = 0xFFFF


class UploadInterrupted(Exception):
    """Transport dropped before activation; retrying the same SHA is safe."""


def decode_presence(data):
    """Bleak removes the 2-byte company ID from manufacturer_data values."""
    if len(data) != 6 or data[0] != 1 or data[5] != 1 or data[1] == 0:
        raise ValueError('Not a supported BuddyGuard tag presence frame')
    if data[4] & ~0x03:
        raise ValueError('Unknown status flags')
    return {'protocol': data[0], 'tag': data[1],
            'sequence': int.from_bytes(data[2:4], 'little'),
            'maintenance': bool(data[4] & 1), 'unconfirmed': bool(data[4] & 2)}


def show_response(response):
    def convert(value):
        if isinstance(value, bytes):
            return value.hex()
        return str(value)
    print(json.dumps(response.model_dump(exclude={'header', 'smp_data'}),
                     default=convert), flush=True)
    # SMP error responses carry rc or err; success responses do not.
    if getattr(response, 'rc', 0) or getattr(response, 'err', None):
        raise RuntimeError('SMP operation returned an error')


def confirmed_primary_for_cleanup(images):
    """Refuse candidate cleanup until there is one confirmed primary and no pending swap."""
    if any(item.image not in (None, 0) for item in images):
        raise RuntimeError('Cleanup supports only this single-image board layout')
    active = [item for item in images if item.active]
    if len(active) != 1 or active[0].slot != 0 or not active[0].confirmed:
        raise RuntimeError('Cleanup requires exactly one active, confirmed primary')
    if any(item.pending for item in images):
        raise RuntimeError('Refusing cleanup while an image is pending activation')
    return active[0]


async def scan(args):
    last = {}

    def received(device, advertisement):
        name = advertisement.local_name or device.name or ''
        raw = advertisement.manufacturer_data.get(COMPANY_ID)
        if raw is None or (name and not name.startswith('BG-')):
            return
        try:
            frame = decode_presence(raw)
        except ValueError:
            return
        key = (device.address, frame['sequence'], raw)
        if key in last:
            return
        last[key] = time.monotonic()
        print(json.dumps({'event': 'presence', 'address': device.address,
                          'name': name, 'rssi': advertisement.rssi, **frame}), flush=True)

    async with BleakScanner(detection_callback=received):
        await asyncio.sleep(args.seconds)
    print(json.dumps({'event': 'scan_complete', 'unique_frames': len(last)}), flush=True)


async def find_address(args):
    if args.address:
        return args.address
    device = await BleakScanner.find_device_by_filter(
        lambda device, ad: (ad.local_name or device.name) == args.name,
        timeout=15.0)
    if device is None:
        raise RuntimeError('Tag not found; check power and run bg maintenance 300 over USB.')
    return device.address


async def erase_secondary(address):
    """Erase once; reconnect/read back if bulk Flash erase drops the BLE ACK."""
    async with SMPClient(SMPBLETransport(), address, timeout_s=20.0) as client:
        before = await client.request(image_management.ImageStatesRead())
        show_response(before)
        primary = confirmed_primary_for_cleanup(before.images)
        if not any(item.slot == 1 for item in before.images):
            print(json.dumps({'event': 'no_secondary_image_listed'}), flush=True)
            return
        try:
            show_response(await client.request(image_management.ImageErase(slot=1), timeout_s=60))
        except (SMPTransportDisconnected, asyncio.TimeoutError):
            print(json.dumps({'event': 'erase_ack_missing',
                              'note': 'Reconnecting for readback; the erase request is not repeated.'}),
                  flush=True)
    await asyncio.sleep(2)
    async with SMPClient(SMPBLETransport(), address, timeout_s=20.0) as client:
        after = await client.request(image_management.ImageStatesRead())
        show_response(after)
        current = confirmed_primary_for_cleanup(after.images)
        if current.hash != primary.hash or current.version != primary.version or \
                any(item.slot == 1 for item in after.images):
            raise RuntimeError('Secondary cleanup verification failed')
        print(json.dumps({'event': 'secondary_candidate_removed', 'primary_version': current.version}), flush=True)


async def manage(args):
    address = await find_address(args)
    if args.command == 'erase-secondary':
        return await erase_secondary(address)
    transport = SMPBLETransport()
    async with SMPClient(transport, address, timeout_s=20.0) as client:
        transport.initialize(min(args.packet_size, transport.max_unencoded_size))
        if args.command == 'images':
            show_response(await client.request(image_management.ImageStatesRead()))
            return
        if args.command == 'reset':
            show_response(await client.request(os_management.ResetWrite()))
            return
        if args.command == 'test':
            show_response(await client.request(image_management.ImageStatesWrite(
                hash=bytes.fromhex(args.hash), confirm=False)))
            return
        image = args.image.read_bytes()
        started = time.monotonic()
        print(json.dumps({'event': 'upload_start', 'bytes': len(image),
                          'packet_limit': transport.max_unencoded_size}), flush=True)
        progress = -1
        try:
            async for offset in client.upload(image, upgrade=False,
                                             first_timeout_s=60.0,
                                             subsequent_timeout_s=30.0):
                percent = int(offset * 100 / len(image))
                if percent // 5 != progress:
                    progress = percent // 5
                    print(json.dumps({'event': 'upload', 'bytes': offset,
                                      'total': len(image), 'percent': percent}), flush=True)
                if args.stop_at is not None and percent >= args.stop_at:
                    print(json.dumps({'event': 'intentional_upload_interruption',
                                      'bytes': offset}), flush=True)
                    return
        except (SMPTransportDisconnected, asyncio.TimeoutError) as exc:
            raise UploadInterrupted('Upload interrupted before activation') from exc
        except RuntimeError as exc:
            # smpclient 7.3.0 can mask an asyncio receive timeout with this
            # condition-lock error. Recreate the transport and resume by SHA.
            if str(exc) != 'Lock is not acquired.':
                raise
            raise UploadInterrupted('Receive timeout interrupted upload') from exc
        print(json.dumps({'event': 'upload_complete',
                          'elapsed_seconds': round(time.monotonic() - started, 2)}), flush=True)
        response = await client.request(image_management.ImageStatesRead())
        show_response(response)
        if args.command == 'update':
            candidates = [item for item in response.images
                          if item.slot == 1 and not item.active]
            if len(candidates) != 1:
                raise RuntimeError('No unique secondary candidate; refusing to request activation.')
            show_response(await client.request(image_management.ImageStatesWrite(
                hash=candidates[0].hash, confirm=False)))
            show_response(await client.request(os_management.ResetWrite()))
            print(json.dumps({'event': 'test_upgrade_requested',
                              'note': 'Device self-test must confirm; verify serial and image state.'}),
                  flush=True)


async def resilient_manage(args):
    for attempt in range(1, 4):
        try:
            return await manage(args)
        except UploadInterrupted:
            if attempt == 3:
                raise
            print(json.dumps({'event': 'retry_upload', 'next_attempt': attempt + 1,
                              'note': 'Reconnect using the same image SHA; do not confirm.'}),
                  flush=True)
            await asyncio.sleep(2)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--name', default='BG-T01')
    parser.add_argument('--address')
    parser.add_argument('--packet-size', type=int, default=244, choices=range(128, 1025))
    sub = parser.add_subparsers(dest='command', required=True)
    scanning = sub.add_parser('scan')
    scanning.add_argument('--seconds', type=float, default=10.0)
    sub.add_parser('images')
    sub.add_parser('erase-secondary', help='Remove a listed inactive slot1 image after confirmed-primary checks')
    sub.add_parser('reset')
    testing = sub.add_parser('test')
    testing.add_argument('--hash', required=True)
    for name in ['upload', 'update']:
        operation = sub.add_parser(name)
        operation.add_argument('image', type=Path)
        operation.add_argument('--stop-at', type=int, choices=range(1, 100))
    args = parser.parse_args()
    asyncio.run(scan(args) if args.command == 'scan' else resilient_manage(args))


if __name__ == '__main__':
    main()
