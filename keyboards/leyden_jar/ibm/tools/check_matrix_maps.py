#!/usr/bin/env python3
"""Check native maps against actual AVR scanner macros; optionally inspect UF2s.

Run after port_ibm.py, and after builds with --compiled. No hardware is opened.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import tempfile
from port_ibm import ROOT, source_transforms


def native_arrays(config):
    code = '#include <stdint.h>\n#include <stdio.h>\n#include "%s"\n' % config
    code += '''
static const uint8_t rows[] = LEYDEN_JAR_MATRIX_ROWS;
static const uint8_t cols[] = LEYDEN_JAR_MATRIX_COLS;
static const uint8_t pads[] = LEYDEN_JAR_ACTIVE_PADS;
int main(void) {
    for (unsigned n=0;n<sizeof rows;n++) printf("%u ", rows[n]);
    puts("");
    for (unsigned n=0;n<sizeof cols;n++) printf("%u ", cols[n]);
    puts("");
    for (unsigned n=0;n<sizeof pads;n++) printf("%u ", pads[n]);
}
'''
    with tempfile.TemporaryDirectory() as folder:
        exe = Path(folder) / 'native-map'
        subprocess.run(['cc', '-x', 'c', '-', '-o', str(exe)], input=code, text=True, check=True)
        return tuple(bytes(map(int, line.split())) for line in
                     subprocess.check_output([str(exe)], text=True).splitlines())


def compiled_arrays(name):
    raw = (ROOT / (name + '.uf2')).read_bytes()
    blocks = {}
    for offset in range(0, len(raw), 512):
        _, _, _, address, size, *_ = struct.unpack_from('<8I', raw, offset)
        blocks[address] = raw[offset+32:offset+32+size]
    base = min(blocks)
    image = b''.join(blocks[address] for address in sorted(blocks))
    symbols = {}
    for line in subprocess.check_output(['arm-none-eabi-nm', '-S',
                                        str(ROOT / '.build' / (name + '.elf'))], text=True).splitlines():
        parts = line.split()
        if len(parts) == 4:
            symbols[parts[3]] = int(parts[0], 16), int(parts[1], 16)
    values = []
    for symbol in ('s_matrixToControllerRow', 's_matrixToControllerCol', 's_active_pads'):
        address, size = symbols[symbol]
        values.append(image[address-base:address-base+size])
    return tuple(values)


def check(compiled=False):
    ports = json.loads((ROOT / 'keyboards/leyden_jar/ibm/ports.json').read_text())
    count = 0
    for board in ports:
        source = ROOT / board['source']
        info = json.loads((source / 'universal/keyboard.json').read_text())
        golden = source_transforms(source, info['matrix_size'])
        # Independent physical regression anchor: all eight rows were measured
        # on the FAT, not inferred from a layout JSON or the generator output.
        if board['board'] == 'fat':
            assert golden['qmk_to_gpio_rows'] == [7, 6, 5, 4, 3, 2, 1, 0]
        rows = [row if 0 <= row < 8 else 255 for row in golden['qmk_to_gpio_rows'][:8]]
        rows += [255] * (8 - len(rows))
        cols = golden['qmk_to_physical_cols']
        pads = [0] * 18
        for layout in info['layouts'].values():
            for key in layout['layout']:
                row, col = key['matrix']
                if row < 8 and rows[row] < 8 and 0 <= cols[col] < 18:
                    pads[cols[col]] |= 1 << rows[row]
        expected = bytes(rows), bytes((cols+[255]*18)[:18]), bytes(pads)
        dest = ROOT / 'keyboards/leyden_jar/ibm' / board['board']
        assert native_arrays(dest / 'config.h') == expected, board['board']
        assert board['source_transforms'] == golden, 'Regenerate stale inventory: ' + board['board']
        if compiled and not board['blocked_reasons']:
            for variant in board['variants']:
                if variant['issues']:
                    continue
                name = 'leyden_jar_ibm_' + board['board'] + '_' + variant['keymap']
                assert compiled_arrays(name) == expected, name
                count += 1
    print('Source and pad masks checked for %d boards; %d compiled UF2 map tables checked.' % (len(ports), count))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiled', action='store_true', help='Also inspect every compatible built UF2/ELF')
    args = parser.parse_args()
    check(args.compiled)
