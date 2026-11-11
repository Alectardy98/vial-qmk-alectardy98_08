#!/usr/bin/env python3
"""Reproduce IBM definitions without importing the AVR scanner or pin settings.

The Rev 3 connector has eight sense inputs and eighteen drive outputs. Targets
that exceed those limits are deliberately stopped by their rules.mk, not built
as deceptively incomplete firmware. Source keymaps and Vial geometry are retained.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / 'keyboards/xwhatsit/ibm'
DEST = ROOT / 'keyboards/leyden_jar/ibm'
def source_transforms(board, matrix):
    """Compile the source scanner's transforms, including controller selection.

    Native Rev 3 sense channels match the universal/TEST decoded GPIO bit
    numbers on the shared sense connector (all eight verified on the FAT).
    F77/F62's older XWHATSIT compatibility permutation is a different family.
    """
    code = '#include <stdio.h>\n#define MATRIX_ROWS %d\n#define MATRIX_COLS %d\n' % (matrix['rows'], matrix['cols'])
    for path in (board / 'config.h', board / 'universal/config.h', ROOT / 'keyboards/xwhatsit/capsense.h'):
        code += '#include "%s"\n' % path
    code += '''
#if defined(CONTROLLER_IS_UNIVERSAL_MODEL_F)
#define SOURCE_FAMILY "universal_model_f"
#elif defined(CONTROLLER_IS_UNIVERSAL_BEAMSPRING)
#define SOURCE_FAMILY "universal_beamspring"
#elif defined(CONTROLLER_IS_TEST)
#define SOURCE_FAMILY "test"
#else
#error "Controller family needs an explicit connector translation"
#endif
int main(void) {
    printf("%s %d\\n", SOURCE_FAMILY, MATRIX_CAPSENSE_ROWS);
'''
    for row in range(matrix['rows']):
        code += 'printf("%%d ", %d < MATRIX_CAPSENSE_ROWS ? CAPSENSE_KEYMAP_ROW_TO_GPIO_ROW(%d) : -1);\n' % (row, row)
    for macro in ('CAPSENSE_KEYMAP_COL_TO_PHYSICAL_COL', 'CAPSENSE_KEYMAP_COL_TO_GPIO_COL'):
        code += 'printf("\\n");\n'
        code += '\n'.join('printf("%%d ", %s(%d));' % (macro, col) for col in range(matrix['cols']))
    code += '\n}\n'
    with tempfile.TemporaryDirectory(prefix='leyden-map-') as temp:
        folder = Path(temp)
        (folder / 'quantum.h').write_text('/* Host evaluation of configuration macros only. */\n')
        exe = folder / 'mapping'
        subprocess.run(['cc', '-I'+temp, '-x', 'c', '-', '-o', str(exe)], input=code, text=True, check=True)
        lines = subprocess.check_output([str(exe)], text=True).splitlines()
    family, count = lines[0].split()
    return {'family': family, 'capacitive_rows': int(count),
            'qmk_to_gpio_rows': list(map(int, lines[1].split())),
            'qmk_to_physical_cols': list(map(int, lines[2].split())),
            'qmk_to_gpio_cols': list(map(int, lines[3].split()))}


def board_support(name):
    # Verified Rev 3 R47/J2.30 wiring; user confirmed the switch closes to GND.
    return {'row': 8, 'col': 12, 'pin': 27} if name == '3178' else None


def board_column_adapter(name):
    # BEAMal's ordered physical sweep follows its source layout with reversed
    # columns. Keep the AVR TEST transform as source evidence, not as wiring.
    return {col: 11-col for col in range(12)} if name == 'beamal' else None


def matrix_mapping(info, transforms, configs, direct_key=None, extra_columns=False,
                   column_adapter=None):
    """Keep source signals distinct from native channels and logical positions.

    Compact only when the complete logical matrix fits the existing scanner.
    This selects a wiring adapter; it does not change Vial coordinates or claim
    that the source signal indices are numbered pins on an IBM connector.
    Existing in-range wiring maps stay unchanged.
    """
    matrix = info['matrix_size']
    positions = {tuple(key['matrix']) for layout in info['layouts'].values()
                 for key in layout['layout']}
    capacitive_positions = {(r, c) for r, c in positions if r < transforms['capacitive_rows']}
    used_cols = sorted({transforms['qmk_to_physical_cols'][col]
                        for _, col in capacitive_positions})
    source_cols = transforms['qmk_to_physical_cols']
    drive_count = 20 if extra_columns else 18
    compact = (matrix['cols'] <= drive_count and len(used_cols) <= drive_count
               and any(col < 0 or col >= 18 for col in source_cols)
               and all(col >= 0 for col in used_cols))
    adapter = {col: channel for channel, col in enumerate(used_cols)} if compact else {}
    if extra_columns:
        # 3276 physical sweep: connector drive order runs opposite the
        # original source's column indices. Rows retain their source order.
        adapter = {col: len(used_cols)-1-channel for channel,col in enumerate(used_cols)}
    if column_adapter is not None:
        assert set(column_adapter) == set(used_cols), 'Adapter must cover all source drives'
        assert len(set(column_adapter.values())) == len(used_cols), 'Adapter drives must be distinct'
        assert all(0 <= col < drive_count for col in column_adapter.values())
        adapter = dict(column_adapter)
    cols = [adapter.get(col, 255) for col in source_cols] if adapter else list(source_cols)
    row_count = 9 if direct_key else 8
    rows = [row if 0 <= row < 8 else 255 for row in transforms['qmk_to_gpio_rows'][:row_count]]
    rows += [255] * (row_count - len(rows))
    blocked = []
    if direct_key and positions - capacitive_positions != {(direct_key['row'], direct_key['col'])}:
        blocked.append('Direct-switch layout differs from the verified 3178 adapter')
    if matrix['rows'] > 8 and not (direct_key and matrix['rows'] == 9):
        blocked.append('Logical rows exceed eight; extra direct keys or custom sense wiring need an adapter mapping')
    if len(used_cols) > drive_count:
        blocked.append('%d populated drive signals exceed the Rev 3 eighteen-column scanner' % len(used_cols))
    elif matrix['cols'] > drive_count:
        blocked.append('Logical matrix exceeds the current eighteen-column mapping/protocol; remapping needs verification')
    elif any(not 0 <= cols[col] < drive_count for _, col in capacitive_positions):
        blocked.append('Source drive signals need a verified Rev 3 adapter mapping')
    if 'MATRIX_EXTRA_DIRECT_ROWS' in configs and not direct_key:
        blocked.append('AVR direct-key GPIO wiring has no verified Rev 3 equivalent')
    masks = [0] * (16 if direct_key else drive_count)
    for row, col in capacitive_positions:
        if row < 8 and rows[row] < 8 and 0 <= cols[col] < drive_count:
            masks[cols[col]] |= 1 << rows[row]
    return rows, cols, masks, blocked, {
        'populated_positions': len(positions),
        'used_source_columns': used_cols,
        'source_column_to_controller': {str(col): channel for col, channel in adapter.items()},
    }


def mapping_defines(rows, cols, masks):
    return {
        'LEYDEN_JAR_MATRIX_ROWS': rows,
        'LEYDEN_JAR_MATRIX_COLS': (cols + [255] * 18)[:max(18, len(masks))],
        'LEYDEN_JAR_ACTIVE_PADS': masks,
    }


def refresh_maps():
    """Update mapping files/inventory only; preserve keymaps and calibration."""
    path = DEST / 'ports.json'
    inventory = json.loads(path.read_text())
    for record in inventory:
        source = ROOT / record['source']
        info = json.loads((source / 'universal/keyboard.json').read_text())
        configs = (source / 'config.h').read_text() + (source / 'universal/config.h').read_text()
        transforms = source_transforms(source, info['matrix_size'])
        direct_key = board_support(record['board'])
        extra_columns = record['board'] == '3276_3278_75key'
        rows, cols, masks, blocked, usage = matrix_mapping(
            info, transforms, configs, direct_key, extra_columns, board_column_adapter(record['board']))
        dest = DEST / record['board']
        text = (dest / 'config.h').read_text()
        for name, values in mapping_defines(rows, cols, masks).items():
            rendered = ', '.join(hex(v) if name.endswith('PADS') else str(v) for v in values)
            text, count = re.subn(r'^#define '+name+r' .*$', '#define '+name+' {'+rendered+'}', text, flags=re.M)
            if count != 1:
                raise ValueError('Missing or duplicate mapping: '+str(dest)+' '+name)
        (dest / 'config.h').write_text(text)
        rules = 'include keyboards/leyden_jar/ibm/common/rules.mk\n'
        if blocked:
            rules += '$(error '+record['board']+': '+'; '.join(blocked)+'. See keyboards/leyden_jar/ibm/README.md)\n'
        (dest / 'rules.mk').write_text(rules)
        record.update(controller_rows=rows, controller_cols=cols, source_transforms=transforms,
                      status='blocked' if blocked else 'candidate_requires_physical_validation',
                      blocked_reasons=blocked, matrix_usage=usage)
        if direct_key:
            record.update(direct_key=direct_key, controller_scan_cols=16)
        if extra_columns:
            record.update(controller_scan_cols=20, controller_scan_rows=4,
                          gpio_column_pins=[4,5,6,7], status='modified_rev3_gpio_columns')
    path.write_text(json.dumps(inventory, indent=2)+'\n')
    print('Refreshed mapping-only files for %d ports; keymaps/calibration unchanged.' % len(inventory))


def generate():
    inventory = []
    for board in sorted(SOURCE.iterdir()):
        info_path = board / 'universal/keyboard.json'
        if not info_path.exists():
            continue
        info = json.loads(info_path.read_text())
        matrix = info['matrix_size']
        configs = (board / 'config.h').read_text() + (board / 'universal/config.h').read_text()
        transforms = source_transforms(board, matrix)
        direct_key = board_support(board.name)
        extra_columns = board.name == '3276_3278_75key'
        row_map, col_map, masks, blocked, usage = matrix_mapping(
            info, transforms, configs, direct_key, extra_columns, board_column_adapter(board.name))
        beamspring = ('CONTROLLER_IS_UNIVERSAL_BEAMSPRING' in configs
                      or 'CAPSENSE_CONDUCTIVE_PLASTIC_IS_PULLED_UP_ON_KEYPRESS' in configs)
        if 'CAPSENSE_CONDUCTIVE_PLASTIC_IS_PUSHED_DOWN_ON_KEYPRESS' in configs:
            beamspring = False
        dest = DEST / board.name
        dest.mkdir(parents=True, exist_ok=True)
        uid = hashlib.sha256(('leyden_jar/ibm/' + board.name).encode()).digest()[:8]
        info.update(processor='RP2040', board='GENERIC_RP_RP2040', bootloader='rp2040',
                    manufacturer='IBM / Eric Becourt / Alectardy98',
                    keyboard_name=info['keyboard_name'] + ' - Leyden Jar Rev 3',
                    usb={'vid': '0x1209', 'pid': '0x4704', 'device_version': '1.0.0'})
        info.pop('qmk', None)
        info['features'].update(bootmagic=False, console=False, command=False, nkro=True)
        (dest / 'keyboard.json').write_text(json.dumps(info, indent=4) + '\n')
        config = '#pragma once\n\n/* Leyden Jar Rev 3; native PIO, DAC and IO-expander drivers. */\n'
        config += '#define I2C_DRIVER I2CD0\n#define I2C1_SDA_PIN GP16\n#define I2C1_SCL_PIN GP17\n#define I2C1_CLOCK_SPEED 400000\n'
        config += '#define CONTROLLER_ROWS %d\n#define CONTROLLER_COLS %d\n' % (4 if extra_columns else 8, 20 if extra_columns else (16 if direct_key else 18))
        if extra_columns:
            config += '#define LEYDEN_JAR_GPIO_COLUMNS\n#define USB_WAIT_FOR_ENUMERATION\n#define LEYDEN_JAR_IO_EXPANDER_INPUT_MASK 0x60\n'
        config += '#define LEYDEN_JAR_MATRIX_ROWS {' + ', '.join(map(str, row_map)) + '}\n'
        config += '#define LEYDEN_JAR_MATRIX_COLS {' + ', '.join(map(str, (col_map + [255] * 18)[:max(18,len(masks))])) + '}\n'
        config += '#define LEYDEN_JAR_ACTIVE_PADS {' + ', '.join(hex(m) for m in masks) + '}\n'
        if direct_key:
            for field, value in direct_key.items():
                config += '#define LEYDEN_JAR_DIRECT_KEY_%s %s\n' % (field.upper(), ('GP'+str(value)) if field == 'pin' else value)
        if direct_key:
            config += '#define LEYDEN_JAR_LEVEL_SCAN_MAX 900\n'
        if extra_columns:
            config += '#include "calibration.h"\n'
        else:
            config += '#define NB_CAL_BINS 1\n#define ACTIVATION_OFFSETS {%d}\n' % (-30 if beamspring else 10)
        if beamspring:
            config += '#define BEAMSPRING_KEYBOARD\n'
        config += '#define HAPTIC_ENABLE_PIN GP28\n#define SOLENOID_PIN GP29\n#define SOLENOID_DEFAULT_DWELL 20\n#define SOLENOID_MIN_DWELL 4\n#define SOLENOID_MAX_DWELL 100\n#define HAPTIC_OFF_IN_LOW_POWER 1\n'
        config += '#define USB_POLLING_INTERVAL_MS 1\n#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET\n'
        (dest / 'config.h').write_text(config)
        for name in ('halconf.h', 'mcuconf.h'):
            shutil.copyfile(ROOT / 'keyboards/leyden_jar/f122' / name, dest / name)
        rules = 'include keyboards/leyden_jar/ibm/common/rules.mk\n'
        if blocked:
            rules += '$(error ' + board.name + ': ' + '; '.join(blocked) + '. See keyboards/leyden_jar/ibm/README.md)\n'
        (dest / 'rules.mk').write_text(rules)
        variants = []
        for definition in sorted((board / 'keymaps').glob('*/vial.json')):
            src_map, name = definition.parent, definition.parent.name
            out = dest / 'keymaps' / name
            out.mkdir(parents=True, exist_ok=True)
            vial = json.loads(definition.read_text())
            issues = []
            if vial['matrix'] != matrix:
                issues.append('Vial matrix dimensions differ from the source keyboard matrix')
            vial.update(vendorId='0x1209', productId='0x4704', name=vial['name'] + ' - Leyden Jar')
            (out / 'vial.json').write_text(json.dumps(vial, indent=2) + '\n')
            text = (src_map / 'config.h').read_text()
            text = re.sub(r'^#define VIAL_KEYBOARD_UID .*$', '#define VIAL_KEYBOARD_UID {' + ', '.join('0x%02X' % n for n in uid) + '}', text, flags=re.M)
            text = '\n'.join(line for line in text.splitlines() if not line.startswith(('#define CAPSENSE_', '#define DYNAMIC_KEYMAP_LAYER_COUNT', '#define VIAL_TAP_DANCE_ENTRIES', '#define VIAL_COMBO_ENTRIES', '#define VIAL_KEY_OVERRIDE_ENTRIES'))) + '\n'
            if name == 'vial_alec' and (out / 'calibration.h').exists():
                # Preserve the hardware-specific native calibration override.
                text += '\n#include \"calibration.h\"\n'
            if (out / 'population.h').exists():
                text += '\n#include "population.h"\n'
            if (dest / 'calibration.h').exists() and name in ('vial', 'vial_alec'):
                text += '\n#include "../../calibration.h"\n'
            (out / 'config.h').write_text(text)
            keymap = (src_map / 'keymap.c').read_text()
            if re.search(r'writePin(?:High|Low)\(', keymap):
                keymap = keymap.replace('#include QMK_KEYBOARD_H', '#include QMK_KEYBOARD_H\n#include "keyboards/leyden_jar/io_expander.h"')
                for pin, led in [('D7', 0), ('E6', 1), ('B6', 2)]:
                    for level, value in [('High', 'true'), ('Low', 'false')]:
                        keymap = keymap.replace('writePin%s(%s)' % (level, pin), 'io_expander_set_led%d_status(%s)' % (led, value))
            (out / 'keymap.c').write_text(keymap)
            krules = 'include keyboards/leyden_jar/vial_features.mk\n'
            if issues:
                krules += '\n$(error ' + '; '.join(issues) + '. Source definition requires correction before release)\n'
            (out / 'rules.mk').write_text(krules)
            variants.append({'keymap': name, 'source_sha256': hashlib.sha256(definition.read_bytes()).hexdigest(), 'issues': issues})
        inventory.append({'board': board.name, 'source': str(board.relative_to(ROOT)), 'matrix': matrix,
                          'controller_revision': 3, 'controller_rows': row_map, 'controller_cols': col_map,
                          'source_transforms': transforms, 'matrix_usage': usage,
                          **({'direct_key': direct_key, 'controller_scan_cols': 16} if direct_key else {}),
                          **({'controller_scan_cols':20,'controller_scan_rows':4,'gpio_column_pins':[4,5,6,7]} if extra_columns else {}),
                          'mapping_verification': 'source compiler checked; physical connector verified on FAT only',
                          'switch_technology': 'beamspring' if beamspring else 'model_f',
                          'status': 'blocked' if blocked else 'candidate_requires_physical_validation',
                          'blocked_reasons': blocked, 'variants': variants, 'uid': uid.hex()})
    (DEST / 'ports.json').write_text(json.dumps(inventory, indent=2) + '\n')
    print(json.dumps({'boards': len(inventory), 'candidates': sum(not b['blocked_reasons'] for b in inventory),
                      'blocked': [b['board'] for b in inventory if b['blocked_reasons']]}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--maps-only', action='store_true', help='Preserve keymaps and calibration; refresh mapping definitions/inventory')
    args = parser.parse_args()
    refresh_maps() if args.maps_only else generate()
