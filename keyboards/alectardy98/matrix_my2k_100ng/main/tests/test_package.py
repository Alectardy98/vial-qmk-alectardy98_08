#!/usr/bin/env python3
"""Exercise real ELF packaging output switches and internal-file protection."""
import hashlib
import itertools
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

REPO = next(p for p in Path(__file__).resolve().parents if (p / "quantum").is_dir())
SCRIPT = REPO / "keyboards/alectardy98/matrix_my2k_100ng/tools/package.py"


class PackageOutputs(unittest.TestCase):
    def invoke(self, mcu, prefix, options=(), internal_uf2=None, input_bin=None):
        stem = "alectardy98_matrix_my2k_100ng_" + ("main_vial" if mcu == "main" else "aux_aux")
        raw = input_bin or REPO / ".build" / (stem + ".bin")
        args = [sys.executable, str(SCRIPT), "--mcu", mcu, "--input", str(raw),
                "--elf", str(REPO / ".build" / (stem + ".elf")), "--output", str(prefix)]
        for name, enabled in zip(["bin", "json", "sha256"], options):
            args.extend(["--generate-" + name, "yes" if enabled else "no"])
        if internal_uf2:
            args.extend(["--uf2-output", str(internal_uf2)])
        return subprocess.run(args, capture_output=True, text=True)

    def test_output_combinations_and_stale_cleanup(self):
        for mcu in ["main", "aux"]:
            stem = "alectardy98_matrix_my2k_100ng_" + ("main_vial" if mcu == "main" else "aux_aux")
            expected = (REPO / (stem + ".uf2")).read_bytes()
            raw = REPO / ".build" / (stem + ".bin")
            original = raw.read_bytes()
            for options in itertools.product([False, True], repeat=3):
                with self.subTest(mcu=mcu, options=options), tempfile.TemporaryDirectory() as directory:
                    base = Path(directory)
                    prefix = base / "firmware"
                    keep = base / "other.bin"
                    keep.write_bytes(b"unrelated file")
                    for suffix in [".bin", ".json", ".sha256"]:
                        prefix.with_suffix(suffix).write_bytes(b"stale optional output")
                    result = self.invoke(mcu, prefix, options)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(prefix.with_suffix(".uf2").read_bytes(), expected)
                    for suffix, enabled in zip([".bin", ".json", ".sha256"], options):
                        self.assertEqual(prefix.with_suffix(suffix).exists(), enabled)
                    if options[0]:
                        capacity = 262144 if mcu == "main" else 524288
                        self.assertEqual(prefix.with_suffix(".bin").read_bytes(), original.ljust(capacity, b"\xff"))
                    if options[1]:
                        metadata = json.loads(prefix.with_suffix(".json").read_text())
                        self.assertEqual(metadata["mcu"], mcu)
                        self.assertEqual(len(metadata["hashes"]), 2 if options[0] else 1)
                    if options[2]:
                        lines = prefix.with_suffix(".sha256").read_text().splitlines()
                        self.assertEqual(len(lines), 2 if options[0] else 1)
                        for line in lines:
                            digest, name = line.split("  ", 1)
                            self.assertEqual(digest, hashlib.sha256(Path(name).read_bytes()).hexdigest())
                    self.assertEqual(keep.read_bytes(), b"unrelated file")
                    self.assertEqual(raw.read_bytes(), original)

    def test_default_uf2_only_and_qmk_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            prefix = base / "main"
            internal = base / "qmk.uf2"
            result = self.invoke("main", prefix, internal_uf2=internal)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(internal.read_bytes(), (REPO / "alectardy98_matrix_my2k_100ng_main_vial.uf2").read_bytes())
            self.assertEqual({p.name for p in base.iterdir()}, {"qmk.uf2"})

    def test_optional_bin_cannot_overwrite_internal_input(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "input"
            source = prefix.with_suffix(".bin")
            source.write_bytes((REPO / ".build/alectardy98_matrix_my2k_100ng_main_vial.bin").read_bytes())
            before = source.read_bytes()
            result = self.invoke("main", prefix, [True, False, False], input_bin=source)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(source.read_bytes(), before)
            self.assertFalse(prefix.with_suffix(".uf2").exists())
            # Disabled BIN output must not delete the input, either.
            result = self.invoke("main", prefix, input_bin=source)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(source.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
