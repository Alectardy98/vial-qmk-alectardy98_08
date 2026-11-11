#!/usr/bin/env python3
"""Host-test native calibration + EEPROM preference handling with ASan/UBSan.

Run: python3 keyboards/leyden_jar/tests/test_calibration.py
The fixture is a released-key capture from Alec's modified FAT; it is not a
calibration specification for other Model F AT assemblies.
"""
from pathlib import Path
import json
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BOARD = ROOT / 'keyboards/leyden_jar'
QUANTUM = '''#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
typedef uint16_t matrix_row_t;
#define MATRIX_ROWS 8
#define MATRIX_COLS 16
#include "ROOT_CONFIG"
#include "BOARD_CONFIG"
#ifdef PERSONAL
#include "PERSONAL_CONFIG"
#endif
#ifdef BEAMTEST
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {-30}
#define BEAMSPRING_KEYBOARD
#endif
static void wait_us(unsigned n) {(void)n;}
'''.replace('ROOT_CONFIG', str(BOARD/'config.h')).replace('BOARD_CONFIG', str(BOARD/'ibm/fat/config.h')).replace('PERSONAL_CONFIG', str(BOARD/'ibm/fat/keymaps/vial_alec/config.h'))
EEPROM = '''#pragma once
uint32_t eeconfig_read_kb_datablock(void *data, uint32_t offset, uint32_t length);
uint32_t eeconfig_update_kb_datablock(const void *data, uint32_t offset, uint32_t length);
void eeconfig_init_user(void);
'''
DRIVER = r'''#include "quantum.h"
#include <assert.h>
#include <stdio.h>
static uint8_t eeprom[48];
static unsigned writes;
static bool reject_write, reject_dac;
void eeconfig_init_user(void){}
uint32_t eeconfig_read_kb_datablock(void *d, uint32_t o, uint32_t n) {assert(o+n<=48);memcpy(d,eeprom+o,n);return n;}
uint32_t eeconfig_update_kb_datablock(const void *d, uint32_t o, uint32_t n) {assert(o+n<=48);writes++;if(!reject_write)memcpy(eeprom+o,d,n);return n;}
static uint16_t threshold, levels[18][8] = INITIAL;
static uint8_t scanned[18];
int dac_init(void){return 1;}
int dac_write_val(uint16_t v){if(reject_dac)return 0;threshold=v;return 1;}
int io_expander_init(void){return 1;}
void io_expander_update_state(void){}
void pio_matrix_scan_init(bool extra){(void)extra;}
void pio_raw_scan(void){memset(scanned,0,sizeof scanned);for(int c=0;c<18;c++)for(int r=0;r<8;r++)if(threshold<levels[c][r])scanned[c]|=1<<r;}
const uint8_t *pio_get_scan_vals(void){return scanned;}
#include "STORE"
#include "COMMON"
static void no_keys(void) {
 matrix_row_t m[8]={0};leyden_jar_logical_matrix_scan(m);
 for(int r=0;r<8;r++)assert(m[r]==0);
}
int main(void){
 eeconfig_init_kb(); // First-boot reset runs before matrix initialization.
 leyden_jar_init();leyden_jar_calibrate();
#ifdef PERSONAL
 assert(NB_CAL_BINS==8 && NB_CUSTOM_CAL_BINS==2 && s_bin_map[2][2]==1);
 no_keys();assert(writes==0);
 unsigned tested=0;
 for(int r=0;r<8;r++)for(int c=0;c<16;c++){
  unsigned pr=s_matrixToControllerRow[r],pc=s_matrixToControllerCol[c];
  if(!leyden_jar_pad_is_active(pr,pc))continue;
  unsigned bin=s_bin_map[pc][pr];assert(bin<8);assert(s_dac_thresholds[bin]>levels[pc][pr]);
  uint16_t old=levels[pc][pr];levels[pc][pr]=s_dac_thresholds[bin]+20;
  matrix_row_t m[8]={0};leyden_jar_logical_matrix_scan(m);
  for(int row=0;row<8;row++)assert(m[row]==(row==r?(1u<<c):0));
  levels[pc][pr]=old;no_keys();tested++;
 }
 assert(tested==92);
 assert(s_dac_thresholds[1]==s_dac_ref_level[1]+12);
 assert(leyden_jar_save_calibration_offset(1,14));assert(writes==1);
 assert(leyden_jar_calibration_is_saved(1));assert(s_dac_thresholds[1]==s_dac_ref_level[1]+14);
 assert(leyden_jar_save_calibration_offset(1,14));assert(writes==1);
 /* Repeat startup calibration with a shifted released reference. */
 levels[2][2]+=2;leyden_jar_calibrate();
 assert(s_dac_thresholds[1]==s_dac_ref_level[1]+14 && writes==1);
 assert(!leyden_jar_save_calibration_offset(1,0));
 assert(!leyden_jar_save_calibration_offset(1,-1));
 assert(!leyden_jar_save_calibration_offset(1,1023));
 assert(!leyden_jar_save_calibration_offset(8,12));assert(writes==1);
 uint16_t previous=s_dac_thresholds[1];
 reject_dac=true;assert(!leyden_jar_save_calibration_offset(1,15));reject_dac=false;
 assert(s_dac_thresholds[1]==previous && writes==1);
 reject_write=true;assert(!leyden_jar_save_calibration_offset(1,15));reject_write=false;
 assert(s_dac_thresholds[1]==previous && leyden_jar_calibration_offset(1)==14);
 assert(leyden_jar_restore_calibration_defaults());
 for(int bin=0;bin<8;bin++){assert(!leyden_jar_calibration_is_saved(bin));assert(s_dac_thresholds[bin]==s_dac_ref_level[bin]+s_bin_activation_offsets[bin]);}
 leyden_jar_calibrate();assert(s_dac_thresholds[1]==s_dac_ref_level[1]+12);no_keys();
 /* Damage/torn record and incompatible firmware configuration are ignored. */
 assert(leyden_jar_save_calibration_offset(1,14));eeprom[18]^=1;
 leyden_jar_calibrate();assert(!leyden_jar_calibration_is_saved(1));
 assert(s_dac_thresholds[1]==s_dac_ref_level[1]+12);
 assert(leyden_jar_save_calibration_offset(1,14));
 leyden_jar_calibration_store_load(calibration_signature()+1,8,s_bin_activation_offsets);
 assert(!leyden_jar_calibration_is_saved(1));
 memset(eeprom,0,sizeof(eeprom));eeconfig_init_kb();
 assert(!leyden_jar_calibration_is_saved(1));
 assert(s_dac_thresholds[1]==s_dac_ref_level[1]+12);
 leyden_jar_calibrate();
 puts("92 native press/release paths; persistent relative offsets, reset, invalid values, torn/incompatible records and failures passed.");
#elif defined(BEAMTEST)
 assert(NB_CAL_BINS==1 && s_bin_activation_offsets[0]==-30);
 no_keys();assert(writes==0);
 assert(!leyden_jar_save_calibration_offset(0,1));
 assert(leyden_jar_save_calibration_offset(0,-20));
 leyden_jar_calibrate();
 assert(leyden_jar_calibration_is_saved(0));
 assert(s_dac_thresholds[0]==s_dac_ref_level[0]-20);
 assert(leyden_jar_restore_calibration_defaults());
 assert(s_dac_thresholds[0]==s_dac_ref_level[0]-30);
 puts("Beamspring negative offsets persist and restore with the correct polarity.");
#else
 assert(NB_CAL_BINS==1 && s_bin_activation_offsets[0]==10);
 puts("Standard vial retains its native single-bin calibration default.");
#endif
 for(int c=0;c<18;c++)for(int r=0;r<8;r++)levels[c][r]=350;
 leyden_jar_calibrate();assert(!leyden_jar_is_controller_soldered());
 assert(!leyden_jar_save_calibration_offset(0,leyden_jar_is_beamspring()?-20:12));
 assert(leyden_jar_restore_calibration_defaults());
 for(int b=0;b<NB_CAL_BINS;b++)assert(s_dac_thresholds[b]==(leyden_jar_is_beamspring()?300:1023));
}
'''.replace('STORE', str(BOARD/'calibration_store.c')).replace('COMMON', str(BOARD/'common.c'))

class CalibrationTests(unittest.TestCase):
    def run_case(self, personal, beamspring=False):
        values=json.loads((Path(__file__).parent/'fixtures/fat_released.json').read_text())
        array='{'+','.join('{'+','.join(str(values[f'{r},{c}']+1) for r in range(8))+'}' for c in range(18))+'}'
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)
            (path/'quantum.h').write_text(QUANTUM)
            (path/'eeconfig.h').write_text(EEPROM)
            (path/'haptic.h').write_text('')
            (path/'test.c').write_text(DRIVER.replace('INITIAL',array))
            cmd=['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+temp,str(path/'test.c'),'-o',str(path/'test')]
            if personal:cmd.insert(1,'-DPERSONAL')
            if beamspring:cmd.insert(1,'-DBEAMTEST')
            subprocess.run(cmd,check=True)
            subprocess.run([str(path/'test')],check=True)

    def test_persistent_personal_calibration(self):self.run_case(True)
    def test_standard_calibration_unchanged(self):self.run_case(False)
    def test_beamspring_offsets(self):self.run_case(False, beamspring=True)

if __name__=='__main__':unittest.main(verbosity=2)
