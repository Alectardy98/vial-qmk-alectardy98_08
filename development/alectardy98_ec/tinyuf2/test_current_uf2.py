#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Full actual GhostFAT CURRENT.UF2 export/roundtrip, physical512K vs app192K."""
from pathlib import Path
import tempfile, subprocess, os
repo=Path(__file__).resolve().parents[3];tiny=repo.parent/'tinyuf2'
header=r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define BOARD_FLASH_ADDR_ZERO 0x08000000U
#define BOARD_FLASH_APP_START 0x08010000U
#define BOARD_FLASH_APP_END 0x08040000U
#define BOARD_UF2_FAMILY_ID 0x57755a57U
#define CFG_UF2_FLASH_SIZE 524288U
#define UF2_VERSION "host-test"
#define UF2_PRODUCT_NAME "EC Pro X TinyUF2"
#define UF2_BOARD_ID "EC-Pro-X-STM32F411"
#define UF2_VOLUME_LABEL "ECPROXBOOT"
#define UF2_INDEX_URL "https://github.com/adafruit/tinyuf2"
#define TUF2_LOG 0
'''
code='#include <assert.h>\n#include "'+str(tiny/'src/ghostfat.c')+'"\n'
code+=r'''
static unsigned reads,writes,flushes;
uint32_t board_flash_size(void){return 524288;}
void board_flash_read(uint32_t addr,void *buffer,uint32_t len){
 assert(addr>=BOARD_FLASH_APP_START&&len==256&&addr<=BOARD_FLASH_APP_END-len);
 for(unsigned i=0;i<len;i++)((uint8_t*)buffer)[i]=(uint8_t)((addr+i)^((addr+i)>>8));
 reads++;
}
bool board_flash_write(uint32_t addr,const void *data,uint32_t len){
 assert(addr>=BOARD_FLASH_APP_START&&len==256&&addr<=BOARD_FLASH_APP_END-len);
 for(unsigned i=0;i<len;i++)assert(((const uint8_t*)data)[i]==(uint8_t)((addr+i)^((addr+i)>>8)));
 writes++;return true;
}
void board_flash_flush(void){flushes++;}
int main(void){
 uf2_init();assert(strstr(infoUf2File,"00080000")); // physical density remains truthful
 assert(UF2_SECTOR_COUNT==768 && info[FID_UF2].size==393216);
 unsigned first=FS_START_CLUSTERS_SECTOR+(info[FID_UF2].cluster_start-2)*BPB_SECTORS_PER_CLUSTER;
 uint8_t buffer[512];WriteState state={0};
 for(unsigned i=0;i<UF2_SECTOR_COUNT;i++){
  uf2_read_block(first+i,buffer);UF2_Block *block=(void*)buffer;
  assert(is_uf2_block(block)&&block->familyID==0x57755a57&&block->payloadSize==256);
  assert(block->blockNo==i&&block->numBlocks==768&&block->targetAddr==BOARD_FLASH_APP_START+i*256);
  assert(uf2_write_block(i,buffer,&state)==512);
 }
 assert(reads==768&&writes==768&&state.numWritten==768&&flushes==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'boards.h').write_text(header);(p/'test.c').write_text(code)
 flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.getenv('SANITIZE')=='1' else []
 subprocess.run(['cc','-std=gnu11',*flags,'-I'+str(p),'-I'+str(tiny/'src'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS full actual GhostFAT CURRENT.UF2: 768 valid blocks export/roundtrip all192KiB app, correct final block, physical512KiB INFO, no protected reads/writes')
