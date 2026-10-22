#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile real QMK backend/engine and policy guards against an emulated NOR."""
from pathlib import Path
import subprocess,tempfile,os
kb=(Path(__file__).resolve().parents[3]/'keyboards/alectardy98_ec/class0413_ec');repo=kb.parents[2]
hal=Path(__file__).with_name('flash_hal.h').read_text()
main=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "diag.h"
#include "wear_leveling.h"
#include "wear_leveling_internal.h"
unsigned char diag_arena[1024] __attribute__((aligned(16)));
unsigned char diag_host_flash[131072] __attribute__((aligned(16)));
static unsigned char protected_before[131072];
EFlashDriver EFLD1;
void actuator_storage_quiet(void) {}
uint16_t host_factory_kib=128;
uint32_t host_optcr=0x003caaed;
volatile diag_record_t efl_diag_record;
static bool unlocked,cut_erase,tear_snapshot,erase_error;
static int cut_program=-1;
static unsigned raw_programs,raw_erases,raw_unlocks;
void diag_mark(uint32_t p){efl_diag_record.phase=p;efl_diag_record.milestones|=1U<<p;}
int eflStart(EFlashDriver *p,void*c){(void)c;efl_lld_start(p);if(!unlocked)return -1;p->state=FLASH_READY;return 0;}
void eflStop(EFlashDriver *p){unlocked=false;p->state=FLASH_STOP;}
int flashProgram(BaseFlash*p,uint32_t o,size_t n,const uint8_t*v){return efl_lld_program(p,o,n,v);}
int flashStartEraseSector(BaseFlash*p,unsigned i){return efl_lld_start_erase_sector(p,i);}
int efl_lld_query_erase(void*,uint32_t*);
int flashWaitErase(BaseFlash*p){return efl_lld_query_erase(p,0);}
int s2_hw_query_erase(void*p,uint32_t*m){(void)m;assert(p==&EFLD1);EFLD1.state=FLASH_READY;if(erase_error){erase_error=false;return 11;}return 0;}
void s2_hw_start(EFlashDriver*p){assert(p==&EFLD1);unlocked=true;raw_unlocks++;}
flash_error_t s2_hw_program(void*p,flash_offset_t o,size_t n,const uint8_t*v){
 assert(p==&EFLD1 && unlocked && o>=0x8000 && o+n<=0xc000);
 raw_programs++;
 if(cut_program==0){diag_host_flash[o]&=v[0];cut_program=-1;return 11;}
 if(cut_program>0)cut_program--;
 for(size_t i=0;i<n;i++){assert((diag_host_flash[o+i]&v[i])==v[i]);diag_host_flash[o+i]&=v[i];}
 return 0;
}
flash_error_t s2_hw_erase_sector(void*p,flash_sector_t s){
 assert(p==&EFLD1 && s==2 && unlocked);raw_erases++;
 memset(diag_host_flash+0x8000,255,0x4000);
 if(cut_erase){cut_erase=false;return 11;}
 if(tear_snapshot){tear_snapshot=false;cut_program=1;}
 EFLD1.state=FLASH_ERASE;return 0;
}
static void setup(void){
 memset((void*)&efl_diag_record,0,sizeof efl_diag_record);s2_armed=false;unlocked=false;
 flash_descriptor_t*d=(void*)diag_arena;flash_sector_descriptor_t*s=(void*)(diag_arena+128);
 uint32_t o[]={0,0x4000,0x8000,0xc000,0x10000},n[]={0x4000,0x4000,0x4000,0x4000,0x10000};
 for(unsigned i=0;i<5;i++)s[i]=(flash_sector_descriptor_t){o[i],n[i]};
 *d=(flash_descriptor_t){3,4,5,s,0,(uint8_t*)0x08000000,0x20000};
 EFLD1=(EFlashDriver){d,FLASH,FLASH_STOP};host_factory_kib=128;host_optcr=0x003caaed;
}
static void preserved(void){
 assert(!memcmp(diag_host_flash,protected_before,0x8000));
 assert(!memcmp(diag_host_flash+0xc000,protected_before+0xc000,0x14000));
}
static void blank(void){memset(diag_host_flash+0x8000,255,0x4000);setup();}
static void boot(void){setup();s2_storage_init();}
static void no_init_mutation(void){uint32_t options_before=host_optcr;unsigned p=raw_programs,e=raw_erases,u=raw_unlocks;s2_storage_init();assert(!s2_armed);assert(p==raw_programs&&e==raw_erases&&u==raw_unlocks);assert(host_optcr==options_before);preserved();}
int main(void){
 // Distinct sentinels detect collateral mutation of all protected sectors.
 for(unsigned i=0;i<sizeof diag_host_flash;i++)diag_host_flash[i]=(uint8_t)(i*13U+19U);
 memcpy(protected_before,diag_host_flash,sizeof diag_host_flash);
 blank();unsigned p=raw_programs,e=raw_erases;s2_storage_init();
 assert(s2_armed && efl_diag_record.wl_status==WEAR_LEVELING_SUCCESS);
 assert(efl_diag_record.physical_reads>0 && p==raw_programs && e==raw_erases);
 unsigned char calibration[219],out[219];memset(calibration,0x5a,sizeof calibration);
 assert(wear_leveling_write(512,calibration,sizeof calibration)!=WEAR_LEVELING_FAILED);preserved();
 boot();assert(s2_armed);assert(wear_leveling_read(512,out,sizeof out)!=WEAR_LEVELING_FAILED);assert(!memcmp(out,calibration,sizeof out));
 puts("PASS initialization, physical journal write/replay and 219-byte calibration persistence");
 static uint8_t full[4096],full_out[4096];for(unsigned i=0;i<sizeof full;i++)full[i]=(uint8_t)(i*7+13);
 assert(wear_leveling_write(0,full,sizeof full)!=WEAR_LEVELING_FAILED);boot();assert(s2_armed);
 wear_leveling_read(0,full_out,sizeof full_out);assert(!memcmp(full,full_out,sizeof full));preserved();
 assert(wear_leveling_write(4096,full,1)==WEAR_LEVELING_FAILED);
 assert(wear_leveling_write(4095,full,2)==WEAR_LEVELING_FAILED);
 puts("PASS full 4096-byte logical EEPROM save/replay and logical boundary rejection");
 // Real EC logical region: changing bytes37..255 must preserve every Vial byte.
 assert(wear_leveling_write(37,calibration,sizeof calibration)!=WEAR_LEVELING_FAILED);
 memcpy(full+37,calibration,sizeof calibration);boot();assert(s2_armed);
 wear_leveling_read(0,full_out,sizeof full_out);assert(!memcmp(full,full_out,sizeof full));preserved();
 puts("PASS EC region37..255 update/replay preserves core, Vial keymaps, all macros and other settings");

 // Enough updates for multiple real QMK consolidations and physical sector erases.
 for(unsigned i=0;i<12000;i++){uint8_t v=(uint8_t)i;assert(wear_leveling_write(0,&v,1)!=WEAR_LEVELING_FAILED);}
 assert(raw_erases>0);boot();assert(s2_armed);uint8_t v;wear_leveling_read(0,&v,1);assert(v==(uint8_t)11999);preserved();
 puts("PASS consolidation and reboot; all erases restricted to sector 2");
 // Both valid edge words must be accepted without crossing the selected sector.
 static uint8_t edge_snapshot[0x4000];memcpy(edge_snapshot,diag_host_flash+0x8000,sizeof edge_snapshot);
 boot();assert(backing_store_unlock());uint32_t zero=0;
 assert(efl_lld_program(&EFLD1,0x8000,4,(void*)&zero)==0);
 assert(efl_lld_program(&EFLD1,0xbffc,4,(void*)&zero)==0);
 backing_store_lock();preserved();assert(host_optcr==0x003caaed);
 memcpy(diag_host_flash+0x8000,edge_snapshot,sizeof edge_snapshot);
 // Live protection change is read back and rejects before programming.
 boot();assert(backing_store_unlock());p=raw_programs;host_optcr&=~0x00040000U;
 assert(efl_lld_program(&EFLD1,0x8000,4,(void*)&zero)!=0);assert(p==raw_programs && !s2_armed);
 puts("PASS both sector-2 edge words and live WRP rejection; option bits untouched");
 // Invalid requests must not reach any actual hardware operation.
 uint32_t word=0;const uint32_t bad[]={0,0x4000,0x7ffc,0x8001,0xc000,0x10000,0xfffffffc};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++){boot();assert(backing_store_unlock());p=raw_programs;assert(efl_lld_program(&EFLD1,bad[i],4,(void*)&word)!=0);assert(p==raw_programs&&!s2_armed);}
 boot();assert(backing_store_unlock());p=raw_programs;assert(efl_lld_program(&EFLD1,0xbffc,8,(void*)&word)!=0);assert(p==raw_programs);
 boot();assert(backing_store_unlock());p=raw_programs;assert(efl_lld_program(&EFLD1,0x8000,(size_t)-4,(void*)&word)!=0);assert(p==raw_programs);
 for(unsigned i=0;i<6;i++)if(i!=2){boot();assert(backing_store_unlock());e=raw_erases;assert(efl_lld_start_erase_sector(&EFLD1,i)!=0);assert(e==raw_erases);}
 boot();assert(backing_store_unlock());e=raw_erases;assert(efl_lld_start_erase_all(&EFLD1)!=0);assert(e==raw_erases);
 boot();assert(backing_store_read(0x3ffc,&word));assert(!backing_store_read(0x4000,&word));assert(!backing_store_read(1,&word));
 puts("PASS program/read bounds, overflow, unaligned access, mass erase and protected-sector rejection");
 // Every bit of every descriptor field is checked before any destructive operation.
 for(unsigned sector=0;sector<5;sector++)for(unsigned bit=0;bit<32;bit++){
  blank();((flash_sector_descriptor_t*)(diag_arena+128))[sector].offset^=1U<<bit;no_init_mutation();
  blank();((flash_sector_descriptor_t*)(diag_arena+128))[sector].size^=1U<<bit;no_init_mutation();
 }
 puts("PASS all320 descriptor offset/size bit mutations reject before unlock/program/erase");
 blank();EFLD1.descriptor=0;no_init_mutation();
 blank();((flash_descriptor_t*)EFLD1.descriptor)->size=0x40000;no_init_mutation();
 blank();((flash_sector_descriptor_t*)(diag_arena+128))[2].offset=0xc000;no_init_mutation();
 blank();host_factory_kib=256;no_init_mutation();
 // Invalid journal cannot trigger a destructive boot-time repair.
 blank();memset(diag_host_flash+0x8000+4104,0,4);no_init_mutation();
 blank();host_optcr&=~0x00040000U;no_init_mutation();assert(efl_diag_record.error==11);
 blank();host_optcr|=0x80000000U;no_init_mutation();assert(efl_diag_record.error==11);
 blank();host_optcr|=0x00010000U;no_init_mutation();assert(efl_diag_record.error==11);
 blank();host_optcr|=0x00020000U;no_init_mutation();assert(efl_diag_record.error==11);
 puts("PASS read-only WRP/SPRMOD checks reject protected S2 or unexpected S0/S1 protection");
 puts("PASS invalid geometry/density and malformed journal fail closed without init writes");
 // Interrupt each word of a calibration-sized append, including a torn word.
 static unsigned char snapshot[0x4000];
 blank();s2_storage_init();v=0x33;assert(wear_leveling_write(0,&v,1)!=WEAR_LEVELING_FAILED);
 memcpy(snapshot,diag_host_flash+0x8000,sizeof snapshot);
 for(int cut=0;cut<100;cut++){
  memcpy(diag_host_flash+0x8000,snapshot,sizeof snapshot);boot();assert(s2_armed);
  cut_program=cut;wear_leveling_status_t r=wear_leveling_write(512,calibration,sizeof calibration);cut_program=-1;
  if(r==WEAR_LEVELING_FAILED){assert(!s2_armed);p=raw_programs;v=0x44;wear_leveling_write(0,&v,1);assert(raw_programs==p);}
  boot();preserved(); // Data recovery is deliberately not assumed atomic.
 }
 puts("PASS 100 interrupted append positions; protected sectors unchanged, errors latch writes off");
 // Cut after sector erase during consolidation: standard QMK can lose settings.
 blank();s2_storage_init();cut_erase=true;
 for(unsigned i=0;i<10000;i++){v=(uint8_t)i;if(wear_leveling_write(0,&v,1)==WEAR_LEVELING_FAILED)break;}
 assert(!cut_erase && !s2_armed);boot();assert(s2_armed);wear_leveling_read(0,&v,1);assert(v==0);preserved();
 puts("PASS interrupted consolidation isolation; demonstrated documented data-loss window");
 blank();s2_storage_init();tear_snapshot=true;
 for(unsigned i=0;i<10000;i++){v=(uint8_t)i;if(wear_leveling_write(0,&v,1)==WEAR_LEVELING_FAILED)break;}
 cut_program=-1;assert(!tear_snapshot && !s2_armed);e=raw_erases;p=raw_programs;boot();assert(s2_armed);assert(e==raw_erases && p==raw_programs);wear_leveling_read(0,&v,1);assert(v==0);preserved();
 puts("PASS interrupted snapshot follows standard QMK checksum/default fallback without init erase");
 blank();s2_storage_init();assert(backing_store_unlock());erase_error=true;
 assert(!backing_store_erase());assert(!s2_armed && efl_diag_record.flash_errors);backing_store_lock();preserved();
 puts("PASS erase-completion error disables further writes");
}
'''
with tempfile.TemporaryDirectory(prefix='s2-persist-test-') as temp:
 p=Path(temp);(p/'hal.h').write_text(hal);(p/'timer.h').write_text('#pragma once\n');(p/'test.c').write_text(main)
 (p/'backend.c').write_text('#define backing_store_init s2_qmk_init\n#define backing_store_read s2_qmk_read\n#define backing_store_unlock s2_qmk_unlock\n#include "'+str(repo/'platforms/chibios/drivers/wear_leveling/wear_leveling_efl.c')+'"\n')
 flags=['-std=gnu11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-DDIAG_HOST_TEST','-DBACKING_STORE_WRITE_SIZE=4','-DWEAR_LEVELING_BACKING_SIZE=16384','-DWEAR_LEVELING_LOGICAL_SIZE=4096','-DWEAR_LEVELING_EFL_FLASH_SIZE=131072','-DWEAR_LEVELING_EFL_FIRST_SECTOR=2','-DWEAR_LEVELING_EFL_OMIT_LAST_SECTOR_COUNT=2','-DDIAG_TABLE_START=((uintptr_t)diag_arena)','-DDIAG_TABLE_END=((uintptr_t)(diag_arena+1024))','-I'+str(p),'-I'+str(kb),'-I'+str(kb/'modules'),'-I'+str(repo/'quantum'),'-I'+str(repo/'quantum/wear_leveling'),'-I'+str(repo/'lib/fnv'),'-include',str(p/'hal.h')]
 if os.environ.get('SANITIZE')=='1': flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 sources=[p/'test.c',p/'backend.c',kb/'modules/geometry.c',kb/'modules/storage_guards.c',repo/'quantum/wear_leveling/wear_leveling.c',repo/'lib/fnv/hash_64a.c']
 subprocess.run(['cc',*flags,*map(str,sources),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
