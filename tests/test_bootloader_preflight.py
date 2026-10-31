"""Offline checks for the guards protecting a bootloader-only installation."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('install_boot', ROOT / 'scripts/install_recovery_bootloader.py')
install = importlib.util.module_from_spec(spec)
spec.loader.exec_module(install)


class BootloaderPreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / 'build'
        z = self.build / 'mcuboot/zephyr'
        z.mkdir(parents=True)
        self.flash = bytearray(b'\xff' * 0x800000)
        self.flash[:4] = b'\xe9\x04\x02\x3f'
        (self.root / 'flash.bin').write_bytes(self.flash)
        self.info = self.root / 'backup-info.json'
        self.info.write_text(json.dumps({'backup_file':'flash.bin', 'flash_bytes':len(self.flash),
            'sha256':hashlib.sha256(self.flash).hexdigest(), 'device_usb_serial':'TEST'}))
        image = bytes(self.flash[:64])
        (z / 'zephyr.bin').write_bytes(image)
        (z / '.config').write_text('CONFIG_MCUBOOT_SERIAL=y\nCONFIG_BOOT_SERIAL_UART=y\n'
            'CONFIG_BOOT_VALIDATE_SLOT0=y\nCONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256=y\nCONFIG_MULTITHREADING=y\n')
        (self.build / 'signing-public.pem').write_bytes((ROOT / 'keys/team2-signing-public.pem').read_bytes())
        (self.build / 'buddyguard-build.json').write_text(json.dumps({
            'board':'xiao_esp32s3/esp32s3/procpu', 'serial_recovery':True,
            'bootloader_sha256':hashlib.sha256(image).hexdigest()}))

    def test_valid_plan_does_not_modify_backup(self):
        _, original, image = install.preflight(self.build, self.info)
        self.assertEqual(original, self.flash)
        self.assertEqual(len(image), 64)

    def test_corrupt_backup_rejected(self):
        self.flash[-1] = 0
        (self.root / 'flash.bin').write_bytes(self.flash)
        with self.assertRaisesRegex(ValueError, 'backup size/hash'):
            install.preflight(self.build, self.info)

    def test_primary_validation_cannot_be_omitted(self):
        p = self.build / 'mcuboot/zephyr/.config'
        p.write_text(p.read_text().replace('CONFIG_BOOT_VALIDATE_SLOT0=y', ''))
        with self.assertRaisesRegex(ValueError, 'safeguard'):
            install.preflight(self.build, self.info)

    def test_different_signing_root_rejected(self):
        (self.build / 'signing-public.pem').write_text('another root')
        with self.assertRaisesRegex(ValueError, 'signing identity'):
            install.preflight(self.build, self.info)

    def test_oversized_bootloader_rejected(self):
        (self.build / 'mcuboot/zephyr/zephyr.bin').write_bytes(bytes(65537))
        with self.assertRaisesRegex(ValueError, 'oversized'):
            install.preflight(self.build, self.info)

    def test_flash_header_change_rejected(self):
        p = self.build / 'mcuboot/zephyr/zephyr.bin'
        changed = bytearray(p.read_bytes()); changed[3] = 0
        p.write_bytes(changed)
        report = self.build / 'buddyguard-build.json'
        info = json.loads(report.read_text()); info['bootloader_sha256'] = hashlib.sha256(changed).hexdigest()
        report.write_text(json.dumps(info))
        with self.assertRaisesRegex(ValueError, 'header differs'):
            install.preflight(self.build, self.info)
