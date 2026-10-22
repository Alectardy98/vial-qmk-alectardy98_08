#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise actual opt-in TinyUF2 write/packet guards against host-mapped flash.
No USB access, no physical flash. Pass TinyUF2 checkout as first argument.
"""
from pathlib import Path
import os, subprocess, sys, tempfile, re
repo = Path(__file__).resolve().parents[3]
tiny = Path(sys.argv[1]) if len(sys.argv)>1 else repo.parent/'tinyuf2'
def function(text, name):
    pos=re.search(r'\b'+name+r'\s*\(',text).start(); start=text.rfind('\n',0,pos)+1
    end=text.index('{',pos)+1; depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[start:end]
flash=(tiny/'ports/stm32f4/board_flash.c').read_text()
ghost=(tiny/'src/ghostfat.c').read_text()
source=r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <sys/mman.h>
#define BOARD_FLASH_APP_START 0x08010000U
#define BOARD_FLASH_APP_END 0x08040000U
#define BOARD_UF2_FAMILY_ID 0x57755a57U
#define UF2_MAGIC_START0 0x0A324655U
#define UF2_MAGIC_START1 0x9E5D5157U
#define UF2_MAGIC_END 0x0AB16F30U
#define UF2_FLAG_FAMILYID 0x2000U
#define UF2_FLAG_NOFLASH 1U
#define MAX_BLOCKS 1024
#define BPB_SECTOR_SIZE 512
#define HAL_FLASH_ERROR_NONE 0
static bool prox_write_failed, corrupt, erase_fail, program_fail, wait_fail;
static unsigned calls, unlocked, flushed;
static unsigned error;
static void HAL_FLASH_Unlock(void){unlocked++;}
static void HAL_FLASH_Lock(void){assert(unlocked);unlocked--;}
static unsigned HAL_FLASH_GetError(void){return error;}
#define TUF2_LOG1(...) ((void)0)
#define HAL_OK 0
#define HAL_MAX_DELAY 0xffffffffU
#define FLASH_TYPEPROGRAM_WORD 0
static bool flash_erase(uint32_t addr){assert(addr>=BOARD_FLASH_APP_START && addr<BOARD_FLASH_APP_END);return !erase_fail;}
static int HAL_FLASH_Program(int type,uint32_t addr,uint64_t data){
 (void)type;calls++;if(program_fail)return 1;if(!corrupt)memcpy((void*)(uintptr_t)addr,&data,4);return 0;
}
static int FLASH_WaitForLastOperation(unsigned timeout){(void)timeout;return wait_fail;}
static void board_flash_flush(void){flushed++;}
typedef struct {
 uint32_t magicStart0,magicStart1,flags,targetAddr,payloadSize,blockNo,numBlocks,familyID;
 uint8_t data[476];uint32_t magicEnd;
} UF2_Block;
typedef struct {uint32_t numBlocks,numWritten;uint8_t writtenMask[MAX_BLOCKS/8];} WriteState;
'''
source+='\n'+function(flash,'flash_write')+'\n'+function(flash,'board_flash_write')+'\n'+function(ghost,'is_uf2_block')+'\n'+function(ghost,'uf2_write_block')
source+=r'''
static UF2_Block valid(void){
 UF2_Block b={.magicStart0=UF2_MAGIC_START0,.magicStart1=UF2_MAGIC_START1,
 .magicEnd=UF2_MAGIC_END,.flags=UF2_FLAG_FAMILYID,.targetAddr=BOARD_FLASH_APP_START,
 .payloadSize=256,.familyID=BOARD_UF2_FAMILY_ID,.numBlocks=1};memset(b.data,0xa5,256);return b;
}
int main(void){
 assert(mmap((void*)0x08000000U,0x40000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
 memset((void*)0x08000000U,0x5a,0x40000);
 UF2_Block b=valid();WriteState state={0};
 assert(uf2_write_block(0,(void*)&b,&state)==512 && state.numWritten==1 && calls==64 && flushed==1);
 b=valid();b.targetAddr=BOARD_FLASH_APP_END-256;assert(uf2_write_block(0,(void*)&b,&state)==512);
 const uint32_t rejected[]={0x08000000,0x08004000,0x08008000,0x0800c000,0x08040000,0xfffffff0,0x08010001};
 for(unsigned i=0;i<sizeof(rejected)/sizeof(*rejected);i++){
  b=valid();b.targetAddr=rejected[i];unsigned before=calls;WriteState s={0};
  assert(uf2_write_block(0,(void*)&b,&s)==-1 && !s.numWritten && calls==before);
  assert(!board_flash_write(b.targetAddr,b.data,256) && !unlocked && calls==before);
 }
 for(unsigned len=0;len<=512;len++)if(len!=256){
  b=valid();b.payloadSize=len;unsigned before=calls;WriteState s={0};assert(uf2_write_block(0,(void*)&b,&s)==-1&&!s.numWritten&&calls==before);
 }
 b=valid();b.familyID++;assert(uf2_write_block(0,(void*)&b,&state)==-1);
 b=valid();b.flags=0;assert(uf2_write_block(0,(void*)&b,&state)==-1);
 b=valid();b.flags|=UF2_FLAG_NOFLASH;assert(uf2_write_block(0,(void*)&b,&state)==-1);
 // Failed programming must not be counted as a completed update.
 prox_write_failed=false;b=valid();memset((void*)(uintptr_t)b.targetAddr,0xff,256);corrupt=true;state=(WriteState){0};
 assert(uf2_write_block(0,(void*)&b,&state)==-1&&!state.numWritten&&prox_write_failed);
 corrupt=false;error=1;assert(!board_flash_write(b.targetAddr,b.data,256));
 // Actual flash_write helper propagates erase/program/wait failures.
 error=0;corrupt=false;
 for(unsigned kind=0;kind<3;kind++){
  prox_write_failed=false;erase_fail=kind==0;program_fail=kind==1;wait_fail=kind==2;
  b=valid();assert(!board_flash_write(b.targetAddr,b.data,256)&&prox_write_failed&&!unlocked);
 }
 // Bootloader, EEPROM and reserved sector canaries remain intact.
 for(uintptr_t a=0x08000000;a<0x08010000;a++)assert(*(uint8_t*)a==0x5a);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.c').write_text(source)
 flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.getenv('SANITIZE')=='1' else []
 subprocess.run(['cc','-std=gnu11','-fno-pie','-no-pie',*flags,str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
board=(tiny/'ports/stm32f4/boards/ec_prox/board.h').read_text()
assert 'TINYUF2_PROTECT_BOOTLOADER 0' in board and 'HSE_VALUE' not in board.split('clock_init')[0]
boards=(tiny/'ports/stm32f4/boards.c').read_text()
assert 'if (ec_prox_flash_failed()) return;' in boards
for path in ['acquisition.c','matrix.c']:
 assert (repo/'keyboards/alectardy98_ec/ec_prox'/path).read_bytes()==(repo/'keyboards/alectardy98_ec/ec_prox_uf2'/path).read_bytes()
print('PASS actual UF2/write guard functions: range/alignment/length/family/flags/error/readback; storage canaries; unchanged acquisition/scanner')

# Same application identity/layout intentionally uses the existing GUI profile.
import json
old=json.loads((repo/'keyboards/alectardy98_ec/ec_prox/keyboard.json').read_text())
new=json.loads((repo/'keyboards/alectardy98_ec/ec_prox_uf2/keyboard.json').read_text())
for field in ['usb','matrix_size','layouts','manufacturer','keyboard_name','rgblight']:
 assert old[field]==new[field],field
for keymap in ['vial','vial_alec']:
 assert (repo/f'keyboards/alectardy98_ec/ec_prox/keymaps/{keymap}/vial.json').read_bytes()==(repo/f'keyboards/alectardy98_ec/ec_prox_uf2/keymaps/{keymap}/vial.json').read_bytes()
print('PASS unchanged application identity, GUI geometry/layout and Vial definitions')
