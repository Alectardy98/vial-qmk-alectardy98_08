// SPDX-License-Identifier: GPL-2.0-or-later
// Standard QMK journal and ChibiOS flash implementation behind strict policy.
#include <hal.h>
#include "wear_leveling.h"
#include "wear_leveling_internal.h"
#include "diag.h"
_Static_assert(BACKING_STORE_WRITE_SIZE==4 && (1U<<STM32_FLASH_PSIZE)==4,"F4 flash line mismatch");
_Static_assert(WEAR_LEVELING_LOGICAL_SIZE==4096,"Logical EEPROM ABI");
_Static_assert(A98_STORAGE_END>A98_STORAGE_START && A98_STORAGE_END-A98_STORAGE_START>=WEAR_LEVELING_BACKING_SIZE,"Backing must fit its guarded erase sector");
static bool healthy;
extern const uint8_t A98_IMAGE_END_SYMBOL[];
bool a98_qmk_init(void);
bool a98_qmk_unlock(void);
void a98_hw_start(EFlashDriver *);
flash_error_t a98_hw_program(void *,flash_offset_t,size_t,const uint8_t *);
flash_error_t a98_hw_erase(void *,flash_sector_t);
uint16_t a98_storage_density(void){return *(volatile const uint16_t *)(uintptr_t)0x1fff7a22U;}
static bool geometry(void){
 const flash_descriptor_t *d=EFLD1.descriptor;
 uintptr_t p=(uintptr_t)d,end=(uintptr_t)A98_IMAGE_END_SYMBOL;
 if(p<A98_TABLE_START||p>end||sizeof(*d)>end-p||p%4)return false;
 uint32_t density=a98_storage_density();
 if(EFLD1.flash!=FLASH||d->address!=(uint8_t*)0x08000000U||density*1024U!=d->size||end>0x08000000U+d->size)return false;
 if(!(d->attributes&FLASH_ATTR_ERASED_IS_ONE)||!(d->attributes&FLASH_ATTR_MEMORY_MAPPED)||d->page_size!=4||d->sectors_size||!d->sectors)return false;
 p=(uintptr_t)d->sectors;if(p<A98_TABLE_START||p>end||p%4||d->sectors_count>12||d->sectors_count<5||d->sectors_count*sizeof(*d->sectors)>end-p)return false;
 for(unsigned i=0;i<d->sectors_count;i++){
  unsigned offset=i<4?i*0x4000U:i==4?0x10000U:0x20000U+(i-5)*0x20000U;
  unsigned size=i<4?0x4000U:i==4?0x10000U:0x20000U;
  if(d->sectors[i].offset!=offset||d->sectors[i].size!=size||offset>d->size||size>d->size-offset)return false;
 }
 unsigned s=WEAR_LEVELING_EFL_FIRST_SECTOR;
 if(s>=d->sectors_count||d->sectors[s].offset!=A98_STORAGE_START||d->sectors[s].size!=A98_STORAGE_END-A98_STORAGE_START)return false;
 // Active-low normal WRP. No option-byte unlock or write exists here.
 return !(FLASH->OPTCR&0x80000000U)&&(FLASH->OPTCR&(1U<<(16+s)));
}
static flash_error_t reject(void){healthy=false;return FLASH_ERROR_UNIMPLEMENTED;}
static bool ready(void *p){return healthy&&p==&EFLD1&&geometry();}
bool backing_store_init(void){healthy=false;if(!geometry()||!a98_qmk_init())return false;healthy=true;return true;}
void eeprom_driver_init(void){
 if(wear_leveling_init()==WEAR_LEVELING_FAILED){
  healthy=false;
  if(EFLD1.flash==FLASH&&(EFLD1.state==FLASH_STOP||EFLD1.state==FLASH_READY))eflStop(&EFLD1);
 }
}
bool ec_storage_ready(void){return healthy;}
bool backing_store_unlock(void){
 if(!ready(&EFLD1)||(EFLD1.state!=FLASH_STOP&&EFLD1.state!=FLASH_READY)){reject();return false;}
 return a98_qmk_unlock()&&healthy;
}
__attribute__((noinline,noclone)) void efl_lld_start(EFlashDriver *p){if(!ready(p)||(p->state!=FLASH_STOP&&p->state!=FLASH_READY)){reject();return;}a98_hw_start(p);}
__attribute__((noinline,noclone)) flash_error_t efl_lld_program(void *p,flash_offset_t offset,size_t n,const uint8_t *data){
 if(!ready(p)||EFLD1.state!=FLASH_READY||!data||offset<A98_STORAGE_START||offset>=A98_STORAGE_END||offset%4||!n||n%4||n>A98_STORAGE_END-offset)return reject();
 flash_error_t result=a98_hw_program(p,offset,n,data);if(result!=FLASH_NO_ERROR)healthy=false;return result;
}
__attribute__((noinline,noclone)) flash_error_t efl_lld_start_erase_sector(void *p,flash_sector_t sector){
 if(!ready(p)||EFLD1.state!=FLASH_READY||sector!=WEAR_LEVELING_EFL_FIRST_SECTOR)return reject();
 flash_error_t result=a98_hw_erase(p,sector);if(result!=FLASH_NO_ERROR)healthy=false;return result;
}
__attribute__((noinline,noclone)) flash_error_t efl_lld_start_erase_all(void *p){(void)p;return reject();}

flash_error_t a98_hw_query_erase(void *,uint32_t *);
__attribute__((noinline,noclone)) flash_error_t efl_lld_query_erase(void *p,uint32_t *msec){
 if(p!=&EFLD1||EFLD1.flash!=FLASH)return reject();
 flash_error_t result=a98_hw_query_erase(p,msec);
 if(result!=FLASH_NO_ERROR&&result!=FLASH_BUSY_ERASING)healthy=false;
 return result;
}
