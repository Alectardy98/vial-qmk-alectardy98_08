#!/usr/bin/env python3
"""Package Prox ELF flash sections with erased gaps; no device access.

Use section contents, not PT_LOAD padding (which can contain zero bytes).
This is the same clean-install mapping validated in development/package.py.
The carrier requires an intentional clean erase; it is not an EEPROM backup.
"""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess


def package(elf, output, objdump):
    raw = elf.read_bytes()
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', raw)
    assert header[0][:7] == b'\x7fELF\x01\x01\x01' and header[2] == 40, 'Expected ARM ELF32 LE'
    segments = []
    for i in range(header[10]):
        kind, offset, _, address, size, _, _, _ = struct.unpack_from(
            '<8I', raw, header[5] + i * header[9])
        if kind == 1 and size and 0x08000000 <= address < 0x08100000:
            assert address + size <= 0x08004000 or address >= 0x08008000, 'ELF overlaps EEPROM'
            segments.append((address, size))
    start = min(address for address, _ in segments)
    end = max(address + size for address, size in segments)
    assert start == 0x08000000 and end <= 0x08020000, 'Unexpected Prox flash layout'
    image = bytearray(b'\xff' * (end - start))
    headers = subprocess.check_output([objdump, '-h', str(elf)], text=True).splitlines()
    for i, line in enumerate(headers[:-1]):
        fields = line.split()
        if len(fields) != 7 or not fields[0].isdigit():
            continue
        if 'CONTENTS' not in headers[i + 1] or 'LOAD' not in headers[i + 1]:
            continue
        size, address, offset = int(fields[2], 16), int(fields[4], 16), int(fields[5], 16)
        if size and 0x08000000 <= address < 0x08100000:
            assert start <= address and address + size <= end
            assert address + size <= 0x08004000 or address >= 0x08008000
            assert offset + size <= len(raw)
            image[address - start:address - start + size] = raw[offset:offset + size]
    assert image[0x4000:0x8000] == b'\xff' * 0x4000
    sp, reset = struct.unpack_from('<II', image)
    assert 0x20000000 < sp <= 0x20010000 and reset & 1
    assert 0x08008000 <= reset & ~1 < end
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(image)
    print(f'Prox clean-install: {output} ({len(image)} bytes, SHA-256 {hashlib.sha256(image).hexdigest()})')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--objdump', default='arm-none-eabi-objdump')
    args = parser.parse_args()
    package(args.elf, args.output, args.objdump)
