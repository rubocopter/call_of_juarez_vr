"""Fail-closed LAA patch checks; the exact binary fixture is optional."""
import importlib.util
import hashlib
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "tools/patch_coj_large_address.py"


class LargeAddressPatchTests(unittest.TestCase):
    def test_helper_exists(self):
        self.assertTrue(HELPER.is_file(), "Exact LAA patch helper is missing")

    def load_helper(self):
        self.assertTrue(HELPER.is_file(), "Exact LAA patch helper is missing")
        spec = importlib.util.spec_from_file_location("coj_laa", HELPER)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_unknown_and_malformed_images_fail_closed(self):
        helper = self.load_helper()
        for image in (b"", b"MZ", bytes(512), b"MZ" + bytes(510)):
            with self.assertRaises(ValueError):
                helper.patch_image(image)

    def test_unknown_cli_input_preserves_source_and_existing_output(self):
        self.load_helper()
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source.exe"
            output = Path(directory) / "output.exe"
            source.write_bytes(b"MZ unknown executable")
            output.write_bytes(b"keep existing output")
            completed = subprocess.run([sys.executable, str(HELPER), str(source), str(output)], capture_output=True)
            self.assertNotEqual(completed.returncode, 0)
            self.assertEqual(source.read_bytes(), b"MZ unknown executable")
            self.assertEqual(output.read_bytes(), b"keep existing output")

    def test_exact_image_changes_only_large_address_header_bit(self):
        helper = self.load_helper()
        if not FIXTURE:
            self.skipTest("Pass a preserved exact CoJ.exe fixture")
        original = FIXTURE.read_bytes()
        patched = helper.patch_image(original)
        self.assertEqual(hashlib.sha256(patched).hexdigest().upper(), helper.LAA_SHA256)
        self.assertEqual(len(original), len(patched))
        self.assertEqual([i for i, pair in enumerate(zip(original, patched)) if pair[0] != pair[1]], [0x10E])
        self.assertEqual(original[0x10E] ^ patched[0x10E], 0x20)
        with self.assertRaises(ValueError):
            helper.patch_image(patched)
        altered = bytearray(original)
        altered[-1] ^= 1
        with self.assertRaises(ValueError):
            helper.patch_image(altered)


FIXTURE = Path(sys.argv.pop(1)) if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else None
if __name__ == "__main__":
    unittest.main()
