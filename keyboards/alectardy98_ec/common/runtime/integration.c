// SPDX-License-Identifier: GPL-3.0-or-later
#include "quantum.h"
#include "ec_switch_matrix.h"
#include "calibration.h"
#include "ec_diagnostic.h"
#include "usb_device_state.h"
#include "bootloader.h"
static bool configured,boot_pending;
static uint32_t boot_time;
void ec_calibration_cancel(void){ec_cal_abort();}
bool ec_actuators_blocked(void){return usb_device_state_get_configure_state()!=USB_DEVICE_STATE_CONFIGURED;}
void raw_hid_receive_kb(uint8_t *d,uint8_t n){
 if(n==32&&d[0]==0xca&&d[1]=='E'&&d[2]=='C'&&d[3]=='4'&&d[4]=='1'&&d[5]==7){
  ec_cal_abort();ec_va_disconnected();boot_pending=true;boot_time=timer_read32();d[8]=0;return;
 }
 if(!ec_diagnostic_command(d,n))d[0]=0xff;
}
void matrix_scan_kb(void){
 bool now=!ec_actuators_blocked();
 if(configured&&!now){ec_cal_abort();ec_va_disconnected();}
 configured=now;ec_va_task();if(boot_pending&&timer_elapsed32(boot_time)>=100)bootloader_jump();matrix_scan_user();
}
