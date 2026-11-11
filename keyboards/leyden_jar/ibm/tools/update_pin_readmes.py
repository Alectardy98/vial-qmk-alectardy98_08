#!/usr/bin/env python3
"""Update only README pin references from current board/source configurations.

No firmware build, source regeneration or USB access. --check detects stale docs.
Rev 3 connector wiring was checked with read-pcb against the schematic revision
linked below; matrix assignments are separately derived from each port's source.
"""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from port_ibm import ROOT
from check_matrix_maps import native_arrays

LEYDEN = ROOT / 'keyboards/leyden_jar'
BEGIN = '<!-- BEGIN LEYDEN JAR PIN REFERENCE -->'
END = '<!-- END LEYDEN JAR PIN REFERENCE -->'
SCHEMATIC = ('https://github.com/mymakercorner/Leyden_Jar/blob/e01dea552d0a/'
             'pcb/leyden_jar_controller/Leyden_Jar.kicad_sch')
# Header pin order, not a view from either side of the physical connector.
J3 = ('GND', '3.3V', 'SWCLK', 'SDA (GP16)', 'SWDIO (schematic: SWIO)', 'SCL (GP17)')
J4 = ('+5V', 'Solenoid enable (GP28)', 'PS2_ENABLE', 'Solenoid pulse (GP29)', '3.3V', 'GND')


def current_native_map(folder, matrix):
    """Preprocess the actual common.c selection, rather than duplicate its maps."""
    with tempfile.TemporaryDirectory() as tmp:
        temp = Path(tmp)
        (temp/'quantum.h').write_text('#pragma once\n#include <stdint.h>\n#include <stdbool.h>\n')
        (temp/'haptic.h').write_text('')
        result = subprocess.run(['cc', '-E', '-P', '-I'+tmp,
                                 '-DMATRIX_ROWS='+str(matrix['rows']),
                                 '-DMATRIX_COLS='+str(matrix['cols']),
                                 '-include', str(folder/'config.h'), str(LEYDEN/'common.c')],
                                capture_output=True, text=True, check=True)
    arrays = []
    for name in ('s_matrixToControllerRow', 's_matrixToControllerCol'):
        match = re.search(r'\b'+name+r'\s*\[[^]]*\]\s*=\s*\{([^}]*)\}', result.stdout)
        if match is None:
            return None
        arrays.append([int(x.strip(), 0) for x in match[1].split(',') if x.strip()])
    return arrays


def inventory():
    records = []
    for port in json.loads((LEYDEN/'ibm/ports.json').read_text()):
        folder = LEYDEN/'ibm'/port['board']
        info = json.loads((folder/'keyboard.json').read_text())
        rows, cols, _ = native_arrays(folder/'config.h')
        expected_rows = bytes(port['controller_rows'])
        expected_cols = bytes((port['controller_cols']+[255]*18)[:max(18,port.get('controller_scan_cols',18))])
        if rows != expected_rows or cols != expected_cols:
            raise ValueError('Stale port inventory: '+port['board'])
        records.append(dict(folder=folder, name=info['keyboard_name'], matrix=port['matrix'],
                            rows=port['source_transforms']['qmk_to_gpio_rows'],
                            cols=port['controller_cols'], reasons=port['blocked_reasons'],
                            technology=port['switch_technology'], original=port['source'],
                            sense_rows=port['source_transforms']['capacitive_rows'],
                            family=port['source_transforms']['family'], scanned_cols=port.get('controller_scan_cols',18), legacy=False,
                            direct_key=port.get('direct_key'),
                            usage=port.get('matrix_usage'),
                            source_cols=port['source_transforms']['qmk_to_physical_cols']))
        records[-1]['gpio_column_pins'] = port.get('gpio_column_pins')
        mask = re.search(r'^#define LEYDEN_JAR_IO_EXPANDER_INPUT_MASK\s+(\S+)', (folder/'config.h').read_text(), re.M)
        records[-1]['io_expander_input_mask'] = int(mask[1],0) if mask else 0
    # Cover reproduction-board beamspring targets too, keeping them distinct
    # from original IBM assemblies. The two old targets have no current map.
    for folder in sorted(LEYDEN.iterdir()):
        definition = folder/'keymaps/vial/vial.json'
        if not folder.is_dir() or not definition.exists() or not (folder/'config.h').exists():
            continue
        vial = json.loads(definition.read_text())
        matrix = vial['matrix']
        mapping = current_native_map(folder, matrix)
        try:
            info = json.loads((folder/'info.json').read_text())
        except json.JSONDecodeError:
            info = {}
        config = (folder/'config.h').read_text()
        match = re.search(r'^#define CONTROLLER_COLS\s+(\d+)', config, re.M)
        records.append(dict(folder=folder, name=info.get('keyboard_name', folder.name), matrix=matrix,
                            rows=mapping[0][:matrix['rows']] if mapping else None,
                            cols=mapping[1][:matrix['cols']] if mapping else None,
                            reasons=[] if mapping else ['Legacy target has no matrix-format selection recognized by current common.c; build metadata/configuration also needs repair.'],
                            technology='beamspring' if 'BEAMSPRING_KEYBOARD' in config else 'model_f',
                            original=None, sense_rows=8 if mapping else None,
                            family='native reproduction target',
                            scanned_cols=int(match[1]) if match else None, legacy=mapping is None))
    return records


def inverse(mapping, physical, logical_count):
    if mapping is None:
        return None
    return [i for i, value in enumerate(mapping[:logical_count]) if value == physical]


def assignment(record, kind, physical):
    direct = record.get('direct_key')
    if kind == 'column' and direct and physical == 17:
        return 'Blue button to GND (GPIO27 input; excluded from PIO)'
    if kind == 'column' and direct and physical == direct['col']:
        return 'No capacitive pads; blue button is a separate switch'
    values = inverse(record['rows' if kind == 'row' else 'cols'], physical,
                     record['matrix']['rows' if kind == 'row' else 'cols'])
    if values is None:
        return 'Unverified; legacy port'
    if not values:
        if kind == 'column' and physical >= record['scanned_cols']:
            return 'Not scanned by this target'
        return 'No assigned '+kind
    text = 'QMK '+kind+' '+', '.join(map(str, values))
    return text + (' [source only; BLOCKED]' if record['reasons'] else '')


def render(record):
    blocked = bool(record['reasons'])
    gpio_columns = record.get('gpio_column_pins')
    lines = [BEGIN, '## Leyden Jar Rev 3 pin reference', '',
             '**Logical diagram, not a solder-side view.** Match the numbered PCB pads; '
             'the drawing does not establish connector orientation. QMK rows/columns are '
             'zero-based firmware coordinates, **not original IBM ribbon or connector pin numbers**.', '']
    lines += [f'**Keyboard source declares:** {record["matrix"]["rows"]} logical rows × '
              f'{record["matrix"]["cols"]} logical columns. '
              f'**Rev 3 controller capacity:** 8 sense inputs and 18 drive outputs.', '']
    if record['sense_rows'] is not None:
        lines += [f'The original/native scanner declares **{record["sense_rows"]} capacitive sense rows**. '
                  'Any extra QMK logical rows may represent direct keys rather than additional capacitive rows. '
                  'All eight A_ROW pins below belong to the controller; unassigned ones do not '
                  'imply extra rows on the keyboard.', '']
    if record['original'] and record['family'] == 'test':
        lines += ['**TEST-controller source definition:** this target uses the source '
                  '`CONTROLLER_IS_TEST` mapping. It is not a verified standard IBM beamspring '
                  'connector reference; do not generalize its dimensions or identity mapping '
                  'to a 327x/5251 keyboard.', '']
    if record['folder'].name == 'beamal':
        lines += ['**BEAMal physical sweep:** the Rev 3 connector carries source columns '
                  '11 down to 0 on COL0 through COL11; rows retain their source order. '
                  'The 2026-11-11 capture matched all 88 shorter-layout keys to the original layout. '
                  '`vial_alec` excludes six unpopulated bottom side-cluster positions '
                  'for the shorter side layout; standard `vial` keeps all 94 source positions. '
                  'All five spacebar-row switches, including right Ctrl, remain enabled.', '']
    if record['original'] and record['family'] == 'universal_beamspring' and record['sense_rows'] == 4:
        lines += ['The [original Beamspring-USB installation guide, page 2]'
                  '(https://static.wongcornall.com/ibm-capsense-usb/installation_beamspring.pdf) '
                  'describes the standard 4-row / 23-column interface and treats Displaywriter '
                  'separately. This keyboard source may use only a subset of those columns. '
                  'The stock J2 connector has 18 drive outputs: ports needing more signals require '
                  'additional verified hardware/driver support. The populated-signal audit is in '
                  '[DRIVE-CAPACITY.md](../DRIVE-CAPACITY.md).', '']
    usage = record.get('usage')
    if gpio_columns:
        lines += ['**3276 modified-header candidate.** J2.3-6 are the four sense inputs. '
                  'J2.7-10 drive columns 0-3 through GPIO4-7 after U6 removal. '
                  'Install four 1 kΩ series links at U6, pads 2-to-5, 1-to-7, '
                  '14-to-9 and 13-to-11. Remove R27, R35, R39 and R37 to '
                  'disconnect these former sense inputs from the shared bias rail. '
                  'J2.13-28 provide the remaining sixteen drives. The installed physical '
                  'sweep verified all 75 key positions with two calibration bins.', '']
    if usage:
        lines += [f"**Populated source-layout union:** {usage['populated_positions']} positions, "
                  f"{len(usage['used_source_columns'])} distinct drive signals. Empty intersections "
                  "do not free a column unless that entire column is unused.", '']
        if usage['source_column_to_controller']:
            lines += ['**Compact adapter mapping:** the firmware now uses the mapping below. '
                      'Wire the corresponding source signals through an adapter; this is not '
                      'a drop-in match for their old column numbers. Physical connector contact '
                      'numbers/orientation still require continuity verification.', '',
                      '| QMK column | Source column signal | Rev 3 column | Controller connection |'
                      if gpio_columns else '| QMK column | Source column signal | Rev 3 column | J2 pin |',
                      '|---:|---:|---:|---:|']
            for logical, source_col in enumerate(record['source_cols']):
                native = record['cols'][logical]
                if 0 <= native < record['scanned_cols']:
                    pin = native + 13
                    if gpio_columns:
                        pin = f'J2.{native+7}' if native < 4 else f'J2.{native+9}'
                    lines.append(f'| {logical} | {source_col} | COL{native} | {pin} |')
            lines += ['', 'The row mapping and QMK/Vial key coordinates are unchanged.', '']
    if record.get('direct_key'):
        lines += ['**Blue button:** connect one terminal to J2.30 / COL17 and the other to GND. '
                  'This target reserves GPIO27 as a pulled-up input and scans only the first 16 '
                  'PIO columns. The button reports QMK (8,12); it is not a ninth capacitive row. '
                  'LED0/1/2 and solenoid signals remain available.', '']
    if not record['original']:
        lines += ['**Model F Labs reproduction/legacy target, not an original IBM pinout.** '
                  'Its logical matrix cannot be substituted for the wiring of an original IBM beamspring.', '']
    if blocked:
        lines += ['**BLOCKED / UNVERIFIED PORT — this is not a complete soldering guide.**', '']
        lines += ['- '+reason for reason in record['reasons']]
        lines += ['', 'Any source relationships below describe only the declared signals. '
                  'They do not supply a missing adapter, extra sense inputs, or unsupported drive outputs.', '']
    else:
        lines += ['This table follows this target\'s source mapping. It does not establish '
                  'the original keyboard connector orientation, wire colours, supply requirements, '
                  'or that every row/column intersection has a physical key.', '']
    lines += ['```text', '                 +--------------------------+',
              'USB-C -----------|     LEYDEN JAR REV 3      |',
              '                 |          RP2040          |',
              '                 | J2  Matrix / LED pads    |',
              '                 | J3  Debug / I2C header    |',
              '                 | J4  Expansion header     |',
              '                 +--------------------------+', '',
              'J2 — MAIN CONNECTOR'.replace('—','-'),
              'Pin   Controller signal   Logical keyboard signal',
              '---   -----------------   -----------------------']
    for pin in range(1,34):
        if pin in (1,2,11,12):
            signal, target = 'GND', 'Matrix ground'
        elif 3 <= pin <= 10:
            signal, target = 'A_ROW'+str(pin-3), assignment(record,'row',pin-3)
            if gpio_columns and pin >= 7:
                target = f'Digital drive COL{pin-7} after U6 modification'
        elif 13 <= pin <= 30:
            signal, target = 'COL'+str(pin-13), assignment(record,'column',pin-13)
            if gpio_columns:
                target = assignment(record,'column',pin-9) if pin <= 28 else 'Unused in this adapter'
        else:
            signal, target = 'LED'+str(pin-31), 'I/O-expander LED output; see note below'
            if record.get('io_expander_input_mask', 0) & (1 << (6 - (pin - 31))):
                target = 'High-impedance input in this candidate'
        lines.append(f'{pin:02d}    {signal:<17}   {target}')
    for title, pins in (('J3 - DEBUG / I2C HEADER',J3),('J4 - EXPANSION / SOLENOID DRIVER HEADER',J4)):
        lines += ['',title,'Pin   Signal','---   ------']
        lines += [f'{pin:02d}    {signal}' for pin,signal in enumerate(pins,1)]
    lines += ['```', '',
              'J2 sense signals are analog inputs to the controller circuitry; '
              'do not bypass the comparators by wiring the keyboard directly to RP2040 GPIOs. '
              'J4 solenoid enable/pulse are driver-control signals, not bare-coil connections.', '']
    if blocked and record['rows'] is not None:
        missing = []
        for kind, mapping, count, limit in (('row',record['rows'],record['matrix']['rows'],8),
                                            ('column',record['cols'],record['matrix']['cols'],18)):
            for logical in range(count):
                physical = mapping[logical] if logical < len(mapping) else -1
                if not 0 <= physical < limit:
                    text = f'controller {kind} {physical}' if 0 <= physical < 255 else 'no valid controller input'
                    missing.append(f'QMK {kind} {logical}: {text}; no assigned Rev 3 J2 pin.')
        if missing:
            lines += ['Source signals without a Rev 3 connection:', '', '```text', *missing, '```', '']
    lines += ['**LED labels versus firmware names:** the current shared driver assigns '
              'firmware LED0 to IO4, while the Rev 3 schematic labels that output LED2; '
              'firmware LED2 reaches the PCB pad LED0. Firmware LED1 matches PCB LED1. '
              'The default lock handler therefore sends Num Lock to J2.33, Caps Lock '
              'to J2.32 and Scroll Lock to J2.31. A keymap\'s `led_update_user()` may '
              'override those functions or add layer indications. No original keyboard '
              'light-board voltage/pinout is assumed.', '']
    if gpio_columns:
        lines += ['On this modified target, J2.31/32 remain high-impedance. '
                  'J2.33 retains the native Num Lock output.', '']
    if record['folder'].name == 'fat' and record['original']:
        lines += ['For the current FAT `vial_alec` lock-light signal mapping:', '',
                  '```text', 'AT A1 (Caps Lock)   -> J2.32 / PCB LED1',
                  'AT A2 (Scroll Lock) -> J2.31 / PCB LED0',
                  'AT A3              -> not connected',
                  'AT B1 (Num Lock)    -> J2.33 / PCB LED2',
                  'AT B2 (GND)         -> GND',
                  'AT B3 (VCC)         -> supply connection not electrically verified', '```', '',
                  'Trace the wires from the AT light-board labels: its cable rearranges '
                  'the connector order. See the [original AT installation manual, pages 13–14]'
                  '(https://static.wongcornall.com/ibm-capsense-usb/installation_model_f.pdf).', '']
    relative_common = '../../common.c' if record['original'] else '../common.c'
    if record['original']:
        original_config = ROOT / record['original'] / 'universal/config.h'
        original_link = Path(os.path.relpath(original_config, record['folder'])).as_posix()
        lines += [f'Original controller-family source: [AVR configuration]({original_link}) '
                  'and [port inventory](../ports.json). Blocked references retain source '
                  'relationships even where a complete native adapter mapping is unavailable.', '']
    if gpio_columns:
        verification = ('The installed modified 3276 passed a complete 75-key physical sweep. '
                        'Other controller assemblies require their own physical verification.')
    elif record['folder'].name == 'beamal':
        verification = ('All 88 keys in the installed shorter-side layout were observed in the '
                        'physical sweep and replayed through the corrected native matrix code. '
                        'The six optional extra positions require a fully populated assembly to test.')
    else:
        verification = ('Only the FAT has the earlier full physical row/key sweep; F122 has firmware/idle checks. '
                        'Other ports require physical matrix verification.')
    lines += [f'Mapping sources: [config.h](config.h) and [common.c]({relative_common}). '
              f'Switch technology: **{record["technology"].replace("_"," ")}**. ' +
              verification, '',
              'Controller wiring source: '
              '`mymakercorner/Leyden_Jar:pcb/leyden_jar_controller/Leyden_Jar.kicad_sch '
              '@ e01dea552d0a`, J2/J3/J4 and the A_ROW/COL/LED/SWD/I2C/expansion nets. '
              f'[Rev 3 schematic]({SCHEMATIC}).', '',
              'This section is maintained by `ibm/tools/update_pin_readmes.py`; '
              'run it after changing a port mapping. '+END]
    return '\n'.join(lines)+'\n'


def document(record):
    path = record['folder']/'README.md'
    original = path.read_text() if path.exists() else '# '+record['name']+'\n'
    block = render(record)
    if BEGIN in original:
        start = original.index(BEGIN)
        end = original.index(END,start)+len(END)
        updated = original[:start]+block.rstrip()+original[end:]
        return path,updated.rstrip()+'\n'
    return path,original.rstrip()+'\n\n'+block


def update(check=False):
    records=inventory();stale=[]
    for record in records:
        path,text=document(record)
        if not path.exists() or path.read_text()!=text:
            stale.append(path)
            if not check:path.write_text(text)
    if check and stale:
        raise SystemExit('Stale pin references:\n'+'\n'.join(str(p.relative_to(ROOT)) for p in stale))
    print(('Checked' if check else 'Updated')+f' {len(records)} board pin references '
          f'({sum(bool(r["original"]) for r in records)} original IBM ports; '
          f'{sum(bool(r["reasons"]) for r in records)} blocked/legacy references explicitly marked).')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true',help='Report stale documentation without writing')
    update(parser.parse_args().check)
