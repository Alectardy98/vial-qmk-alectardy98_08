#!/usr/bin/env python3
"""ASan/UBSan checks for the real 3178 scanner and direct-switch protocol."""
from pathlib import Path
import subprocess
import tempfile
import unittest
import test_calibration as native

QUANTUM = native.QUANTUM.replace('#define MATRIX_ROWS 8','#define MATRIX_ROWS 9').replace('#define MATRIX_COLS 16','#define MATRIX_COLS 13')
QUANTUM = QUANTUM.replace(str(native.BOARD/'ibm/fat/config.h'),str(native.BOARD/'ibm/3178/config.h'))
QUANTUM = QUANTUM.replace('#include "ROOT_CONFIG"','')
QUANTUM += '''
#define GP27 27
static bool direct_high = true, pullup_ready;
static unsigned direct_pin;
static void setPinInputHigh(unsigned pin) { direct_pin=pin;pullup_ready=true; }
static bool readPin(unsigned pin) { if(!pullup_ready || pin!=direct_pin) abort();return direct_high; }
'''
# This standalone fixture deliberately does not include either personal FAT header.
QUANTUM = QUANTUM.replace('#ifdef PERSONAL\n#include "'+str(native.BOARD/'ibm/fat/keymaps/vial_alec/config.h')+'"\n#endif','')
DRIVER = native.DRIVER[:native.DRIVER.index('static void no_keys(void)')]
DRIVER = DRIVER.replace('void pio_matrix_scan_init(bool extra){(void)extra;}', 'void pio_matrix_scan_init(bool extra){assert(!extra);}')
DRIVER += r'''
#define VIA_ENABLE
#define id_get_keyboard_value 2
#define id_set_keyboard_value 3
#define id_unhandled 255
void reset_keyboard(void){abort();}
void soft_reset_keyboard(void){abort();}
void eeconfig_disable(void){abort();}
#include "PROTOCOL"
static void clear_matrix(void) {
 matrix_row_t m[9]={0};leyden_jar_logical_matrix_scan(m);
 for(unsigned r=0;r<9;r++)assert(m[r]==0);
}
int main(void) {
 leyden_jar_init();assert(pullup_ready && direct_pin==27);
 leyden_jar_calibrate();assert(s_matrixToControllerRow[8]==255);
 assert(CONTROLLER_COLS==16 && sizeof s_active_pads==16);
 clear_matrix();unsigned keys=0;
 for(unsigned r=0;r<8;r++)for(unsigned c=0;c<13;c++) {
  unsigned pr=s_matrixToControllerRow[r],pc=s_matrixToControllerCol[c];
  if(!leyden_jar_pad_is_active(pr,pc))continue;
  uint16_t old=levels[pc][pr];levels[pc][pr]=s_dac_thresholds[0]+20;
  matrix_row_t m[9]={0};leyden_jar_logical_matrix_scan(m);
  for(unsigned row=0;row<9;row++)assert(m[row]==(r==row?(1u<<c):0));
  levels[pc][pr]=old;clear_matrix();keys++;
 }
 assert(keys==87);
 direct_high=false;matrix_row_t m[9]={0};leyden_jar_logical_matrix_scan(m);
 for(unsigned row=0;row<9;row++)assert(m[row]==(row==8?(1u<<12):0));
 uint8_t request[32]={id_get_keyboard_value,id_leyden_jar_details,0xC0,0x21};
 raw_hid_receive_kb(request,32);
 assert(request[4]==9 && request[5]==13 && request[6]==8 && request[7]==16);
 assert(request[10]==1 && request[11]==8 && request[12]==12 && request[13]==27);
 leyden_jar_set_scan_logical_matrix();uint32_t bits=0;
 assert(leyden_jar_get_logical_matrix_row(&bits,8) && bits==(1u<<12));
 uint8_t rows[8];assert(leyden_jar_get_matrix_to_controller_rows(rows,8));
 for(unsigned i=0;i<8;i++)assert(rows[i]==7-i);
 direct_high=true;clear_matrix();
 puts("3178: 87 capacitive positions + separate GPIO27 switch; 9-row bounds and protocol passed.");
}
'''.replace('PROTOCOL',str(native.BOARD/'util_protocol.c'))

class DirectTests(unittest.TestCase):
    def test_3178_direct_switch(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)
            (path/'quantum.h').write_text(QUANTUM)
            (path/'eeconfig.h').write_text(native.EEPROM)
            (path/'haptic.h').write_text('')
            (path/'test.c').write_text(DRIVER.replace('INITIAL','{[0 ... 17] = {[0 ... 7] = 401}}'))
            subprocess.run(['cc','-std=gnu11','-g','-fsanitize=address,undefined','-I'+tmp,str(path/'test.c'),'-o',str(path/'test')],check=True)
            subprocess.run([str(path/'test')],check=True)

if __name__=='__main__': unittest.main(verbosity=2)
