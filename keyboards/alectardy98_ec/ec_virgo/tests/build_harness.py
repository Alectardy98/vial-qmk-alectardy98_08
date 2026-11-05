#!/usr/bin/env python3
"""Build the actual Virgo acquisition and EC42 sources with host-only I/O mocks."""
from pathlib import Path
import argparse,json,os,subprocess
BOARD=Path(__file__).resolve().parents[1]
REPO=BOARD.parents[2]

def build(output, sanitize=False):
    output=Path(output).resolve();output.parent.mkdir(parents=True,exist_ok=True)
    stubs=output.parent/(output.name+'-headers');stubs.mkdir(exist_ok=True)
    metadata=json.loads((BOARD/'keyboard.json').read_text())['matrix_size']
    prefix='''#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define PACKED __attribute__((packed))
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define MIN(a,b) ((a)<(b)?(a):(b))
'''
    prefix+=f"#define MATRIX_ROWS {metadata['rows']}\n#define MATRIX_COLS {metadata['cols']}\n"
    prefix+='\n'.join(f'#define {port}{pin} {bank*16+pin}' for bank,port in enumerate('ABC') for pin in range(16))+'\n'
    prefix+='''typedef uint16_t pin_t;
typedef uint8_t adc_mux;
typedef uint32_t matrix_row_t;
#define PAL_MODE_INPUT_ANALOG 0
#define PAL_MODE_OUTPUT_OPENDRAIN 1
#define ATOMIC_BLOCK_FORCEON for(int host_once=1;host_once;host_once=0)
#define uprintf(...) ((void)0)
#define print(...) ((void)0)
#define USB_DEVICE_STATE_CONFIGURED 1
uint32_t timer_read32(void);
uint32_t timer_elapsed32(uint32_t);
matrix_row_t matrix_get_row(uint8_t);
uint32_t eeconfig_read_kb_datablock(void *,uint32_t,uint32_t);
uint32_t eeconfig_update_kb_datablock(const void *,uint32_t,uint32_t);
void eeconfig_init_user(void);
void keyboard_post_init_user(void);
void keyboard_post_init_kb(void);
void matrix_scan_user(void);
void bootloader_jump(void);
void raw_hid_receive_kb(uint8_t *,uint8_t);
int usb_device_state_get_configure_state(void);
void palSetLineMode(pin_t,int);
adc_mux pinToMux(pin_t);
uint16_t adc_read(adc_mux);
void gpio_set_pin_output(pin_t);
void gpio_set_pin_output_open_drain(pin_t);
void gpio_write_pin_high(pin_t);
void gpio_write_pin_low(pin_t);
void gpio_write_pin(pin_t,bool);
void wait_us(uint16_t);
void wait_ms(uint16_t);
'''
    (stubs/'host.h').write_text(prefix)
    for name in ('matrix.h','eeconfig.h','util.h','quantum.h','analog.h','atomic_util.h','print.h','wait.h','usb_device_state.h','bootloader.h','via.h','keyboard.h'):
        (stubs/name).write_text('#pragma once\n#include "host.h"\n')
    command=['cc','-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fno-omit-frame-pointer']
    if sanitize:command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all']
    for include in (stubs,BOARD,BOARD.parent/'common/engine',BOARD.parent/'common/runtime',BOARD.parent/'common/interfaces'):
        command+=['-I',str(include)]
    command+=['-include',str(stubs/'host.h'),str(BOARD/'tests/host_harness.c'),'-lm','-o',str(output)]
    subprocess.run(command,check=True)
    return output

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path);parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();print(build(args.output,args.sanitize))
