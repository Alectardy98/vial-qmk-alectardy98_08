#!/usr/bin/env python3
"""Replay BEAMal's measured sweep through the native C matrix code."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import test_calibration as native

BOARD = native.BOARD
QUANTUM = native.QUANTUM.replace('#define MATRIX_COLS 16', '#define MATRIX_COLS 12')
QUANTUM = QUANTUM.replace(str(BOARD/'ibm/fat/config.h'), str(BOARD/'ibm/beamal/config.h'))
QUANTUM = QUANTUM.replace(str(BOARD/'ibm/fat/keymaps/vial_alec/config.h'),
                          str(BOARD/'ibm/beamal/keymaps/vial_alec/config.h'))
QUANTUM += '''
#ifdef LEGACY
#undef LEYDEN_JAR_MATRIX_COLS
#define LEYDEN_JAR_MATRIX_COLS {0,1,2,3,4,5,6,7,8,9,10,11,255,255,255,255,255,255}
#undef LEYDEN_JAR_ACTIVE_PADS
#define LEYDEN_JAR_ACTIVE_PADS {255,255,255,246,255,255,255,255,255,255,255,255,0,0,0,0,0,0}
#endif
'''
DRIVER = native.DRIVER[:native.DRIVER.index('static void no_keys(void)')]
DRIVER += r'''
typedef struct { unsigned row,col,logical_row,logical_col,held; } CapturedKey;
static const CapturedKey captured[] = CAPTURED_KEYS;
static void expect_clear(void) {
 matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
 for(unsigned r=0;r<8;r++)assert(matrix[r]==0);
}
int main(void) {
 leyden_jar_init();leyden_jar_calibrate();
 assert(NB_CAL_BINS==1 && s_bin_activation_offsets[0]==-30);
 unsigned wrong=0;
#ifndef LEGACY
 expect_clear();
 for(unsigned col=0;col<12;col++)assert(s_matrixToControllerCol[col]==11-col);
 // Original layout holes move with the mapping; the Alt and D pads stay live.
 assert(!leyden_jar_pad_is_active(0,8) && !leyden_jar_pad_is_active(3,8));
 assert(leyden_jar_pad_is_active(0,3) && leyden_jar_pad_is_active(3,3));
#endif
 for(unsigned i=0;i<sizeof captured/sizeof captured[0];i++) {
  const CapturedKey *key=&captured[i];
  uint16_t old=levels[key->col][key->row];
  levels[key->col][key->row]=key->held+1;
  matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
  bool correct=true;
  for(unsigned r=0;r<8;r++)correct &= matrix[r]==(r==key->logical_row?(1u<<key->logical_col):0);
  if(!correct)wrong++;
#ifndef LEGACY
  assert(correct);
#endif
  levels[key->col][key->row]=old;
#ifndef LEGACY
  expect_clear();
#endif
 }
 assert(writes==0); // Reading/scanning/calibrating never persists new settings.
#ifdef LEGACY
 assert(wrong==88);
 puts("Old identity map misreports all 88 captured physical keys.");
#else
 assert(wrong==0);
 // Simulate a chord spanning both ends and a previously masked intersection.
 levels[0][6]=377;levels[11][7]=377;levels[3][0]=375;
 matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
 for(unsigned r=0;r<8;r++)assert(matrix[r]==(r==6?(1u<<11):r==7?1u:r==0?(1u<<8):0));
 puts("88 captured press/release paths, corrected holes and cross-column chord passed.");
#endif
}
'''


class BeamalMappingTests(unittest.TestCase):
    def run_capture(self, legacy=False, personal=False):
        fixture = json.loads((Path(__file__).parent/'fixtures/beamal_sweep_levels.json').read_text())
        self.assertEqual(len(fixture['keys']), 88)
        released = dict(fixture['released'])
        if not personal and not legacy:
            # The standard map supports the full 94-position layout union.
            # Model switches installed at the six optional bottom side pads;
            # these are not claimed as physically tested on Alec's assembly.
            for row, col in ((0,0),(0,1),(0,9),(0,10),(0,11),(1,9)):
                released[f'{row},{col}'] = 420
        initial = '{'+','.join('{'+','.join(str(released[f'{r},{c}']+1)
                                          for r in range(8))+'}' for c in range(18))+'}'
        keys = '{'+','.join('{'+','.join(map(str,[*key['physical'],*key['logical'],key['pressed']]))+'}'
                            for key in fixture['keys'])+'}'
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'quantum.h').write_text(QUANTUM)
            (root/'haptic.h').write_text('')
            (root/'eeconfig.h').write_text(native.EEPROM)
            (root/'test.c').write_text(DRIVER.replace('INITIAL',initial).replace('CAPTURED_KEYS',keys))
            subprocess.run(['cc','-std=gnu11','-g','-fsanitize=address,undefined',
                            *(['-DLEGACY'] if legacy else []),
                            *(['-DPERSONAL'] if personal else []),
                            '-I'+tmp,str(root/'test.c'),'-o',str(root/'test')],check=True)
            subprocess.run([str(root/'test')],check=True)

    def test_previous_mapping_reproduces_failure(self):
        self.run_capture(legacy=True)

    def test_full_layout_translation(self):
        self.run_capture()

    def test_personal_population_and_captured_levels(self):
        self.run_capture(personal=True)


if __name__=='__main__':
    unittest.main(verbosity=2)
