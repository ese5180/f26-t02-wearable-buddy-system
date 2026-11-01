"""Prevent cleanup from erasing a trial, pending or ambiguous image layout."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

spec = importlib.util.spec_from_file_location('bg_ble_cleanup',
    Path(__file__).resolve().parents[1] / 'tools/bg_ble.py')
ble = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ble)


def image(slot, active=False, confirmed=False, pending=False, number=None):
    return SimpleNamespace(slot=slot, active=active, confirmed=confirmed,
                           pending=pending, image=number)


class CandidateCleanupTests(unittest.TestCase):
    def test_confirmed_primary_and_inactive_candidate_allowed(self):
        primary = image(0, active=True, confirmed=True)
        self.assertIs(ble.confirmed_primary_for_cleanup([primary, image(1)]), primary)

    def test_unconfirmed_trial_refused(self):
        with self.assertRaisesRegex(RuntimeError, 'confirmed primary'):
            ble.confirmed_primary_for_cleanup([image(0, active=True), image(1)])

    def test_pending_swap_refused(self):
        with self.assertRaisesRegex(RuntimeError, 'pending activation'):
            ble.confirmed_primary_for_cleanup([
                image(0, active=True, confirmed=True), image(1, pending=True)])

    def test_other_active_slot_and_multi_image_refused(self):
        with self.assertRaisesRegex(RuntimeError, 'confirmed primary'):
            ble.confirmed_primary_for_cleanup([image(1, active=True, confirmed=True)])
        with self.assertRaisesRegex(RuntimeError, 'single-image'):
            ble.confirmed_primary_for_cleanup([image(0, active=True, confirmed=True, number=1)])


class CandidateCleanupReconnectTests(unittest.IsolatedAsyncioTestCase):
    async def test_missing_erase_ack_only_reconnects_for_readback(self):
        primary = image(0, active=True, confirmed=True)
        primary.hash = b'known-primary'
        primary.version = '1.1.20'
        before = SimpleNamespace(images=[primary, image(1)])
        after = SimpleNamespace(images=[primary])
        first, second = MagicMock(), MagicMock()
        for client in (first, second):
            client.__aenter__ = AsyncMock(return_value=client)
            client.__aexit__ = AsyncMock(return_value=False)
        first.request = AsyncMock(side_effect=[before, ble.SMPTransportDisconnected('erase ACK lost')])
        second.request = AsyncMock(return_value=after)
        with patch.object(ble, 'SMPClient', side_effect=[first, second]), \
             patch.object(ble, 'SMPBLETransport'), patch.object(ble, 'show_response'), \
             patch.object(ble.asyncio, 'sleep', new=AsyncMock()):
            await ble.erase_secondary('test-device')
        self.assertEqual(first.request.await_count, 2)
        request = first.request.await_args_list[1].args[0]
        self.assertIsInstance(request, ble.image_management.ImageErase)
        self.assertEqual(request.slot, 1)
        second.request.assert_awaited_once()
        self.assertIsInstance(second.request.await_args.args[0], ble.image_management.ImageStatesRead)
