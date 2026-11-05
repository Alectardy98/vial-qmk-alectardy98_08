// SPDX-License-Identifier: GPL-3.0-or-later
// Offline only: real board/engine sources below, simulated ADC/GPIO/EEPROM.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "post_config.h"
#include "ec_switch_matrix.h"
static uint32_t now,writes,adc_reads;
static bool pins[48],adc_ready,storage_ready=true,boot_called;
static uint16_t input[MATRIX_ROWS*MATRIX_COLS];
static uint8_t persisted[4096];
matrix_row_t raw_matrix[MATRIX_ROWS],matrix[MATRIX_ROWS];
uint32_t timer_read32(void){return now;}
uint32_t timer_elapsed32(uint32_t t){return now-t;}
matrix_row_t matrix_get_row(uint8_t r){return matrix[r];}
uint32_t eeconfig_read_kb_datablock(void *p,uint32_t o,uint32_t n){assert(o+n<=sizeof(eeprom_ec_config_t));memcpy(p,persisted+o,n);return n;}
uint32_t eeconfig_update_kb_datablock(const void *p,uint32_t o,uint32_t n){assert(o+n<=sizeof(eeprom_ec_config_t));if(!storage_ready)return 0;memcpy(persisted+o,p,n);writes++;return n;}
bool ec_storage_ready(void){return storage_ready;}
uint16_t a98_storage_density(void){return 256;}
void eeconfig_init_user(void){}
void keyboard_post_init_user(void){}
void matrix_scan_user(void){}
void bootloader_jump(void){boot_called=true;}
int usb_device_state_get_configure_state(void){return 1;}
void palSetLineMode(pin_t p,int mode){assert(p==ANALOG_PORT && mode==PAL_MODE_INPUT_ANALOG);}
adc_mux pinToMux(pin_t p){assert(p==ANALOG_PORT);return 1;}
void gpio_set_pin_output(pin_t p){assert(p<48);}
void gpio_set_pin_output_open_drain(pin_t p){assert(p==DISCHARGE_PIN);}
void gpio_write_pin(pin_t p,bool v){assert(p<48);pins[p]=v;}
void gpio_write_pin_high(pin_t p){gpio_write_pin(p,true);}
void gpio_write_pin_low(pin_t p){gpio_write_pin(p,false);}
void wait_us(uint16_t n){(void)n;}
void wait_ms(uint16_t n){now+=n;}
#include "../acquisition.c"
#include "../matrix.c"
#include "../../common/engine/calibration.c"
#undef CELLS
#undef RECORD_BYTES
#include "../../common/engine/ec_diagnostic.c"
#include "../../common/runtime/decision.c"
#include "../../common/runtime/ec_board.c"
#include "../../common/runtime/integration.c"
uint16_t adc_read(adc_mux mux){
    assert(mux==1);adc_reads++;if(!adc_ready)return 100;
    unsigned row=MATRIX_ROWS,channel=AMUX_COUNT,selected=0;
    for(unsigned i=0;i<MATRIX_ROWS;i++)if(pins[row_pins[i]]){assert(row==MATRIX_ROWS);row=i;}
    for(unsigned i=0;i<AMUX_COUNT;i++)if(!pins[amux_en_pins[i]]){assert(channel==AMUX_COUNT);channel=i;}
    for(unsigned i=0;i<ARRAY_SIZE(amux_sel_pins);i++)if(pins[amux_sel_pins[i]])selected|=1U<<i;
    assert(row<MATRIX_ROWS && channel<AMUX_COUNT);
    unsigned offset=0;for(unsigned i=0;i<channel;i++)offset+=amux_n_col_sizes[i];
    for(unsigned c=0;c<amux_n_col_sizes[channel];c++)if(amux_n_col_channels[channel][c]==selected){
        assert(!is_unused_position(row,offset+c));return input[row*MATRIX_COLS+offset+c];
    }
    abort();
}
static void scan(unsigned count){while(count--){matrix_scan_custom(matrix);matrix_scan_kb();now++;}}
int main(void){
    setbuf(stdout,NULL);memset(persisted,0xff,sizeof persisted);
    for(unsigned i=0;i<ARRAY_SIZE(input);i++)input[i]=100;
    ec_init();adc_ready=true;ec_noise_floor();
    assert(adc_reads==1+30*93);keyboard_post_init_kb();scan(1);
    uint8_t b[32];while(fread(b,1,sizeof b,stdin)==sizeof b){
        if(!memcmp(b,"TEST",4)){
            unsigned i=((unsigned)b[5]<<8)|b[6];unsigned value=((unsigned)b[7]<<8)|b[8];
            switch(b[4]){
                case 1:assert(i<ARRAY_SIZE(input));input[i]=value;break;
                case 2:scan(i);break;
                case 3:now+=i;break;
                case 4:ec_cal_abort();memset(matrix,0,sizeof matrix);keyboard_post_init_kb();break;
                case 5:b[8]=writes>>24;b[9]=writes>>16;b[10]=writes>>8;b[11]=writes;break;
                case 6:storage_ready=(bool)b[8];break;
                case 7:b[8]=boot_called;break;
                default:abort();
            }
        }else if(b[0]==0xfe && b[1]==0){
            static const uint8_t uid[]=VIAL_KEYBOARD_UID;
            memset(b,0,sizeof b);b[0]=6;memcpy(b+4,uid,8);
        }else{
            scan(1);raw_hid_receive_kb(b,sizeof b);
        }
        if(fwrite(b,1,sizeof b,stdout)!=sizeof b)return 2;
    }
    return ferror(stdin)?1:0;
}
