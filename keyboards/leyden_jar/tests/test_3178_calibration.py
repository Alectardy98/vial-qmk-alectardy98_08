#!/usr/bin/env python3
"""Replay real released 3178 levels; reproduce the old single-bin fault."""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest
import test_calibration as native
import test_3178_direct as direct

QUANTUM=direct.QUANTUM+'\n#include "KEYMAP_CONFIG"\n'
QUANTUM+='''
#ifdef LEGACY
#undef NB_CAL_BINS
#define NB_CAL_BINS 1
#undef NB_CUSTOM_CAL_BINS
#define NB_CUSTOM_CAL_BINS 0
#undef CUSTOM_CAL_BIN_KEYS
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {10}
#endif
'''
DRIVER=native.DRIVER[:native.DRIVER.index('static void no_keys(void)')]+r'''
static void clear_matrix(void) {
 matrix_row_t m[9]={0};leyden_jar_logical_matrix_scan(m);
 for(unsigned r=0;r<9;r++)assert(m[r]==0);
}
int main(void) {
 leyden_jar_init();leyden_jar_calibrate();matrix_row_t m[9]={0};leyden_jar_logical_matrix_scan(m);
#ifdef LEGACY
 unsigned count=0;for(unsigned r=0;r<9;r++)count+=__builtin_popcount(m[r]);
 assert(count>0);printf("Old one-bin calibration reproduces %u false positions.\n",count);
#else
 assert(NB_CAL_BINS==8 && s_bin_map[2][6]==0 && s_bin_map[9][6]==1);
 clear_matrix();unsigned count=0;
 for(unsigned r=0;r<8;r++)for(unsigned c=0;c<13;c++) {
  unsigned pr=s_matrixToControllerRow[r],pc=s_matrixToControllerCol[c];
  if(!leyden_jar_pad_is_active(pr,pc))continue;
  unsigned b=s_bin_map[pc][pr];assert(s_dac_thresholds[b]>levels[pc][pr]);
  uint16_t old=levels[pc][pr];levels[pc][pr]=s_dac_thresholds[b]+20;
  leyden_jar_logical_matrix_scan(m);
  for(unsigned row=0;row<9;row++)assert(m[row]==(row==r?(1u<<c):0));
  levels[pc][pr]=old;clear_matrix();count++;
 }
 assert(count==87);
 direct_high=false;leyden_jar_logical_matrix_scan(m);assert(m[8]==(1u<<12));
 direct_high=true;clear_matrix();
 assert(leyden_jar_save_calibration_offset(7,18));leyden_jar_calibrate();
 assert(s_dac_thresholds[7]==s_dac_ref_level[7]+18);
 assert(leyden_jar_restore_calibration_defaults());clear_matrix();
 puts("3178: released clear, 87 simulated capacitive keys + blue switch, persistence/defaults passed.");
#endif
}
'''

class CalibrationTests(unittest.TestCase):
 def run_case(self,keymap,legacy=False):
  sweeps=json.loads((Path(__file__).parent/'fixtures/3178_released.json').read_text())
  for values in sweeps:
   array='{'+','.join('{'+','.join(str(values.get(f'{r},{c}',349)+1) for r in range(8))+'}' for c in range(18))+'}'
   with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'quantum.h').write_text(QUANTUM.replace('KEYMAP_CONFIG',str(native.BOARD/'ibm/3178/keymaps'/keymap/'config.h')))
    (p/'eeconfig.h').write_text(native.EEPROM);(p/'haptic.h').write_text('');(p/'test.c').write_text(DRIVER.replace('INITIAL',array))
    cmd=['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(p/'test.c'),'-o',str(p/'test')]
    if legacy:cmd.insert(1,'-DLEGACY')
    subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
 def test_previous_calibration(self):self.run_case('vial',legacy=True)
 def test_vial(self):self.run_case('vial')
 def test_vial_alec(self):self.run_case('vial_alec')

if __name__=='__main__':unittest.main(verbosity=2)
