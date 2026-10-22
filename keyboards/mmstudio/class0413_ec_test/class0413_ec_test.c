// SPDX-License-Identifier: GPL-2.0-or-later
#include "class0413_ec_test.h"
#include "ec_diagnostic.h"
#include "calibration.h"
#include "bootloader.h"
static bool boot_pending;static uint32_t boot_time;
void actuator_storage_quiet(void);
void ec_calibration_request_reset(void) {}
void ec_calibration_cancel(void){ec_cal_abort();}
void raw_hid_receive_kb(uint8_t *d,uint8_t n){
    if(n==32&&((d[0]>=7&&d[0]<=9)||(d[0]==0xca&&d[4]=='2'&&d[6]>=0x17&&d[6]<=0x27)))actuator_storage_quiet();
    if(ec_diagnostic_command(d,n))return;
    if(n==32&&d[0]==0xca&&d[1]=='E'&&d[2]=='C'&&d[3]=='4'&&d[4]=='1'){
        d[8]=2;if(d[5]==0){d[8]=0;d[9]=ec_cal_state;}
        if(d[5]==7){actuator_storage_quiet();ec_cal_abort();ec_va_disconnected();boot_pending=true;boot_time=timer_read32();d[8]=0;}return;
    }d[0]=0xff;
}
bool process_record_kb(uint16_t keycode,keyrecord_t *record){
    if(!process_record_user(keycode,record))return false;
    switch(keycode){
        // Calibration is controlled exclusively by the new desktop workflow.
        case CLASS0413_EC_NOISE:case CLASS0413_EC_BOTTOM_START:return false;
        case CLASS0413_EC_CAL_PRINT:if(record->event.pressed){void ec_show_calibration_data(void);ec_show_calibration_data();}return false;
        case CLASS0413_LK_SONG:if(record->event.pressed)ec_lock_song_toggle();return false;
        default:return true;
    }
}
void matrix_scan_kb(void){ec_va_task();if(boot_pending&&timer_elapsed32(boot_time)>=100)bootloader_jump();matrix_scan_user();}
