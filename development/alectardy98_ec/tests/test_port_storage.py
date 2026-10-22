#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real QMK EFL backend/journal and new guards, mapped in host memory only."""
from pathlib import Path
import ast,tempfile,subprocess,os
repo=Path(__file__).resolve().parents[3];runtime=repo/'keyboards/alectardy98_ec/common/runtime'
hal=(Path(__file__).with_name('flash_hal.h')).read_text()
hal=hal.replace('#define FLASH ((void *)0x40023c00U)','#define FLASH (&fake_flash)\ntypedef struct {uint32_t OPTCR;} FakeFlash;\nextern FakeFlash fake_flash;')
hal=hal.replace('assert(i==2);','assert(i==WEAR_LEVELING_EFL_FIRST_SECTOR);')
a=hal.index('static inline void *flashGetOffsetAddress(');z=hal.index('\nint eflStart',a);hal=hal[:a]+'''static inline void *flashGetOffsetAddress(BaseFlash*p,uint32_t off){(void)p;assert(off>=A98_STORAGE_START && off+4<=A98_STORAGE_END);return (void*)(uintptr_t)(0x08000000U+off);}
'''+hal[z:]
hal+='\nint efl_lld_query_erase(void*,uint32_t*);\n'
main=r'''
#define _GNU_SOURCE
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include "hal.h"
#include "wear_leveling.h"
#include "wear_leveling_internal.h"
#include "diag.h"
FakeFlash fake_flash={0x0fff0000};EFlashDriver EFLD1;
static bool unlocked,program_error,query_error;
static unsigned programs,erases,cut=~0U;
void eeprom_driver_init(void);
void a98_hw_start(EFlashDriver *p){assert(p==&EFLD1);unlocked=true;p->state=FLASH_READY;}
int eflStart(EFlashDriver*p,void*cfg){(void)cfg;efl_lld_start(p);return 0;}
void eflStop(EFlashDriver*p){assert(p==&EFLD1);unlocked=false;p->state=FLASH_STOP;}
flash_error_t a98_hw_program(void*p,flash_offset_t o,size_t n,const uint8_t*d){
 assert(p==&EFLD1&&unlocked&&o>=A98_STORAGE_START&&o+n<=A98_STORAGE_END);programs++;
 if(program_error)return 11;
 uint8_t *b=(uint8_t*)(uintptr_t)(0x08000000U+o);
 for(unsigned j=0;j<n;j++){if(!cut)return 11;if(cut!=~0U)--cut;assert((b[j]&d[j])==d[j]);b[j]&=d[j];}
 return 0;
}
flash_error_t a98_hw_erase(void*p,flash_sector_t s){assert(p==&EFLD1&&unlocked&&s==WEAR_LEVELING_EFL_FIRST_SECTOR);erases++;memset((void*)(uintptr_t)(0x08000000U+A98_STORAGE_START),255,A98_STORAGE_END-A98_STORAGE_START);EFLD1.state=FLASH_ERASE;return 0;}
flash_error_t a98_hw_query_erase(void*p,uint32_t*m){(void)m;assert(p==&EFLD1);EFLD1.state=FLASH_READY;return query_error?11:0;}
int flashProgram(BaseFlash*p,uint32_t o,size_t n,const uint8_t*d){return efl_lld_program(p,o,n,d);}
int flashStartEraseSector(BaseFlash*p,unsigned s){return efl_lld_start_erase_sector(p,s);}
int flashWaitErase(BaseFlash*p){return efl_lld_query_erase(p,0);}
static unsigned hash_outside(void){unsigned h=2166136261U;for(unsigned i=0;i<HOST_PHYSICAL_FLASH_SIZE;i++)if(i<A98_STORAGE_START||i>=A98_STORAGE_END)h=(h^*(uint8_t*)(uintptr_t)(0x08000000U+i))*16777619U;return h;}
static void initialize(void){
 cut=~0U;program_error=query_error=false;eeprom_driver_init();assert(ec_storage_ready());
}
int main(void){
 assert(mmap((void*)0x08000000U,HOST_PHYSICAL_FLASH_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
 assert(mmap((void*)0x1fff7000U,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);
 memset((void*)0x08000000U,0x5a,HOST_PHYSICAL_FLASH_SIZE);
 *(uint16_t*)0x1fff7a22=HOST_PHYSICAL_FLASH_SIZE/1024;
 flash_descriptor_t *d=(void*)(uintptr_t)(A98_TABLE_START+0x1000);
 flash_sector_descriptor_t *sectors=(void*)(uintptr_t)(A98_TABLE_START+0x1100);
 unsigned count=HOST_PHYSICAL_FLASH_SIZE==524288?8:6;
 for(unsigned i=0;i<count;i++){sectors[i].offset=i<4?i*0x4000U:i==4?0x10000U:0x20000U+(i-5)*0x20000U;sectors[i].size=i<4?0x4000U:i==4?0x10000U:0x20000U;}
 *d=(flash_descriptor_t){3,4,count,sectors,0,(uint8_t*)0x08000000U,HOST_PHYSICAL_FLASH_SIZE};EFLD1=(EFlashDriver){d,FLASH,FLASH_STOP};
 memset((void*)(uintptr_t)(0x08000000U+A98_STORAGE_START),255,A98_STORAGE_END-A98_STORAGE_START);
 unsigned outside=hash_outside();
#if WEAR_LEVELING_EFL_FIRST_SECTOR == 1
 extern uint8_t prox_storage_fault_code(void); extern void prox_storage_clear_trace(void);
 d->page_size=8; eeprom_driver_init(); assert(!ec_storage_ready() && prox_storage_fault_code()==1 && programs==0 && erases==0);
 d->page_size=4; prox_storage_clear_trace();
#endif
 initialize();assert(programs==0&&erases==0);
 uint8_t sentinel=73,key_sentinel=44,read=0;assert(wear_leveling_write(3000,&sentinel,1)!=WEAR_LEVELING_FAILED);assert(wear_leveling_write(500,&key_sentinel,1)!=WEAR_LEVELING_FAILED);
 for(unsigned i=0;i<(A98_STORAGE_END-A98_STORAGE_START)/2;i++){
  uint8_t value=i;assert(wear_leveling_write(40,&value,1)!=WEAR_LEVELING_FAILED);
 }
 assert(erases>0);initialize();assert(wear_leveling_read(3000,&read,1)!=WEAR_LEVELING_FAILED&&read==sentinel);assert(wear_leveling_read(500,&read,1)!=WEAR_LEVELING_FAILED&&read==key_sentinel);assert(hash_outside()==outside);
 // Simulated power loss at each byte of a real journal append.
 uint8_t snapshot[A98_STORAGE_END-A98_STORAGE_START];memcpy(snapshot,(void*)(uintptr_t)(0x08000000U+A98_STORAGE_START),sizeof snapshot);
 for(unsigned prefix=0;prefix<12;prefix++){
  memcpy((void*)(uintptr_t)(0x08000000U+A98_STORAGE_START),snapshot,sizeof snapshot);initialize();cut=prefix;
  uint8_t replacement=22;wear_leveling_write(40,&replacement,1);backing_store_lock();initialize();
  assert(wear_leveling_read(3000,&read,1)!=WEAR_LEVELING_FAILED&&read==sentinel);assert(wear_leveling_read(500,&read,1)!=WEAR_LEVELING_FAILED&&read==key_sentinel);assert(hash_outside()==outside);
 }
 // All rejected requests stop before unlock/program/erase, including wraparound.
 const uint32_t offsets[]={0,A98_STORAGE_START-4,A98_STORAGE_END,A98_STORAGE_END+4,0xfffffffcU};uint32_t value=0;
 for(unsigned i=0;i<sizeof offsets/sizeof *offsets;i++){
  initialize();unsigned before=programs;assert(efl_lld_program(&EFLD1,offsets[i],4,(uint8_t*)&value)!=0);assert(programs==before&&!ec_storage_ready());
 }
 for(unsigned i=0;i<count;i++)if(i!=WEAR_LEVELING_EFL_FIRST_SECTOR){initialize();unsigned before=erases;assert(efl_lld_start_erase_sector(&EFLD1,i)!=0&&erases==before);}
 initialize();assert(efl_lld_start_erase_all(&EFLD1)!=0);
 // Active-low write protection and corrupt geometry fail before controller start.
 fake_flash.OPTCR&=~(1U<<(16+WEAR_LEVELING_EFL_FIRST_SECTOR));eeprom_driver_init();assert(!ec_storage_ready());fake_flash.OPTCR|=1U<<(16+WEAR_LEVELING_EFL_FIRST_SECTOR);
 d->sectors_count=0;eeprom_driver_init();assert(!ec_storage_ready());d->sectors_count=count;
 flash_descriptor_t valid=*d;
 for(unsigned fault=0;fault<6;fault++){
  *d=valid;
  if(fault==0)d->size-=16384;
  if(fault==1)d->address++;
  if(fault==2)d->page_size=8;
  if(fault==3)d->attributes=0;
  if(fault==4)d->sectors=(void*)0x20000000;
  if(fault==5)d->sectors_size=16384;
  unsigned before=programs+erases;eeprom_driver_init();assert(!ec_storage_ready()&&!unlocked&&before==programs+erases);
 }
 *d=valid;fake_flash.OPTCR|=0x80000000U;eeprom_driver_init();assert(!ec_storage_ready());fake_flash.OPTCR&=~0x80000000U;
 initialize();program_error=true;uint8_t changed=25;assert(wear_leveling_write(42,&changed,1)==WEAR_LEVELING_FAILED&&!ec_storage_ready());
 initialize();assert(backing_store_unlock());query_error=true;assert(!backing_store_erase()&&!ec_storage_ready());backing_store_lock();
 assert(hash_outside()==outside);
 puts("PASS real QMK NOR journal/replay/consolidation, Vial canary, exact sector isolation, boundary/WRP/geometry/mass-erase rejection and error latching");
}
'''
for board,sector,start,end,total in [('ec_prox',1,0x4000,0x8000,262144),('ec_prox',1,0x4000,0x8000,524288),('me_ec_uf2',7,0x60000,0x80000,524288)]:
 with tempfile.TemporaryDirectory() as temp:
  p=Path(temp);(p/'hal.h').write_text(hal);(p/'timer.h').write_text('#pragma once\n');(p/'main.c').write_text(main)
  (p/'backend.c').write_text('#define backing_store_init a98_qmk_init\n#define backing_store_unlock a98_qmk_unlock\n#include "'+str(repo/'platforms/chibios/drivers/wear_leveling/wear_leveling_efl.c')+'"\n')
  flags=['-std=gnu11','-fno-pie','-no-pie','-DBACKING_STORE_WRITE_SIZE=4','-DSTM32_FLASH_PSIZE=2','-DWEAR_LEVELING_LOGICAL_SIZE=4096',f'-DWEAR_LEVELING_BACKING_SIZE={8192 if sector==1 else end-start}',f'-DWEAR_LEVELING_EFL_FLASH_SIZE={262144 if sector==1 else total}',f'-DHOST_PHYSICAL_FLASH_SIZE={total}',f'-DWEAR_LEVELING_EFL_FIRST_SECTOR={sector}','-DWEAR_LEVELING_EFL_OMIT_LAST_SECTOR_COUNT=0','-DA98_IMAGE_END_SYMBOL=__flash0_free__',f'-DA98_TABLE_START={0x08008000 if sector==1 else 0x08020000}',f'-DA98_STORAGE_START={start}',f'-DA98_STORAGE_END={end}',f'-DA98_APP_START={0x08000000 if sector==1 else 0x08020000}',f'-Wl,--defsym=__flash0_free__={hex(0x0801f000 if sector==1 else 0x08050000)}','-I'+str(p),'-I'+str(runtime),'-I'+str(repo/'quantum'),'-I'+str(repo/'quantum/wear_leveling'),'-I'+str(repo/'lib/fnv'),'-include',str(p/'hal.h')]
  if os.environ.get('SANITIZE')=='1':flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  subprocess.run(['cc',*flags,'-DA98_STORAGE_HOST_TEST',str(p/'main.c'),str(p/'backend.c'),str(repo/'keyboards/alectardy98_ec/ec_prox/storage_diagnostics.c') if board=='ec_prox' else str(runtime/'storage.c'),str(repo/'quantum/wear_leveling/wear_leveling.c'),str(repo/'lib/fnv/hash_64a.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
  print(board,'storage passed')
