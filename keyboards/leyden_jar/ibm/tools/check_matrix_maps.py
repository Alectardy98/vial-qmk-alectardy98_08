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


def native_arrays(config, keymap_config=None):
    code = '#include <stdint.h>\n#include <stdio.h>\n#include "%s"\n' % config
    if keymap_config is not None:
        code += '#include "%s"\n' % keymap_config
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
    symbols = {}
    for line in subprocess.check_output(['arm-none-eabi-nm', '-S',
                                        str(ROOT / '.build' / (name + '.elf'))], text=True).splitlines():
        parts = line.split()
        if len(parts) == 4:
            symbols[parts[3]] = int(parts[0], 16), int(parts[1], 16)
    # Const tables can be folded into .data by LTO: ELF symbol VMA is then
    # RAM, while UF2 contains the section's flash load address (LMA).
    sections = []
    for line in subprocess.check_output(['arm-none-eabi-objdump', '-h',
                                        str(ROOT / '.build' / (name + '.elf'))], text=True).splitlines():
        parts = line.split()
        if len(parts) >= 7 and parts[0].isdigit():
            sections.append(tuple(int(parts[n], 16) for n in (2, 3, 4)))
    values = []
    for symbol in ('s_matrixToControllerRow', 's_matrixToControllerCol', 's_active_pads'):
        address, size = symbols[symbol]
        section = next((item for item in sections
                        if item[1] <= address and address + size <= item[1] + item[0]), None)
        assert section is not None, (name, symbol, hex(address))
        load = address - section[1] + section[2]
        data = bytearray()
        for target in range(load, load + size):
            block = next((start for start in blocks if start <= target < start + len(blocks[start])), None)
            assert block is not None, (name, symbol, hex(target))
            data.append(blocks[block][target - block])
        values.append(bytes(data))
    return tuple(values)


def check(compiled=False, selected=None):
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
        direct = board.get('direct_key')
        if direct:
            assert board['board'] == '3178' and direct == {'row': 8, 'col': 12, 'pin': 27}
        row_count = 9 if direct else 8
        rows = [row if 0 <= row < 8 else 255 for row in golden['qmk_to_gpio_rows'][:row_count]]
        rows += [255] * (row_count - len(rows))
        source_cols = golden['qmk_to_physical_cols']
        positions = {tuple(key['matrix']) for layout in info['layouts'].values()
                     for key in layout['layout']}
        capacitive = {(r, c) for r, c in positions if r < golden['capacitive_rows']}
        used_cols = sorted({source_cols[col] for _, col in capacitive})
        adapter = {}
        drive_count = board.get('controller_scan_cols',18)
        if (info['matrix_size']['cols'] <= drive_count and len(used_cols) <= drive_count
                and all(col >= 0 for col in used_cols)
                and any(not 0 <= col < 18 for col in source_cols)):
            adapter = {str(col): channel for channel, col in enumerate(used_cols)}
            if board['board'] == '3276_3278_75key':
                adapter = {str(col): len(used_cols)-1-channel for channel,col in enumerate(used_cols)}
            cols = [adapter.get(str(col), 255) for col in source_cols]
            assert len(set(cols[col] for _, col in positions)) == len(used_cols)
        else:
            cols = source_cols
        if board['board'] == 'beamal':
            # Independent anchor from the installed board's ordered sweep.
            assert used_cols == list(range(12))
            adapter = {str(col): 11-col for col in used_cols}
            cols = [adapter[str(col)] for col in source_cols]
        assert board['matrix_usage'] == {
            'populated_positions': len(positions), 'used_source_columns': used_cols,
            'source_column_to_controller': adapter}, board['board']
        if board['board'] == '3277_66key':
            assert used_cols == list(range(5, 13)) + list(range(14, 23))
            assert cols == list(range(17)) and len(positions) == 66
        if board['board'] == '3277_split_66key':
            assert used_cols == list(range(4, 13)) + list(range(14, 23))
            assert cols == list(range(18)) and len(positions) == 69
        if board['board'] == '3276_3278_75key':
            assert used_cols == list(range(3,23)) and cols == list(reversed(range(20)))
            assert len(positions) == 75 and board['gpio_column_pins'] == [4,5,6,7]
        pads = [0] * board.get('controller_scan_cols', 18)
        for layout in info['layouts'].values():
            for key in layout['layout']:
                row, col = key['matrix']
                if row < 8 and rows[row] < 8 and 0 <= cols[col] < drive_count:
                    pads[cols[col]] |= 1 << rows[row]
        if not board['blocked_reasons']:
            # Every populated logical position must retain a distinct native
            # intersection, including layout alternatives and unassigned keys.
            assert len({(rows[row], cols[col]) for row, col in capacitive}) == len(capacitive), board['board']
            assert sum(mask.bit_count() for mask in pads) == len(capacitive), board['board']
        expected = bytes(rows), bytes((cols+[255]*18)[:max(18,drive_count)]), bytes(pads)
        dest = ROOT / 'keyboards/leyden_jar/ibm' / board['board']
        assert native_arrays(dest / 'config.h') == expected, board['board']
        assert board['source_transforms'] == golden, 'Regenerate stale inventory: ' + board['board']
        if compiled and not board['blocked_reasons'] and (not selected or board['board'] in selected):
            for variant in board['variants']:
                if variant['issues']:
                    continue
                name = 'leyden_jar_ibm_' + board['board'] + '_' + variant['keymap']
                variant_expected = expected
                if board['board'] == 'beamal' and variant['keymap'] == 'vial_alec':
                    # The shorter side clusters have six unpopulated optional
                    # bottom pads; keep the standard map's full layout union.
                    personal_pads = list(pads)
                    for row, col in ((0,0),(0,1),(0,9),(0,10),(0,11),(1,9)):
                        personal_pads[col] &= ~(1 << row)
                    variant_expected = expected[:2] + (bytes(personal_pads),)
                    assert native_arrays(dest/'config.h', dest/'keymaps/vial_alec/config.h') == variant_expected
                assert compiled_arrays(name) == variant_expected, name
                count += 1
    print('Source and pad masks checked for %d boards; %d compiled UF2 map tables checked.' % (len(ports), count))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiled', action='store_true', help='Also inspect every compatible built UF2/ELF')
    parser.add_argument('--boards', nargs='+', help='Limit compiled checks to these boards; source checks still cover all')
    args = parser.parse_args()
    check(args.compiled, args.boards)
