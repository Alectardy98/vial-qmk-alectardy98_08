#!/usr/bin/env python3
"""Independent ELF/UF2/default-layout checks; no device access."""
import hashlib
import argparse
import json
import lzma
from pathlib import Path
import struct
import subprocess

BOARD = Path(__file__).resolve().parents[1]
REPO = next(p for p in BOARD.parents if (p / "quantum").is_dir())
NAME = "FAUKWAA - [MY-2K] 100NG Edition - Alectardy98 Custom"


def load_elf(path):
    data = path.read_bytes()
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    assert header[0][:7] == b"\x7fELF\x01\x01\x01" and header[2] == 40
    loads = []
    for i in range(header[10]):
        kind, offset, va, pa, size, memory, flags, alignment = struct.unpack_from("<8I", data, header[5] + i * header[9])
        if kind == 1 and size:
            loads.append((va, pa, data[offset:offset + size]))
    symbols = {}
    for line in subprocess.check_output(["arm-none-eabi-nm", "-S", str(path)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 4:
            symbols[fields[3]] = (int(fields[0], 16), int(fields[1], 16))

    def symbol(name):
        address, size = symbols[name]
        for va, pa, value in loads:
            if va <= address and address + size <= va + len(value):
                return value[address - va:address - va + size]
        raise AssertionError("Missing load bytes: " + name)

    return loads, symbols, symbol


def check(mcu, start, end, family, keymap="vial"):
    prefix = REPO / f"alectardy98_matrix_my2k_100ng_{'main_' + keymap if mcu == 'main' else 'aux_aux'}"
    uf2 = prefix.with_suffix(".uf2").read_bytes()
    blocks = (end - start) // 256
    assert len(uf2) == blocks * 512
    reconstructed = bytearray()
    for i in range(blocks):
        block = uf2[i * 512:(i + 1) * 512]
        assert struct.unpack_from("<8I", block) == (0x0a324655, 0x9e5d5157, 0x2000, start + i * 256, 256, i, blocks, family)
        assert block[288:508] == bytes(220) and struct.unpack_from("<I", block, 508)[0] == 0x0ab16f30
        reconstructed.extend(block[32:288])
    binary = bytes(reconstructed)
    final_bin = prefix.with_suffix(".bin")
    if final_bin.exists():
        assert final_bin.read_bytes() == binary
    # The optional final BIN is not required for independent UF2/ELF checks.
    raw = (REPO / ".build" / (prefix.name + ".bin")).read_bytes()
    assert binary == raw.ljust(end - start, b"\xff")
    sp, reset = struct.unpack_from("<2I", binary)
    assert 0x20000000 < sp <= (0x20020000 if mcu == "main" else 0x20040000) and not (sp & 7)
    assert reset & 1 and start <= reset < (0x08040000 if mcu == "main" else end)
    elf = REPO / ".build" / (prefix.name + ".elf")
    loads, symbols, symbol = load_elf(elf)
    for va, pa, value in loads:
        assert start <= pa < pa + len(value) <= end
        assert binary[pa - start:pa - start + len(value)] == value
    if mcu == "main":
        assert symbol("my2k_factory_image") == (BOARD / "defaults/factory.bin").read_bytes()
        definition = json.loads(lzma.decompress(symbol("keyboard_definition")))
        assert definition == json.loads((BOARD / "keymaps" / keymap / "vial.json").read_text())
        assert definition["name"] == NAME
        assert symbol("ProductString")[2:].decode("utf-16le").rstrip("\0") == NAME
        assert symbol("ManufacturerString")[2:].decode("utf-16le").rstrip("\0") == "Matrix"
        device = symbol("DeviceDescriptor")
        assert struct.unpack_from("<HH", device, 8) == (0x4d58, 0xf103)
        # Actual compiled QMK defaults, not just the source JSON.
        assert symbol("keymaps") == bytes(b for i in range(0, 1024, 2) for b in (BOARD / "defaults/factory.bin").read_bytes()[633+i:635+i][::-1])
        assert symbols["my2k_factory_image"][0] >= start  # Read-only flash, not SRAM.
    return dict(BIN_SHA256=hashlib.sha256(binary).hexdigest(), UF2_SHA256=hashlib.sha256(uf2).hexdigest(),
                flash_start=hex(start), flash_end_exclusive=hex(end), blocks=blocks,
                stack=hex(sp), reset=hex(reset), ELF_loads_match=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--keymap", choices=["vial", "vial_alec"], default="vial")
    parser.add_argument("--mcu", choices=["main", "aux", "both"], default="both")
    args = parser.parse_args()
    results = {}
    if args.mcu in ("main", "both"):
        results["main"] = check("main", 0x08020000, 0x08060000, 0x57755a57, args.keymap)
    if args.mcu in ("aux", "both"):
        results["aux"] = check("aux", 0x08000000, 0x08080000, 0x53b80f00)
    results["hardware_accessed"] = False
    print(json.dumps(results, indent=2))
