#!/usr/bin/env python3
"""Replay the installed 3276's captured held/released bounds in native C."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import test_calibration as native

BOARD = native.BOARD
QUANTUM = native.QUANTUM.replace('typedef uint16_t matrix_row_t;', 'typedef uint32_t matrix_row_t;')
QUANTUM = QUANTUM.replace('#define MATRIX_ROWS 8', '#define MATRIX_ROWS 4').replace('#define MATRIX_COLS 16', '#define MATRIX_COLS 20')
QUANTUM = QUANTUM.replace(str(BOARD/'ibm/fat/config.h'), str(BOARD/'ibm/3276_3278_75key/config.h'))
QUANTUM += '''
#ifdef LEGACY
#undef NB_CAL_BINS
#define NB_CAL_BINS 1
#undef NB_CUSTOM_CAL_BINS
#define NB_CUSTOM_CAL_BINS 0
#undef CUSTOM_CAL_BIN_KEYS
#undef ACTIVATION_OFFSETS
#define ACTIVATION_OFFSETS {-30}
#endif
'''
DRIVER = native.DRIVER[:native.DRIVER.index('static void no_keys(void)')]
DRIVER = DRIVER.replace('levels[18][8]', 'levels[20][4]').replace('scanned[18]', 'scanned[20]')
DRIVER = DRIVER.replace('c<18', 'c<20').replace('r<8', 'r<4')
DRIVER = DRIVER.replace('#include "'+str(BOARD/'common.c')+'"', '''
static bool scanner_ready=true;
bool pio_gpio_columns_ready(void){return scanner_ready;}
void pio_gpio_columns_status(uint8_t payload[28]){memset(payload,0,28);payload[0]=1;payload[1]=scanner_ready;payload[23]=4;}
#include "'''+str(BOARD/'common.c')+'"')
DRIVER += r'''
#define VIA_ENABLE
#define id_get_keyboard_value 2
#define id_set_keyboard_value 3
#define id_unhandled 255
void reset_keyboard(void){abort();}
void soft_reset_keyboard(void){abort();}
void eeconfig_disable(void){abort();}
#include "PROTOCOL_SOURCE"
static void check_protocol(void) {
 uint8_t guarded[64];memset(guarded,0xa5,sizeof guarded);
 uint8_t *request=guarded+16;
 memset(request,0,32);request[0]=2;request[1]=id_leyden_jar_matrix_mapping;request[2]=0xc0;request[3]=0x21;
 raw_hid_receive_kb(request,32);assert(request[0]==2 && request[4]==MATRIX_LAYOUT_IS_XWHATSIT);
 for(unsigned col=0;col<20;col++)assert(request[5+col]==19-col);
 for(unsigned row=0;row<4;row++)assert(request[25+row]==3-row);
 request[1]=id_leyden_jar_details;raw_hid_receive_kb(request,32);
 assert(request[4]==4 && request[5]==20 && request[6]==4 && request[7]==20);
 assert(request[8]==1 && request[9]==NB_CAL_BINS && request[10]==2);
 assert(request[14]==31 && request[15]==32 && request[16]==4 && request[17]==4 && request[18]==7);
 request[1]=id_leyden_jar_scan_status;raw_hid_receive_kb(request,32);
 assert(request[0]==2 && request[4]==1 && request[27]==4);
 request[0]=3;request[1]=0x96;raw_hid_receive_kb(request,32);
 assert(request[0]==255); // Retired pin control cannot change an output.
 for(unsigned length=0;length<32;length++)raw_hid_receive_kb(request,length);
 for(unsigned i=0;i<16;i++)assert(guarded[i]==0xa5 && guarded[48+i]==0xa5);
}
typedef struct {
 unsigned row,col,logical_row,logical_col,held_low,held_high,released_low,released_high;
} CapturedKey;
static const CapturedKey captured[] = CAPTURED_KEYS;
static void expect_clear(void) {
 matrix_row_t matrix[4]={0};leyden_jar_logical_matrix_scan(matrix);
 for(unsigned r=0;r<4;r++)assert(matrix[r]==0);
}
static void released_bounds(bool high) {
 for(unsigned i=0;i<75;i++){
  const CapturedKey *key=&captured[i];
  levels[key->col][key->row]=(high?key->released_high:key->released_low)+1;
 }
}
int main(void) {
 leyden_jar_init();leyden_jar_calibrate();assert(leyden_jar_is_enabled());expect_clear();check_protocol();
 unsigned detected=0;
 for(unsigned i=0;i<75;i++){
  const CapturedKey *key=&captured[i];
  uint16_t old=levels[key->col][key->row];
  levels[key->col][key->row]=key->held_high+1;
  matrix_row_t matrix[4]={0};leyden_jar_logical_matrix_scan(matrix);
  if(matrix[key->logical_row] & (1u<<key->logical_col))detected++;
#ifndef LEGACY
  for(unsigned r=0;r<4;r++)assert(matrix[r]==(r==key->logical_row?(1u<<key->logical_col):0));
#endif
  levels[key->col][key->row]=old;expect_clear();
 }
#ifdef LEGACY
 assert(detected<75);printf("Previous one-bin profile detects %u/75 captured worst-case held levels.\n",detected);
#else
 assert(detected==75 && NB_CAL_BINS==2 && NB_CUSTOM_CAL_BINS==1);
 printf("Two-bin triggers %u/%u, references %u/%u.\n",s_dac_thresholds[0],s_dac_thresholds[1],s_dac_ref_level[0],s_dac_ref_level[1]);
 for(unsigned high=0;high<2;high++){
  released_bounds(high);expect_clear();
  for(unsigned i=0;i<75;i++){
   const CapturedKey *key=&captured[i];
   uint16_t old=levels[key->col][key->row];
   for(unsigned held=0;held<2;held++){
    levels[key->col][key->row]=(held?key->held_high:key->held_low)+1;
    matrix_row_t matrix[4]={0};leyden_jar_logical_matrix_scan(matrix);
    for(unsigned r=0;r<4;r++)assert(matrix[r]==(r==key->logical_row?(1u<<key->logical_col):0));
   }
   levels[key->col][key->row]=old;expect_clear();
  }
 }
 /* A chord crosses the custom/ordinary bin boundary. */
 released_bounds(false);
 levels[18][3]=433;levels[19][0]=401;
 matrix_row_t matrix[4]={0};leyden_jar_logical_matrix_scan(matrix);
 assert(matrix[0]==(1u<<1) && matrix[3]==1 && !matrix[1] && !matrix[2]);
 released_bounds(true);expect_clear();
 assert(leyden_jar_save_calibration_offset(0,-22));leyden_jar_calibrate();
 assert(s_dac_thresholds[0]==s_dac_ref_level[0]-22);
 assert(leyden_jar_restore_calibration_defaults());
 assert(s_dac_thresholds[0]==s_dac_ref_level[0]-23 && s_dac_thresholds[1]==s_dac_ref_level[1]-22);
 expect_clear();
 scanner_ready=false;matrix_row_t failed[4]={0xffffffff,0xffffffff,0xffffffff,0xffffffff};
 leyden_jar_logical_matrix_scan(failed);
 for(unsigned row=0;row<4;row++)assert(failed[row]==0);
 assert(!leyden_jar_is_enabled() && !leyden_jar_set_enable_keyboard(1));
 puts("All 75 captured keys, held/released bounds, two-bin chord and persistent defaults passed.");
#endif
}
'''.replace('PROTOCOL_SOURCE', str(BOARD/'util_protocol.c'))


class CalibrationTests(unittest.TestCase):
    def run_capture(self, legacy=False):
        fixture = json.loads((Path(__file__).parent/'fixtures/3276_sweep_levels.json').read_text())
        assert fixture['physical_order_matches'] and len(fixture['keys']) == 75
        initial = '{'+','.join('{'+','.join(str(fixture['released'][f'{r},{c}']+1) for r in range(4))+'}' for c in range(20))+'}'
        keys = '{'+','.join('{'+','.join(map(str, [*key['physical'], *key['logical'], key['pressed_min'], key['pressed_max'], key['released_min'], key['released_max']]))+'}' for key in fixture['keys'])+'}'
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root/'quantum.h').write_text(QUANTUM)
            (root/'haptic.h').write_text('')
            (root/'eeconfig.h').write_text(native.EEPROM)
            (root/'test.c').write_text(DRIVER.replace('INITIAL', initial).replace('CAPTURED_KEYS', keys))
            subprocess.run(['cc', '-std=gnu11', '-g', '-fsanitize=address,undefined',
                            *(['-DLEGACY'] if legacy else []), '-I'+tmp,
                            str(root/'test.c'), '-o', str(root/'test')], check=True)
            subprocess.run([str(root/'test')], check=True)

    def test_previous_single_bin(self):
        self.run_capture(True)

    def test_measured_groups(self):
        self.run_capture()


if __name__ == '__main__':
    unittest.main(verbosity=2)
