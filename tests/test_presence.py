import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('bg_ble',
    Path(__file__).resolve().parents[1] / 'tools/bg_ble.py')
bg_ble = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bg_ble)


class PresenceTests(unittest.TestCase):
    def test_sequence_endianness_and_status(self):
        result = bg_ble.decode_presence(bytes([1, 7, 0x34, 0x12, 3, 1]))
        self.assertEqual(result['tag'], 7)
        self.assertEqual(result['sequence'], 0x1234)
        self.assertTrue(result['maintenance'])
        self.assertTrue(result['unconfirmed'])

    def test_reject_truncated_or_unrelated_radio_data(self):
        for data in [b'', bytes(5), bytes(7), bytes([2, 1, 0, 0, 0, 1]),
                     bytes([1, 0, 0, 0, 0, 1]), bytes([1, 1, 0, 0, 4, 1]),
                     bytes([1, 1, 0, 0, 0, 2])]:
            with self.subTest(data=data), self.assertRaises(ValueError):
                bg_ble.decode_presence(data)


if __name__ == '__main__':
    unittest.main()
