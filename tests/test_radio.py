"""Compile the real radio policy without Zephyr or physical hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class RadioPolicyTests(unittest.TestCase):
    def test_retry_deadlines_interleaving_and_duplicate_history(self):
        compiler = shutil.which('cc')
        self.assertIsNotNone(compiler)
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / 'radio-tests')
            sources = ['firmware/src/protocol/wire.c',
                       'firmware/src/radio/exchange.c',
                       'firmware/src/radio/history.c', 'tests/radio_vectors.c']
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT / 'firmware/include'),
                            *(str(ROOT / name) for name in sources), '-o', binary], check=True)
            subprocess.run([binary], check=True)
