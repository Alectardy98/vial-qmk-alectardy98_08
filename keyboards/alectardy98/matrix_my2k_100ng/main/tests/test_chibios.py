#!/usr/bin/env python3
"""Private ChibiOS generation, cache repair and fail-closed input checks."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = next(p for p in Path(__file__).resolve().parents if (p / "quantum").is_dir())
BOARD = REPO / "keyboards/alectardy98/matrix_my2k_100ng"
SCRIPT = BOARD / "tools/prepare-chibios.py"
SPEC = importlib.util.spec_from_file_location("my2k_chibios", SCRIPT)
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)


class PrivateChibiOS(unittest.TestCase):
    def status(self):
        return {name: subprocess.check_output(["git", "status", "--porcelain"],
                    cwd=REPO / "lib" / name) for name in ["chibios", "chibios-contrib"]}

    def test_reconstruct_cache_repair_and_leave_submodules_clean(self):
        before = self.status()
        self.assertTrue(all(not value for value in before.values()))
        with tempfile.TemporaryDirectory(prefix="overlay-tests-", dir=REPO / ".build") as directory:
            output = Path(directory) / "private"
            def run():
                subprocess.run([sys.executable, str(SCRIPT), "--output", str(output)], check=True)
            run()
            record = json.loads((output / "manifest.json").read_text())
            for name, digest in record["patched_files"].items():
                path = output / name
                self.assertFalse(path.is_symlink())
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest)
            timestamp = (output / "manifest.json").stat().st_mtime_ns
            run()
            self.assertEqual(timestamp, (output / "manifest.json").stat().st_mtime_ns)
            header = output / "chibios-contrib/os/hal/include/usbh/dev/msd.h"
            expected = header.read_bytes()
            header.write_bytes(b"damaged generated cache")
            run()
            self.assertEqual(header.read_bytes(), expected)
        self.assertEqual(before, self.status())

    def test_unknown_or_already_patched_upstream_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            path = base / "source.c"
            entry = {"upstream_sha256": hashlib.sha256(b"pristine").hexdigest(),
                     "patched_sha256": hashlib.sha256(b"private change").hexdigest()}
            path.write_bytes(b"pristine")
            self.assertEqual(PREPARE.checked_input(base, path.name, entry), b"pristine")
            for value in [b"private change", b"unknown update"]:
                path.write_bytes(value)
                with self.assertRaises(ValueError):
                    PREPARE.checked_input(base, path.name, entry)
                self.assertEqual(path.read_bytes(), value)

    def test_modified_board_patch_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="overlay-tests-", dir=REPO / ".build") as directory:
            base = Path(directory)
            copies = base / "patches"
            shutil.copytree(BOARD / "main/chibios", copies)
            patch = copies / "chibios.patch"
            patch.write_bytes(patch.read_bytes() + b"\nUnexpected edit\n")
            original = PREPARE.PATCHES
            try:
                PREPARE.PATCHES = copies
                with self.assertRaisesRegex(ValueError, "patch digest mismatch"):
                    PREPARE.prepare(base / "output")
                self.assertFalse((base / "output").exists())
            finally:
                PREPARE.PATCHES = original


if __name__ == "__main__":
    unittest.main()
