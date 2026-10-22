#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the actual QMK jump and TinyUF2 validity/completion functions with host IO."""
from pathlib import Path
import re, subprocess, tempfile, os
repo=Path(__file__).resolve().parents[3];tiny=repo.parent/'tinyuf2'
def fn(text,name):
 pos=re.search(r'\b'+name+r'\s*\(',text).start();start=text.rfind('\n',0,pos)+1;end=text.index('{',pos)+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
board=(tiny/'ports/stm32f4/boards.c').read_text();qmk=(repo/'platforms/chibios/bootloaders/tinyuf2.c').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>
#define BOARD_FLASH_APP_START 0x08010000U
#define BOARD_FLASH_APP_END 0x08040000U
#define BOARD_FLASH_SIZE 524288U
#define TUF2_LOG1_HEX(x) ((void)(x))
#define DBL_TAP_MAGIC 0xF01669EFU
static volatile uint32_t mailbox;
#define DBL_TAP_REG mailbox
static unsigned resets;
static bool failed;
static void NVIC_SystemReset(void){assert(mailbox==DBL_TAP_MAGIC);resets++;}
static int ec_prox_flash_failed(void){return failed;}
'''
code+='\n'+fn(qmk,'bootloader_jump')+'\n'+fn(board,'board_app_valid')+'\n'+fn(board,'board_dfu_complete')
code+=r'''
int main(void){
 assert(mmap((void*)BOARD_FLASH_APP_START,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
 volatile uint32_t *vector=(void*)BOARD_FLASH_APP_START;
 vector[0]=0x20000400;vector[1]=0x080102fd;assert(board_app_valid());
 const uint32_t bad_sp[]={0,0xffffffff,0x20000000,0x20000404,0x2000fffc,0x20010000};
 for(unsigned i=0;i<sizeof(bad_sp)/sizeof(*bad_sp);i++){vector[0]=bad_sp[i];assert(!board_app_valid());}
 vector[0]=0x20000400;
 const uint32_t bad_pc[]={0xffffffff,0,0x08000001,0x080102fc,0x08040001};
 for(unsigned i=0;i<sizeof(bad_pc)/sizeof(*bad_pc);i++){vector[1]=bad_pc[i];assert(!board_app_valid());}
 bootloader_jump();assert(mailbox==DBL_TAP_MAGIC&&resets==1);
 failed=true;board_dfu_complete();assert(resets==1);failed=false;board_dfu_complete();assert(resets==2);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.c').write_text(code)
 flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.getenv('SANITIZE')=='1' else []
 subprocess.run(['cc','-std=gnu11','-fno-pie','-no-pie',*flags,str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS actual QMK SRAM magic/reset; TinyUF2 SP/Thumb/address validation and failed-update completion stays in bootloader')
