#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual TinyUF2 sector selection/erase/program functions on host NOR memory."""
from pathlib import Path
import os, re, subprocess, tempfile
repo=Path(__file__).resolve().parents[3]
text=(repo.parent/'tinyuf2/ports/stm32f4/board_flash.c').read_text()
def fn(name):
 pos=re.search(r'\b'+name+r'\s*\(',text).start();start=text.rfind('\n',0,pos)+1;end=text.index('{',pos)+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#define FLASH_BASE_ADDR 0x08000000U
#define BOARD_FLASH_SIZE 524288U
#define BOARD_FLASH_APP_START 0x08010000U
#define BOARD_FLASH_APP_END 0x08040000U
#define FLASH_VOLTAGE_RANGE_3 3
#define FLASH_TYPEPROGRAM_WORD 0
#define HAL_OK 0
#define HAL_MAX_DELAY 0xffffffffU
#define HAL_FLASH_ERROR_NONE 0
#define TUF2_ASSERT(c) do {if(!(c))return false;} while(0)
#define TUF2_LOG1(...) ((void)0)
static uint32_t sector_size[]={16384,16384,16384,16384,65536,131072,131072,131072};
enum {SECTOR_COUNT=8};
static uint8_t erased_sectors[SECTOR_COUNT];
static bool prox_write_failed, fail_erase;
static unsigned erase_mask,programmed,unlocked;
static void HAL_FLASH_Unlock(void){unlocked++;}
static void HAL_FLASH_Lock(void){assert(unlocked);unlocked--;}
static unsigned HAL_FLASH_GetError(void){return 0;}
static unsigned FLASH_WaitForLastOperation(unsigned timeout){(void)timeout;return 0;}
static void FLASH_Erase_Sector(unsigned s,unsigned voltage){
 assert(unlocked&&voltage==3&&(s==4||s==5));erase_mask|=1U<<s;
 uint32_t addr=FLASH_BASE_ADDR;for(unsigned i=0;i<s;i++)addr+=sector_size[i];
 if(!fail_erase)memset((void*)(uintptr_t)addr,255,sector_size[s]);
}
static int HAL_FLASH_Program(unsigned kind,uint32_t addr,uint64_t value){
 (void)kind;assert(unlocked&&addr>=BOARD_FLASH_APP_START&&addr<=BOARD_FLASH_APP_END-4);
 uint32_t *p=(void*)(uintptr_t)addr;assert((*p&(uint32_t)value)==(uint32_t)value);*p&=(uint32_t)value;programmed++;return 0;
}
'''
for name in ['flash_sector_size','is_blank','flash_erase','flash_write','board_flash_write']:code+='\n'+fn(name)
code+=r'''
int main(void){
 assert(mmap((void*)FLASH_BASE_ADDR,BOARD_FLASH_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
 memset((void*)FLASH_BASE_ADDR,0x5a,BOARD_FLASH_SIZE);uint8_t data[256];memset(data,0xa5,sizeof(data));
 assert(board_flash_write(0x08010000,data,256));assert(erase_mask==(1U<<4));
 assert(board_flash_write(0x0801ff00,data,256));assert(erase_mask==(1U<<4));
 assert(board_flash_write(0x08020000,data,256));assert(erase_mask==((1U<<4)|(1U<<5)));
 assert(board_flash_write(0x0803ff00,data,256));assert(erase_mask==((1U<<4)|(1U<<5)));
 const uint32_t forbidden[]={0x08000000,0x08004000,0x08008000,0x0800c000,0x08040000,0x08060000,0x08080000,0xffffff00};
 for(unsigned i=0;i<sizeof(forbidden)/sizeof(*forbidden);i++){unsigned before=programmed;assert(!board_flash_write(forbidden[i],data,256));assert(programmed==before&&!unlocked);}
 for(uint32_t a=FLASH_BASE_ADDR;a<BOARD_FLASH_APP_START;a++)assert(*(uint8_t*)(uintptr_t)a==0x5a);
 for(uint32_t a=BOARD_FLASH_APP_END;a<FLASH_BASE_ADDR+BOARD_FLASH_SIZE;a++)assert(*(uint8_t*)(uintptr_t)a==0x5a);
 memset(erased_sectors,0,sizeof erased_sectors);memset((void*)BOARD_FLASH_APP_START,0x5a,65536);prox_write_failed=false;fail_erase=true;unsigned before=programmed;
 assert(!board_flash_write(BOARD_FLASH_APP_START,data,256)&&prox_write_failed&&programmed==before&&!unlocked);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.c').write_text(code)
 flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.getenv('SANITIZE')=='1' else []
 subprocess.run(['cc','-std=gnu11','-fno-pie','-no-pie',*flags,str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS actual STM32F4 sector selection and NOR erase/write: only sectors4/5; boot/EEPROM/reserved/upper flash preserved; erase failure latched')
