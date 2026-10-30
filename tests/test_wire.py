"""Compile the firmware's actual protocol implementation with the host C compiler."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class WireTests(unittest.TestCase):
    def test_on_air_vectors_and_rejection(self):
        compiler = shutil.which('cc')
        self.assertIsNotNone(compiler, 'Install the host C compiler (Xcode command-line tools on macOS).')
        with tempfile.TemporaryDirectory() as directory:
            binary = str(Path(directory) / 'wire-tests')
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-I', str(ROOT / 'firmware/include'), str(ROOT / 'firmware/src/protocol/wire.c'),
                str(ROOT / 'tests/wire_vectors.c'), '-o', binary], check=True)
            subprocess.run([binary], check=True)
