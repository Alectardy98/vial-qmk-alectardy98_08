#!/usr/bin/env python3
"""Encode the twice-verified saved GUI configuration as Main factory defaults.

capture.json contains GET responses, not a raw flash dump. Unexposed/reserved
core fields use QMK defaults. Assets and AUX OLED files are not Main EEPROM.
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

BOARD = Path(__file__).resolve().parents[1] / "main"
REPO = next(path for path in BOARD.parents if (path / "quantum").is_dir())


def render_keymap(keymap):
    """Name captured keycodes without changing any electrical matrix entries."""
    header = (REPO / "quantum/keycodes.h").read_text()
    constants = {name: int(value, 0) for name, value in re.findall(
        r"^\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)\s*,", header, re.MULTILINE)}
    for name, target in re.findall(r"^\s*(\w+)\s*=\s*(\w+)\s*,", header, re.MULTILINE):
        if target in constants:
            constants[name] = constants[target]
    names = {value: name for name, value in constants.items()
             if name.startswith("KC_") and value <= 255}
    # Prefer familiar QMK spellings that keep all sixteen columns readable.
    for name in ("KC_ENT", "KC_ESC", "KC_BSPC", "KC_TAB", "KC_SPC", "KC_MINS", "KC_EQL",
                 "KC_LBRC", "KC_RBRC", "KC_BSLS", "KC_SCLN", "KC_QUOT", "KC_GRV",
                 "KC_COMM", "KC_DOT", "KC_SLSH", "KC_CAPS", "KC_PSCR", "KC_INS",
                 "KC_DEL", "KC_PGUP", "KC_PGDN", "KC_RGHT", "KC_NUM", "KC_PSLS",
                 "KC_PAST", "KC_PMNS", "KC_PPLS", "KC_PENT", "KC_PDOT", "KC_NUBS",
                 "KC_LCTL", "KC_LSFT", "KC_LALT", "KC_LGUI", "KC_RCTL", "KC_RSFT",
                 "KC_RALT", "KC_RGUI", *[f"KC_P{i}" for i in range(10)]):
        names[constants[name]] = name
    names.update({0: "KC_NO", 1: "_______"})

    # Resolve the board enum against QMK's actual base, preserving its stored ABI.
    declarations = re.search(r"enum my2k_keycodes\s*\{(.*?)\};",
                             (BOARD / "src/main_keycodes.h").read_text(), re.DOTALL).group(1)
    custom = {}
    value = 0
    for declaration in declarations.split(","):
        declaration = declaration.strip()
        if not declaration:
            continue
        name, separator, initial = declaration.partition("=")
        if separator:
            value = constants[initial.strip()]
        custom[name.strip()] = value
        value += 1
    labels = {
        "MY2K_STORAGE_TOGGLE": ("STORAGE", "Enter or leave USB storage mode"),
        "MY2K_ANIMATION_NEXT": ("GIF_NEXT", "Play the next saved GIF"),
        "MY2K_ANIMATION_PREVIOUS": ("GIF_PREV", "Play the previous saved GIF"),
        **{f"MY2K_ANIMATION_{i:02d}": (f"GIF_{i:02d}", f"Play GIF slot {i}") for i in range(1, 11)},
        "MY2K_NES_TOGGLE": ("NES_MODE", "Enter or leave NES mode"),
        "MY2K_DOOM_TOGGLE": ("DOOM_MODE", "Enter or leave Doom mode"),
        "MY2K_NORMAL_MODE": ("OLED_DFLT", "Return to the normal animation display"),
        "MY2K_GBC_TOGGLE": ("GBC_MODE", "Enter or leave Game Boy Color mode"),
        "MY2K_OLED_MODE_TOGGLE": ("OLED_MODE", "Cycle normal display, NES, GBC and Doom"),
    }
    # Preserve the shared enum's order, including custom keys not used by this
    # captured keymap. A sequential QK_KB_0 enum must match every stored value.
    assert set(custom) == set(labels), "Add a readable label for each board custom keycode"
    for index, value in enumerate(custom.values()):
        assert value == constants["QK_KB_0"] + index, "Custom keycode enum is no longer sequential"
    codes = struct.unpack(">512H", keymap)
    names.update({value: labels[name][0] for name, value in custom.items()})
    names[constants["QK_BOOTLOADER"]] = "QK_BOOT"
    layers = ("_BASE", "_FN", "_LAYER2", "_LAYER3")
    for layer, name in enumerate(layers):
        names[constants["QK_MOMENTARY"] + layer] = f"MO({name})"
    expressions = [names.get(code, f"0x{code:04X}") for code in codes]
    width = max(map(len, expressions))
    text = ["""/* Copyright 2026 Alectardy98
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "main_keycodes.h"

/* Generated from the verified defaults/capture.json; regenerate with
 * tools/generate-defaults.py from this board directory.
 */
enum layer_names {  // Define layers
    _BASE,
    _FN,
    _LAYER2,
    _LAYER3
};

/* Keep this order aligned with main_keycodes.h: Vial stores these values. */
enum custom_keycodes {"""]
    for index, name in enumerate(custom):
        alias, description = labels[name]
        declaration = alias + (" = QK_KB_0," if index == 0 else ",")
        text.append(f"    {declaration:<23} // {description}")
    text += ["};", "", "/* _______ = transparent; KC_NO = no key action.",
             " * Keep the full 8x16 matrix: LAYOUT_capture omits some saved entries.",
             " * Rows 6 and 7 contain the additional numpad keys.", " */",
             "// clang-format off", "const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {"]
    for layer, name in enumerate(layers):
        text.append(f"    [{name}] = {{")
        for row in range(8):
            first = layer * 128 + row * 16
            text.append("        { " + ", ".join(f"{code:>{width}}" for code in expressions[first:first + 16]) + " },")
        text.append("    },")
        if layer != len(layers) - 1:
            text.append("")
    text += ["};", "// clang-format on", ""]
    return "\n".join(text)


def build_image(capture, layout):
    x = capture["first"]
    assert capture["two_readbacks_equal"]
    image = bytearray(32768)
    raw = bytes.fromhex
    nv = raw(x["nv"])
    assert nv[:6] == bytes([253, 234, 170, 1, 0, 32])
    assert nv[7] == 0 and int.from_bytes(nv[8:12], "little") == 0, "Capture is not durable/error-free"

    def put(at, value):
        assert 0 <= at <= len(image) and len(value) <= len(image) - at
        image[at:at + len(value)] = value

    def record(prefix):
        data = bytearray(prefix).ljust(12, b"\0")
        return data + struct.pack("<I", zlib.crc32(data))

    put(0, struct.pack("<H", 0xFEE3))
    image[3] = raw(x["diag"])[30]  # Actual default-layer bitmask.
    # GET magic-setting21 exposes all GUI-editable remap/NKRO flags. Remaining
    # private core bits use the standard enabled one-shot default.
    flags = int.from_bytes(raw(x["qmk"]["21"])[1:5], "little")
    put(4, struct.pack("<H", (flags & 0x3ff) | (1 << 10)))
    put(15, struct.pack("<I", 1))  # EECONFIG_KB_DATA_VERSION.
    put(19, struct.pack("<I", 0x3942524D | (raw(x["performance"])[4] << 31)))

    rgb = bytearray(592)
    rgb[:4] = b"MRG3"
    debounce = raw(x["rgb_scalars"]["9"])[3] | raw(x["rgb_scalars"]["11"])[3] << 4
    aux_mode = raw(x["rgb_scalars"]["58"])[3]
    for i, group in enumerate(x["rgb_groups"]):
        params = {int(k): raw(v)[4:] for k, v in group["params"].items()}
        rgb[4 + i] = params[4][0]
        mode = raw(group["info"])[6]
        if i:
            mode |= params[3][0] << 6 | params[5][0] << 7 | ((debounce >> i) & 1) << 5
        else:
            mode = (mode & 0x2f) | (debounce & 1) << 4 | aux_mode << 6
        rgb[16 + i * 5:21 + i * 5] = bytes([mode, params[2][0], *params[1][:3]])
    #125..127 have no public per-pixel editor; preserve the renderer defaults.
    for i in range(128):
        rgb[56 + i * 4:60 + i * 4] = raw(x["rgb_leds"][i])[4:8] if i < 125 else bytes([0, 255, 255, 0x81])
    for i, value in enumerate(x["rgb_indicators"]):
        rgb[568 + i * 4:572 + i * 4] = raw(value)[4:8]
    rgb[12:16] = struct.pack("<I", zlib.crc32(rgb[:12] + rgb[16:]))
    put(layout["EECONFIG_BASE_SIZE"], rgb)
    put(layout["EECONFIG_SIZE"], bytes([1, 0x59, 0x4d]))  # Stable storage ABI1.
    image[632] = raw(x["layout"])[2]  # VIA layout-options size is one byte.
    keymap = b"".join(raw(value)[4:4 + raw(value)[3]] for value in x["keymap"])
    assert len(keymap) == 1024
    put(layout["VIA_EEPROM_CONFIG_END"], keymap)
    #qmk_settings_t is compiler-verified40 bytes in this fork.
    qs = bytearray(40)
    fields = {1:(26,1), 2:(22,2), 3:(27,1), 4:(0,2), 5:(28,1), 6:(2,2), 7:(24,2),
              9:(4,2), 10:(6,2), 11:(8,2), 12:(10,2), 13:(12,2), 14:(14,2),
              15:(16,2), 16:(18,2), 17:(20,2), 18:(30,2), 19:(32,2),
              20:(34,1), 25:(36,2), 27:(38,2)}
    for qsid, (offset, size) in fields.items():
        qs[offset:offset + size] = raw(x["qmk"][str(qsid)])[1:1 + size]
    for qsid, bit in [(22,0),(23,1),(24,2),(26,3)]:
        qs[29] |= (raw(x["qmk"][str(qsid)])[1] & 1) << bit
    put(layout["VIAL_QMK_SETTINGS_EEPROM_ADDR"], qs)
    for op, key, size in [(1,"VIAL_TAP_DANCE_EEPROM_ADDR",10),(3,"VIAL_COMBO_EEPROM_ADDR",10),
                          (5,"VIAL_KEY_OVERRIDE_EEPROM_ADDR",10),(7,"VIAL_ALT_REPEAT_KEY_EEPROM_ADDR",6)]:
        put(layout[key], b"".join(raw(value)[1:1 + size] for value in x["dynamic"][str(op)]))
    macros = b"".join(raw(value)[4:4 + raw(value)[3]] for value in x["macros"])
    assert len(macros) == layout["DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE"]
    put(layout["DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR"], macros)
    put(32512, raw(x["lock"]))
    gb = b"GBM1" + bytes(raw(x["games"]["3"])[8 + 2 * i] for i in range(8))
    put(32640, gb + struct.pack("<I", zlib.crc32(gb)))
    put(32656, raw(x["touch"])[8:24])
    put(32672, record([1, x["slot"], 0, 0, *b"ANI1"]))
    ani = raw(x["animation"])
    put(32688, record([2, ani[14], ani[18], 0, *b"FPS1"]))
    put(32704, raw(x["sleep"])[8:24])
    games = b"GMT1\x01\x02\x08\x00" + raw(x["games"]["1"])[8:24] + raw(x["games"]["2"])[8:24] + bytes(4)
    put(32720, games + struct.pack("<I", zlib.crc32(games)))
    return bytes(image), keymap


def main():
    capture = json.loads((BOARD / "defaults/capture.json").read_text())
    layout = json.loads((BOARD / "defaults/layout.json").read_text())
    image, keymap = build_image(capture, layout)
    (BOARD / "defaults/factory.bin").write_bytes(image)
    lines = ["/* Generated from defaults/capture.json; regenerate with keyboards/alectardy98/matrix_my2k_100ng/tools/generate-defaults.py. */",
             "const uint8_t my2k_factory_image[MY2K_NV_BYTES] = {"]
    lines += [f" [{i}] = 0x{value:02x}," for i, value in enumerate(image) if value]
    lines += ["};", ""]
    (BOARD / "defaults/factory.inc").write_text("\n".join(lines))
    (BOARD / "keymaps/vial/keymap.c").write_text(render_keymap(keymap))
    (BOARD / "defaults/factory.sha256").write_text(hashlib.sha256(image).hexdigest() + "  factory.bin\n")
    print("Encoded saved settings:", len(image), "logical bytes; no additional RAM mirror")


if __name__ == "__main__":
    main()
