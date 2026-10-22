#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Execute the current 0413 split record adapter with RAM EEPROM and Vial canaries."""
from pathlib import Path
import os, subprocess, tempfile
repo=Path(__file__).resolve().parents[3]
s=(repo/'keyboards/alectardy98_ec/common/runtime/record_io.c').read_text()
s='\n'.join(l for l in s.splitlines() if not l.startswith('#include'))
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#define EC_RECORD_SPLIT
#define EECONFIG_KB_DATA_SIZE 219
#define WEAR_LEVELING_LOGICAL_SIZE 4096
#define EC_RECORD_TAIL_ADDR 3983
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 3982
#define DYNAMIC_KEYMAP_MACRO_COUNT 16
#define MIN(a,b) ((a)<(b)?(a):(b))
typedef struct {uint8_t bytes[332];} eeprom_ec_config_t;
static uint8_t mem[4096];static unsigned writes;static bool health=true;
bool ec_storage_ready(void){return health;}
uint32_t dynamic_keymap_macro_get_buffer_size(void){return 1691;}
void dynamic_keymap_macro_get_buffer(uint32_t o,uint32_t n,void*p){assert(o+n<=1691);memcpy(p,mem+2292+o,n);}
uint32_t eeconfig_read_kb_datablock(void*p,uint32_t o,uint32_t n){assert(o+n<=219);memcpy(p,mem+37+o,n);return n;}
uint32_t eeconfig_update_kb_datablock(const void*p,uint32_t o,uint32_t n){assert(o+n<=219);memcpy(mem+37+o,p,n);writes++;return n;}
void eeprom_read_block(void*p,const void*a,uint32_t n){uintptr_t o=(uintptr_t)a;assert(o>=3983&&o+n<=4096);memcpy(p,mem+o,n);}
void eeprom_update_block(const void*p,void*a,uint32_t n){uintptr_t o=(uintptr_t)a;assert(o>=3983&&o+n<=4096);memcpy(mem+o,p,n);writes++;}
'''
main=r'''
int main(void){
 memset(mem,0xa5,sizeof mem);memset(mem+2292,0,1691);uint8_t protected[4096];memcpy(protected,mem,sizeof mem);
 uint8_t data[332],out[332];for(unsigned i=0;i<332;i++)data[i]=i*7;
 assert(ec_record_storage_ready());assert(ec_record_write(data,0,332)==332);assert(ec_record_read(out,0,332)==332&&!memcmp(data,out,332));
 assert(!memcmp(mem,protected,37)&&!memcmp(mem+256,protected+256,3983-256));
 for(unsigned o=0;o<=332;o++)for(unsigned n=0;n<=332-o;n++){assert(ec_record_read(out,o,n)==n);assert(!memcmp(out,data+o,n));}
 unsigned before=writes;assert(!ec_record_write(data,332,1)&&!ec_record_read(out,333,0)&&writes==before);
 mem[3982]=1;assert(!ec_record_storage_ready()&&!ec_record_write(data,0,332)&&writes==before);mem[3982]=0;
 memset(mem+2292,1,1690);assert(!ec_record_storage_ready()&&!ec_record_write(data,0,332)&&writes==before);
 memset(mem+2292,0,1691);health=false;assert(!ec_record_write(data,0,332)&&writes==before);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(pre+s+main)
 flags=['-fsanitize=address,undefined','-fno-pie','-no-pie'] if os.getenv('SANITIZE')=='1' else []
 subprocess.run(['cc','-std=c11',*flags,str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS split 219+113 byte mapping, every read range, Vial canaries, macro conflict and health rejection')
