#!/usr/bin/env python3
"""Check the real UF2/ELF and EEPROM ABI; no connected hardware access."""
import hashlib
import json
import lzma
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile
import unittest

BOARD = Path(__file__).resolve().parents[1]
ROOT = BOARD.parents[2]


class VirgoUF2Tests(unittest.TestCase):
    def test_common_original_unchanged_and_narrow_flash_correction(self):
        manifest = json.loads((BOARD/'tests/source-baseline.json').read_text())
        corrections = json.loads((BOARD/'tests/flash-correction.json').read_text())
        for name, change in corrections.items():
            self.assertTrue(name.startswith('keyboards/alectardy98_ec/ec_virgo/'))
            self.assertEqual(manifest[name], change['before'])
        for name, digest in manifest.items():
            if name in corrections:
                digest = corrections[name]['after']
            with self.subTest(path=name):
                self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(), digest)

    def test_same_layout_keys_and_identity(self):
        base = json.loads((BOARD.parent/'ec_virgo/keyboard.json').read_text())
        variant = json.loads((BOARD/'keyboard.json').read_text())
        variant['bootloader'] = 'stm32-dfu'
        self.assertEqual(base, variant)
        for keymap in ('vial', 'vial_alec'):
            self.assertEqual((BOARD/'keymaps'/keymap/'vial.json').read_bytes(),
                             (BOARD.parent/'ec_virgo/keymaps'/keymap/'vial.json').read_bytes())

    def test_compiled_definitions_and_eeprom_layout(self):
        for keymap in ('vial', 'vial_alec'):
            with self.subTest(keymap=keymap):
                name = 'alectardy98_ec_ec_virgo_uf2_'+keymap
                objdir = ROOT/'.build'/('obj_'+name)
                source = (objdir/'src/vial_generated_keyboard_definition.h').read_text()
                packed = bytes(int(x, 16) for x in re.findall(r'0x([a-fA-F0-9]{2})', source))
                self.assertEqual(json.loads(lzma.decompress(packed)), json.loads((BOARD/'keymaps'/keymap/'vial.json').read_text()))
                with tempfile.TemporaryDirectory(prefix='virgo-uf2-layout-') as tmp:
                    tmp = Path(tmp)
                    code, obj, data = tmp/'layout.c', tmp/'layout.o', tmp/'layout.bin'
                    code.write_text('''#include "quantum/nvm/eeprom/nvm_dynamic_keymap.c"
#include "ec_switch_matrix.h"
const uint32_t layout[] __attribute__((used,section(".ec42_layout")))={
sizeof(eeprom_ec_config_t),EECONFIG_BASE_SIZE,EECONFIG_SIZE,DYNAMIC_KEYMAP_EEPROM_ADDR,
DYNAMIC_KEYMAP_LAYER_COUNT*MATRIX_ROWS*MATRIX_COLS*2,DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR,
DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE,TOTAL_EEPROM_BYTE_COUNT,WEAR_LEVELING_EFL_FIRST_SECTOR,
A98_STORAGE_START,A98_STORAGE_END,WEAR_LEVELING_EFL_FLASH_SIZE};
''')
                    flags = shlex.split((objdir/'cflags.txt').read_text())
                    subprocess.run(['arm-none-eabi-gcc', *flags, '-fno-lto', '-c', str(code), '-o', str(obj)], cwd=ROOT, check=True)
                    subprocess.run(['arm-none-eabi-objcopy', '--dump-section', '.ec42_layout='+str(data), str(obj)], check=True)
                    self.assertEqual(struct.unpack('<12I', data.read_bytes()),
                                     (341, 37, 378, 382, 864, 2438, 1658, 4096, 2, 0x8000, 0xc000, 262144))
                nm = subprocess.check_output(['arm-none-eabi-nm', str(ROOT/'.build'/(name+'.elf'))], text=True)
                self.assertNotIn('ec_apply_calibration_defaults', nm)
                for symbol in ('a98_hw_program', 'efl_lld_start_erase_sector'):
                    self.assertIn(symbol, nm)


if __name__ == '__main__':
    unittest.main(verbosity=2)
