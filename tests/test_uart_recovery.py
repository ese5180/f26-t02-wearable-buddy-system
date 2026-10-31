"""A missing reset window must not start a slow image-hash request."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock, Mock, patch

spec = importlib.util.spec_from_file_location('uart_recovery',
    Path(__file__).resolve().parents[1] / 'tools/bg_uart_recovery.py')
recovery = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recovery)


class RecoveryEntryTests(unittest.IsolatedAsyncioTestCase):
    async def test_absent_bootloader_retries_without_slow_image_query(self):
        transport = Mock(connect=AsyncMock(), max_unencoded_size=341)
        client = Mock(request=AsyncMock(side_effect=TimeoutError('not yet reset')),
                      disconnect=AsyncMock())
        with patch.object(recovery, 'SMPSerialTransport', return_value=transport), \
             patch.object(recovery, 'SMPClient', return_value=client):
            with self.assertRaises(TimeoutError):
                await recovery.connect(SimpleNamespace(wait=0.01, port='/dev/test'))
        self.assertEqual(client.request.await_count, 1)
        request = client.request.await_args.args[0]
        self.assertIsInstance(request, recovery.os_management.MCUMgrParametersRead)
        client.disconnect.assert_awaited_once()

    async def test_unsupported_params_reply_still_establishes_recovery(self):
        transport = Mock(connect=AsyncMock(), max_unencoded_size=341)
        unsupported = SimpleNamespace(rc=8)
        images = SimpleNamespace(rc=0, model_dump=lambda **kw: {'images': []})
        client = Mock(request=AsyncMock(side_effect=[unsupported, images]),
                      disconnect=AsyncMock())
        with patch.object(recovery, 'SMPSerialTransport', return_value=transport), \
             patch.object(recovery, 'SMPClient', return_value=client):
            result = await recovery.connect(SimpleNamespace(wait=1, port='/dev/test'))
        self.assertIs(result, client)
        self.assertIsInstance(client.request.await_args_list[1].args[0],
                              recovery.image_management.ImageStatesRead)
        self.assertEqual(client.request.await_args_list[1].kwargs['timeout_s'], 15)
