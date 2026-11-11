#!/usr/bin/env python3
"""Replay the FXTAL released capture against the actual native calibration code."""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest
import test_calibration as native

BOARD = native.BOARD / 'ibm/fxtal'
QUANTUM = native.QUANTUM.replace('#define MATRIX_COLS 16', '#define MATRIX_COLS 12')
QUANTUM = QUANTUM.replace(str(native.BOARD / 'ibm/fat/config.h'), str(BOARD / 'config.h'))
QUANTUM = QUANTUM.replace('#ifdef PERSONAL\n#include "'+str(native.BOARD/'ibm/fat/keymaps/vial_alec/config.h')+'"\n#endif', '''#include "KEYMAP_CONFIG"
#ifdef LEGACY
#undef NB_CAL_BINS
#define NB_CAL_BINS 1
#undef NB_CUSTOM_CAL_BINS
#define NB_CUSTOM_CAL_BINS 0
#undef CUSTOM_CAL_BIN_KEYS
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {10}
#endif''')
DRIVER = native.DRIVER[:native.DRIVER.index('int main(void)')] + r'''
int main(void) {
 eeconfig_init_kb();leyden_jar_init();leyden_jar_calibrate();
 matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
#ifdef LEGACY
 assert(NB_CAL_BINS==1 && s_dac_ref_level[0]==377 && s_dac_thresholds[0]==387);
 for(unsigned r=0;r<8;r++) assert(matrix[r]==(r==4?1u:0));
 puts("Original single group reproduces the stuck QMK (4,0) key.");
#else
 assert(NB_CAL_BINS==2 && NB_CUSTOM_CAL_BINS==1);
 assert(s_bin_map[0][3]==0 && s_dac_ref_level[0]==392 && s_dac_thresholds[0]==402);
 assert(s_dac_ref_level[1]==377 && s_dac_thresholds[1]==387);
 no_keys();unsigned count=0;
 for(unsigned r=0;r<8;r++)for(unsigned c=0;c<12;c++) {
  unsigned pr=s_matrixToControllerRow[r],pc=s_matrixToControllerCol[c];
  unsigned bin=s_bin_map[pc][pr];assert(bin==(r==4 && c==0?0:1));
  assert(s_dac_thresholds[bin]>levels[pc][pr]);
  uint16_t old=levels[pc][pr];levels[pc][pr]=s_dac_thresholds[bin]+20;
  leyden_jar_logical_matrix_scan(matrix);
  for(unsigned row=0;row<8;row++)assert(matrix[row]==(row==r?(1u<<c):0));
  levels[pc][pr]=old;no_keys();count++;
 }
 assert(count==96);
 assert(leyden_jar_save_calibration_offset(0,12));
 leyden_jar_calibrate();assert(s_dac_thresholds[0]==404 && s_dac_thresholds[1]==387);
 assert(leyden_jar_restore_calibration_defaults());no_keys();
 assert(s_dac_thresholds[0]==402 && s_dac_thresholds[1]==387);
 /* Existing calibration signatures may not migrate across bin layouts. */
 assert(leyden_jar_save_calibration_offset(0,12));
 leyden_jar_calibration_store_load(calibration_signature()^1,NB_CAL_BINS,s_bin_activation_offsets);
 assert(!leyden_jar_calibration_is_saved(0));
 puts("Both FXTAL groups: released clear; 96 simulated press/release paths and persistent defaults passed.");
#endif
}
'''


class FxtalCalibrationTests(unittest.TestCase):
    def run_case(self, keymap, legacy=False):
        fixture = json.loads((Path(__file__).parent/'fixtures/fxtal_released.json').read_text())
        array = '{'+','.join('{'+','.join(str(fixture[f'{r},{c}']+1) for r in range(8))+'}' for c in range(18))+'}'
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path/'quantum.h').write_text(QUANTUM.replace('KEYMAP_CONFIG',str(BOARD/'keymaps'/keymap/'config.h')))
            (path/'eeconfig.h').write_text(native.EEPROM)
            (path/'haptic.h').write_text('')
            (path/'test.c').write_text(DRIVER.replace('INITIAL',array))
            cmd = ['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(path/'test.c'),'-o',str(path/'test')]
            if legacy: cmd.insert(1,'-DLEGACY')
            subprocess.run(cmd,check=True)
            subprocess.run([str(path/'test')],check=True)

    def test_original_fault(self): self.run_case('vial',legacy=True)
    def test_vial(self): self.run_case('vial')
    def test_vial_alec(self): self.run_case('vial_alec')


if __name__=='__main__': unittest.main(verbosity=2)
