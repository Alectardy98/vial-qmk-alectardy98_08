#!/usr/bin/env python3
"""Reproduce IBM definitions without importing the AVR scanner or pin settings.

The Rev 3 connector has eight sense inputs and eighteen drive outputs. Targets
that exceed those limits are deliberately stopped by their rules.mk, not built
as deceptively incomplete firmware. Source keymaps and Vial geometry are retained.
"""
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
        source_rows = transforms['qmk_to_gpio_rows']
        row_map = [row if 0 <= row < 8 else 255 for row in source_rows[:8]]
        row_map += [255] * (8 - len(row_map))
        col_map = transforms['qmk_to_physical_cols']
        blocked = []
        if matrix['rows'] > 8:
            blocked.append('Logical rows exceed eight; extra direct keys or custom sense wiring need an adapter mapping')
        if matrix['cols'] > 18:
            blocked.append('Required drive columns exceed the Rev 3 eighteen-column connector')
        elif any(c < 0 or c >= 18 for c in col_map):
            blocked.append('Source connector column numbering needs a verified compact Rev 3 adapter mapping')
        if 'MATRIX_EXTRA_DIRECT_ROWS' in configs:
            blocked.append('AVR direct-key GPIO wiring has no verified Rev 3 equivalent')
        masks = [0] * 18
        for layout in info['layouts'].values():
            for key in layout['layout']:
                row, col = key['matrix']
                if row < 8 and row_map[row] < 8 and col < len(col_map) and 0 <= col_map[col] < 18:
                    masks[col_map[col]] |= 1 << row_map[row]
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
        config += '#define CONTROLLER_ROWS 8\n#define CONTROLLER_COLS 18\n'
        config += '#define LEYDEN_JAR_MATRIX_ROWS {' + ', '.join(map(str, row_map)) + '}\n'
        config += '#define LEYDEN_JAR_MATRIX_COLS {' + ', '.join(map(str, (col_map + [255] * 18)[:18])) + '}\n'
        config += '#define LEYDEN_JAR_ACTIVE_PADS {' + ', '.join(hex(m) for m in masks) + '}\n'
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
            text = '\n'.join(line for line in text.splitlines() if not line.startswith('#define CAPSENSE_')) + '\n'
            if board.name == 'fat' and name == 'vial_alec':
                # Preserve the hardware-specific native calibration override.
                text += '\n#include \"calibration.h\"\n'
            (out / 'config.h').write_text(text)
            keymap = (src_map / 'keymap.c').read_text()
            if re.search(r'writePin(?:High|Low)\(', keymap):
                keymap = keymap.replace('#include QMK_KEYBOARD_H', '#include QMK_KEYBOARD_H\n#include "keyboards/leyden_jar/io_expander.h"')
                for pin, led in [('D7', 0), ('E6', 1), ('B6', 2)]:
                    for level, value in [('High', 'true'), ('Low', 'false')]:
                        keymap = keymap.replace('writePin%s(%s)' % (level, pin), 'io_expander_set_led%d_status(%s)' % (led, value))
            (out / 'keymap.c').write_text(keymap)
            krules = (src_map / 'rules.mk').read_text()
            krules = re.sub(r'^BOOTMAGIC_ENABLE\s*=.*$', 'BOOTMAGIC_ENABLE = no', krules, flags=re.M)
            if issues:
                krules += '\n$(error ' + '; '.join(issues) + '. Source definition requires correction before release)\n'
            (out / 'rules.mk').write_text(krules)
            variants.append({'keymap': name, 'source_sha256': hashlib.sha256(definition.read_bytes()).hexdigest(), 'issues': issues})
        inventory.append({'board': board.name, 'source': str(board.relative_to(ROOT)), 'matrix': matrix,
                          'controller_revision': 3, 'controller_rows': row_map, 'controller_cols': col_map,
                          'source_transforms': transforms,
                          'mapping_verification': 'source compiler checked; physical connector verified on FAT only',
                          'switch_technology': 'beamspring' if beamspring else 'model_f',
                          'status': 'blocked' if blocked else 'candidate_requires_physical_validation',
                          'blocked_reasons': blocked, 'variants': variants, 'uid': uid.hex()})
    (DEST / 'ports.json').write_text(json.dumps(inventory, indent=2) + '\n')
    print(json.dumps({'boards': len(inventory), 'candidates': sum(not b['blocked_reasons'] for b in inventory),
                      'blocked': [b['board'] for b in inventory if b['blocked_reasons']]}, indent=2))


if __name__ == '__main__':
    generate()
