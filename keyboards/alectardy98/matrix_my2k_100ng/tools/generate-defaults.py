#!/usr/bin/env python3
"""Encode the twice-verified saved GUI configuration as Main factory defaults.

capture.json contains GET responses, not a raw flash dump. Unexposed/reserved
core fields use QMK defaults. Assets and AUX OLED files are not Main EEPROM.
"""
import hashlib
import json
from pathlib import Path
import struct
import zlib

BOARD = Path(__file__).resolve().parents[1] / "main"


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
    text = ["/* Saved four-layer keymap: generated from the verified live GET capture. */", "#include QMK_KEYBOARD_H",
            "const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {"]
    codes = struct.unpack(">512H", keymap)
    for layer in range(4):
        text.append(f" [{layer}] = {{")
        for row in range(8):
            text.append("  {" + ",".join(f"0x{v:04x}" for v in codes[layer * 128 + row * 16:layer * 128 + row * 16 + 16]) + "},")
        text.append(" },")
    text += ["};", ""]
    (BOARD / "keymaps/vial/keymap.c").write_text("\n".join(text))
    (BOARD / "defaults/factory.sha256").write_text(hashlib.sha256(image).hexdigest() + "  factory.bin\n")
    print("Encoded saved settings:", len(image), "logical bytes; no additional RAM mirror")


if __name__ == "__main__":
    main()
