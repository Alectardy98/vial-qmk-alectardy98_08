#!/usr/bin/env python3
"""Regression for the released-pad activations captured on Alec's F122."""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest
import test_calibration as native

QUANTUM = native.QUANTUM.replace('/ibm/fat/', '/ibm/f122/')
DRIVER = native.DRIVER[:native.DRIVER.index('int main(void)')]+r'''
int main(void) {
 leyden_jar_init();leyden_jar_calibrate();
 matrix_row_t matrix[8]={0};leyden_jar_logical_matrix_scan(matrix);
#ifdef PERSONAL
 assert(NB_CAL_BINS==1 && s_bin_activation_offsets[0]==16);
 no_keys();unsigned tested=0;
 for(unsigned r=0;r<8;r++)for(unsigned c=0;c<16;c++){
  unsigned pr=s_matrixToControllerRow[r],pc=s_matrixToControllerCol[c];
  if(!leyden_jar_pad_is_active(pr,pc))continue;
  assert(s_dac_thresholds[0]>levels[pc][pr]);
  uint16_t old=levels[pc][pr];levels[pc][pr]=s_dac_thresholds[0]+20;
  leyden_jar_logical_matrix_scan(matrix);
  for(unsigned row=0;row<8;row++)assert(matrix[row]==(row==r?(1u<<c):0));
  levels[pc][pr]=old;no_keys();tested++;
 }
 assert(tested==127);
 assert(leyden_jar_save_calibration_offset(0,18));leyden_jar_calibrate();
 assert(s_dac_thresholds[0]==s_dac_ref_level[0]+18);
 assert(leyden_jar_restore_calibration_defaults());
 assert(s_dac_thresholds[0]==s_dac_ref_level[0]+16);no_keys();
 puts("Personal F122: all 127 declared pads pass, no released presses; +16 defaults and persistence verified.");
#else
 assert(s_bin_activation_offsets[0]==10);
 unsigned pressed=0;for(int r=0;r<8;r++)pressed+=__builtin_popcount(matrix[r]);
 assert(pressed>0);printf("Unchanged standard +10 configuration reproduces %u released-pad activations.\n",pressed);
#endif
}
'''

class F122Tests(unittest.TestCase):
 def run_case(self, personal):
  values=json.loads((Path(__file__).parent/'fixtures/f122_released.json').read_text())
  array='{'+','.join('{'+','.join(str(values.get(f'{r},{c}',349)+1) for r in range(8))+'}' for c in range(18))+'}'
  with tempfile.TemporaryDirectory() as tmp:
   path=Path(tmp);(path/'quantum.h').write_text(QUANTUM);(path/'eeconfig.h').write_text(native.EEPROM);(path/'haptic.h').write_text('')
   (path/'test.c').write_text(DRIVER.replace('INITIAL',array))
   cmd=['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(path/'test.c'),'-o',str(path/'test')]
   if personal:cmd.insert(1,'-DPERSONAL')
   subprocess.run(cmd,check=True);subprocess.run([str(path/'test')],check=True)
 def test_personal_released_margin(self):self.run_case(True)
 def test_standard_remains_unchanged(self):self.run_case(False)
if __name__=='__main__':unittest.main(verbosity=2)
