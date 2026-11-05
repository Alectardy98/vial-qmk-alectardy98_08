#!/usr/bin/env python3
"""Package and independently check compiled Virgo bootloader/application images.

No USB discovery or flashing. Combined BIN is an intentional clean installation
at 0x08000000; application UF2 is for subsequent TinyUF2 updates only.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

BOARD = Path(__file__).resolve().parents[1]
ROOT = BOARD.parents[2]
APP_START, FLASH_END, MAILBOX = 0x08010000, 0x08040000, 0x2001FFFC


def symbols(elf):
    result = {}
    for line in subprocess.check_output(['arm-none-eabi-nm', '-n', str(elf)], text=True).splitlines():
        fields = line.split()
        if len(fields) == 3:
            result[fields[2]] = int(fields[0], 16)
    return result


def load_segments(elf):
    raw = elf.read_bytes()
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', raw)
    assert header[0][:7] == b'\x7fELF\x01\x01\x01' and header[2] == 40
    for i in range(header[10]):
        kind, offset, _, lma, size, _, _, _ = struct.unpack_from('<8I', raw, header[5]+i*header[9])
        if kind == 1 and size and 0x08000000 <= lma < FLASH_END:
            yield lma, raw[offset:offset+size]


def read_uf2(path):
    raw = path.read_bytes()
    assert len(raw) and len(raw) % 512 == 0
    blocks, count = {}, len(raw)//512
    for i in range(count):
        block = raw[i*512:(i+1)*512]
        magic0, magic1, flags, addr, size, seq, total, family = struct.unpack_from('<8I', block)
        assert (magic0, magic1, struct.unpack_from('<I', block, 508)[0]) == (0x0a324655, 0x9e5d5157, 0x0ab16f30)
        assert flags == 0x2000 and size == 256 and total == count and seq == i and family == 0x57755a57
        assert APP_START <= addr <= FLASH_END-256 and addr % 256 == 0 and addr not in blocks
        blocks[addr] = block[32:32+256]
    start, end = min(blocks), max(blocks)+256
    assert start == APP_START and len(blocks)*256 == end-start
    return b''.join(blocks[a] for a in range(start, end, 256))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boot-build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve();out.mkdir(parents=True, exist_ok=True)
    boot = (args.boot_build / 'ec_virgo_tinyuf2.bin').read_bytes()
    boot_elf = args.boot_build / 'ec_virgo_tinyuf2.elf'
    assert len(boot) <= 0x7c00
    bsp, br = struct.unpack_from('<II', boot)
    assert bsp == MAILBOX-4 and br & 1 and 0x08000000 <= (br & ~1) < 0x08007c00
    syms = symbols(boot_elf)
    assert syms['_board_dfu_dbl_tap'] == MAILBOX and syms['_estack'] == MAILBOX-4
    for addr, data in load_segments(boot_elf):
        assert addr+len(data) <= 0x08008000
        assert boot[addr-0x08000000:addr-0x08000000+len(data)] == data
    for text in (b'Virgo TinyUF2', b'VIRGOBOOT', b'EC-Virgo-STM32F411'):
        # USB strings are UTF16; INFO_UF2 carries the ASCII identity too.
        assert text in boot or text.decode().encode('utf-16le') in boot
    results = []
    for keymap in ('vial', 'vial_alec'):
        stem = 'alectardy98_ec_ec_virgo_uf2_' + keymap
        elf, uf2 = ROOT/'.build'/(stem+'.elf'), ROOT/(stem+'.uf2')
        app = read_uf2(uf2)
        sp, reset = struct.unpack_from('<II', app)
        assert 0x20000000 < sp <= MAILBOX-4 and not sp % 8 and reset & 1
        assert APP_START <= (reset & ~1) < APP_START+len(app)
        assert b'A98-virgo-UF2-2.9g' in app
        syms = symbols(elf)
        assert syms['__flash0_base__'] == APP_START and syms['__flash0_end__'] == FLASH_END
        assert syms['_board_dfu_dbl_tap'] == syms['__heap_end__'] == MAILBOX
        assert syms['__bss_end__'] <= MAILBOX
        assert struct.pack('<I', MAILBOX) in app and struct.pack('<I', 0xf01669ef) in app
        for addr, data in load_segments(elf):
            assert APP_START <= addr and addr+len(data) <= FLASH_END
            assert app[addr-APP_START:addr-APP_START+len(data)] == data
        combined = bytearray(b'\xff' * (0x10000+len(app)))
        combined[:len(boot)] = boot;combined[0x10000:] = app
        assert combined[0x8000:0x10000] == b'\xff'*0x8000
        path = out/('EC-Virgo-TinyUF2-'+keymap+'-FIRST-INSTALL.bin')
        path.write_bytes(combined)
        (out/(stem+'.application.bin')).write_bytes(app)
        shutil.copy2(uf2, out/uf2.name)
        shutil.copy2(elf, out/elf.name)
        shutil.copy2(elf.with_suffix('.map'), out/elf.with_suffix('.map').name)
        results.append({'keymap':keymap, 'app_bytes_padded':len(app), 'UF2_bytes':uf2.stat().st_size,
                        'combined':path.name, 'sha256':hashlib.sha256(combined).hexdigest(),
                        'settings_gap':'FF, verified', 'mailbox':hex(MAILBOX), 'ELF_vs_UF2':'exact load match'})
    for path in args.boot_build.glob('ec_virgo_tinyuf2.*'):
        shutil.copy2(path, out/path.name)
    (out/'image-validation.json').write_text(json.dumps(results, indent=2)+'\n')
    sums=[hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name for p in sorted(out.iterdir()) if p.suffix in ('.bin','.uf2','.hex')]
    (out/'SHA256SUMS').write_text('\n'.join(sums)+'\n')
    print(json.dumps(results, indent=2))


if __name__ == '__main__':
    main()
