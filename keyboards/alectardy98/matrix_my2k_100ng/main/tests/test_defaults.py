#!/usr/bin/env python3
"""Verify both factory captures and the GET-only capture boundary offline."""
import importlib.util
import json
from pathlib import Path
import unittest

MAIN = Path(__file__).resolve().parents[1]


def module(name, file):
    spec = importlib.util.spec_from_file_location(name, MAIN.parent / "tools" / file)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


class FactoryImages(unittest.TestCase):
    def test_both_captures_match_complete_images(self):
        gen = module("generator", "generate-defaults.py")
        layout = json.loads((MAIN / "defaults/layout.json").read_text())
        for folder in (MAIN / "defaults", MAIN / "keymaps/vial_alec/defaults"):
            capture = json.loads((folder / "capture.json").read_text())
            image, keymap = gen.build_image(capture, layout)
            self.assertEqual(image, (folder / "factory.bin").read_bytes())
            self.assertEqual(keymap, image[633:1657])
            self.assertEqual(image, gen.build_image(dict(capture, first=capture["second"]), layout)[0])

    def test_capture_allows_no_setter_reset_input_or_file_write(self):
        capture = module("capture", "capture-defaults.py")
        for q in ([11], [3, 2], [5], [15], [19], [253, 76, 1],
                  [253, 232, 4], [253, 233, 2], [253, 235, 2],
                  [253, 231, 2], [253, 230, 6], [253, 238, 3],
                  [254, 11], [254, 13, 2], [254, 13, 4], [254, 13, 6],
                  [253, 237, 1, 1, 1, 9, 1], [253, 237, 1, 1, 1, 11, 60]):
            self.assertFalse(capture.allowed(bytes(q).ljust(32, b"\0")), q)
        recorded = json.loads((MAIN / "keymaps/vial_alec/defaults/capture.json").read_text())
        for row in recorded["transactions"]:
            self.assertTrue(capture.allowed(bytes.fromhex(row["request"])))


if __name__ == "__main__":
    unittest.main()
