/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_lock_store.h"
#include "main_persist.h"
#include <string.h>
#define RECORD 96u
#define RECORDS ((MY2K_LOCK_FLASH_END-MY2K_LOCK_FLASH_BASE)/RECORD)
#define MAGIC UINT32_C(0x314a534c) /* LSJ1 */
#define COMMIT UINT32_C(0x314b434c)
static _Alignas(4) uint8_t committed[76],record[RECORD];
static uint32_t generation,cursor,error,commits;
static unsigned next_slot;
static bool valid,pending;
static uint32_t get32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
#ifdef MY2K_PERSIST_HOST
extern bool my2k_test_flash_read(uint32_t,void *,size_t);
extern bool my2k_test_flash_word(uint32_t,uint32_t);
#define read_flash my2k_test_flash_read
#define write_word my2k_test_flash_word
#else
static bool read_flash(uint32_t a,void *p,size_t n){if(a<MY2K_LOCK_FLASH_BASE||a>MY2K_LOCK_FLASH_END||n>MY2K_LOCK_FLASH_END-a)return false;memcpy(p,(const void*)(uintptr_t)a,n);return true;}
static bool write_word(uint32_t a,uint32_t v){return a>=MY2K_LOCK_FLASH_BASE&&a<=MY2K_LOCK_FLASH_END-4&&!(a&3)&&my2k_settings_flash_operation(a,v,false);}
#endif
static uint32_t record_crc(const uint8_t *p){uint8_t b[88];memcpy(b,p,12);memcpy(b+12,p+16,76);return my2k_lock_crc(b,sizeof b);}
static bool empty(const uint8_t*p){for(unsigned i=0;i<RECORD;i++)if(p[i]!=255)return false;return true;}
static void find_slot(unsigned from){
 uint8_t b[RECORD];next_slot=RECORDS;
 for(unsigned i=from;i<RECORDS;i++){if(!read_flash(MY2K_LOCK_FLASH_BASE+i*RECORD,b,RECORD)){error=1;return;}if(empty(b)){next_slot=i;return;}}
}
void my2k_lock_store_init(void){
 uint8_t b[RECORD];valid=pending=false;error=commits=generation=cursor=0;next_slot=RECORDS;
 for(unsigned i=0;i<RECORDS;i++){
  if(!read_flash(MY2K_LOCK_FLASH_BASE+i*RECORD,b,RECORD)){error=1;return;}
  if(empty(b)){if(next_slot==RECORDS)next_slot=i;continue;}
  if(get32(b)!=MAGIC||get32(b+8)!=76||get32(b+92)!=COMMIT||get32(b+12)!=record_crc(b)||!my2k_lock_validate(b+16))continue;
  uint32_t g=get32(b+4);
  if(!valid||(g!=generation&&(uint32_t)(g-generation)<UINT32_C(0x80000000))){memcpy(committed,b+16,76);generation=g;valid=true;}
 }
}
bool my2k_lock_store_load(uint8_t*p){if(!p||!valid)return false;memcpy(p,committed,76);return true;}
bool my2k_lock_store_request(const uint8_t*p){
 if(!p||pending||error||!my2k_lock_validate(p)||my2k_persist_error())return false;
 if(valid&&!memcmp(p,committed,76))return true;
 if(next_slot==RECORDS)return false; /* Never erase EEPROM merely to save rules. */
 memset(record,255,sizeof record);put32(record,MAGIC);put32(record+4,valid?generation+1:0);put32(record+8,76);memcpy(record+16,p,76);put32(record+12,record_crc(record));put32(record+92,COMMIT);cursor=0;pending=true;return true;
}
void my2k_lock_store_task(void){
 if(!pending||error||my2k_persist_busy())return;
 if(my2k_persist_error()){error=3;pending=false;return;}
 for(unsigned budget=0;budget<4&&cursor<RECORD;budget++,cursor+=4){
  uint32_t a=MY2K_LOCK_FLASH_BASE+next_slot*RECORD+cursor,v=get32(record+cursor),check;
  if(next_slot>=RECORDS||a<MY2K_LOCK_FLASH_BASE||a>MY2K_LOCK_FLASH_END-4||!write_word(a,v)||!read_flash(a,&check,4)||check!=v){error=2;pending=false;my2k_persist_extension_fault();return;}
 }
 if(cursor==RECORD){memcpy(committed,record+16,76);generation=get32(record+4);valid=true;pending=false;commits++;find_slot(next_slot+1);}
}
/* Called only after the EXISTING EEPROM owner's sector4 erase. Cached rules
 * are restored asynchronously, including a transaction interrupted by that
 * erase. No new erase path, sector selection, or controller driver is added. */
void my2k_lock_store_erased(void){
 next_slot=0;
 if(pending){cursor=0;return;}
 if(valid&&!error){uint8_t saved[76];memcpy(saved,committed,76);valid=false;(void)my2k_lock_store_request(saved);}
}
bool my2k_lock_store_pending(void){return pending;}
uint32_t my2k_lock_store_error(void){return error;}
uint16_t my2k_lock_store_remaining(void){return RECORDS-next_slot;}
uint32_t my2k_lock_store_commits(void){return commits;}
