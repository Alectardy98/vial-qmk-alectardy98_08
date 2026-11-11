#!/usr/bin/env python3
"""Replay measured same-column coupling through native calibration/scanning."""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest
import test_calibration as native
import test_3178_direct as direct

QUANTUM=direct.QUANTUM+'\n#include "'+str(native.BOARD/'ibm/3178/keymaps/vial_alec/config.h')+'"\n'
QUANTUM+='''
#ifdef OLD_MARGINS
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {10,10,10,10,10,10,10,17}
#endif
'''
DRIVER=native.DRIVER[:native.DRIVER.index('static void no_keys(void)')]+r'''
static const uint16_t held[3][18][8] = HELD;
int main(void) {
 leyden_jar_init();leyden_jar_calibrate();
#ifdef RECORDED_MAP
 /* Preserve the measured boot's bin membership. A later released sweep can
  * sort equal/near-equal pads into different groups; it is not that boot.
  */
 const uint8_t recorded_map[18][8] = BIN_MAP;
 memcpy(s_bin_map,recorded_map,sizeof s_bin_map);
 const uint16_t recorded_thresholds[8] = TRIGGERS;
 for(unsigned b=0;b<8;b++) {
#ifdef OLD_MARGINS
  s_dac_thresholds[b]=recorded_thresholds[b];
#else
  s_dac_thresholds[b]=recorded_thresholds[b]+8;
#endif
 }
#endif
 static const unsigned targets[3][2]={{3,7},{2,6},{5,5}};
 static const unsigned ghosts[3][2]={{4,7},{1,6},{6,5}};
 for(unsigned test=0;test<3;test++) {
  memcpy(levels,held[test],sizeof levels);
  matrix_row_t m[9]={0};leyden_jar_logical_matrix_scan(m);
  unsigned row=targets[test][0],col=targets[test][1];
  assert(m[row]&(1u<<col));
#ifdef OLD_MARGINS
  assert(m[ghosts[test][0]]&(1u<<ghosts[test][1]));
#else
  for(unsigned r=0;r<9;r++)assert(m[r]==(r==row?(1u<<col):0));
  /* Stay held, then return to measured released levels. */
  for(unsigned n=0;n<20;n++) {
   leyden_jar_logical_matrix_scan(m);
   for(unsigned r=0;r<9;r++)assert(m[r]==(r==row?(1u<<col):0));
  }
#endif
 }
#ifdef OLD_MARGINS
 puts("Old margins reproduce false F, 8 and M during Y, U and L holds.");
#else
 puts("Captured Y/U/L coupling: only intended keys activate and stay active.");
#endif
}
'''

class CouplingTests(unittest.TestCase):
 def run_case(self,old,recorded=False):
  d=json.loads((Path(__file__).parent/'fixtures/3178_coupling.json').read_text())
  def array(v):return '{'+','.join('{'+','.join(str(v.get(f'{r},{c}',349)+1) for r in range(8))+'}' for c in range(18))+'}'
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp);(p/'quantum.h').write_text(QUANTUM);(p/'eeconfig.h').write_text(native.EEPROM);(p/'haptic.h').write_text('')
   source=DRIVER.replace('INITIAL',array(d['baseline'])).replace('HELD','{'+','.join(array(c['levels']) for c in d['cases'])+'}')
   bins='{'+','.join('{'+','.join(str(d['recorded_bin_map'].get(f'{r},{c}',255)) for r in range(8))+'}' for c in range(18))+'}'
   source=source.replace('BIN_MAP',bins).replace('TRIGGERS','{'+','.join(map(str,d['recorded_thresholds']))+'}')
   (p/'test.c').write_text(source)
   cmd=['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(p/'test.c'),'-o',str(p/'test')]
   if old:cmd.insert(1,'-DOLD_MARGINS')
   if recorded:cmd.insert(1,'-DRECORDED_MAP')
   subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
 def test_old_margins_reproduce_ghosts(self):self.run_case(True,recorded=True)
 def test_measured_coupling_rejected(self):self.run_case(False)
 def test_recorded_boot_with_new_margins(self):self.run_case(False,recorded=True)
if __name__=='__main__':unittest.main(verbosity=2)
