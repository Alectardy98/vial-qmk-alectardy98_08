/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_eeprom.h"
#include "main_nv32.h"
#include "main_flash32.h"
#include "main_persist.h"
#include "eeprom_driver.h"
#include "timer.h"
#include "keyboard.h"
#include <string.h>
static my2k_nv32 store;
static uint32_t changed_at;
static bool attached,servicing;
enum { RESET_IDLE,RESET_QUEUED,RESET_CLEARING,RESET_DURABLE,RESET_FAILED };
static uint8_t reset_state;
bool my2k_eeprom32_reset_pending(void){return reset_state>=RESET_QUEUED&&reset_state<=RESET_DURABLE;}
bool my2k_eeprom32_reset_ready(void){return reset_state==RESET_DURABLE&&!store.error&&!my2k_nv_pending(&store);}
bool my2k_eeprom32_reset_request(void){
 if(!attached||store.error||reset_state==RESET_FAILED)return false;
 if(reset_state==RESET_IDLE)reset_state=RESET_QUEUED;
 return true;
}
static void reset_failed(void){
 /* Discard incomplete RAM state, retain the last committed page addresses.
  * Keep the IO fault latched: no retry, erase or automatic restart on error. */
 memcpy(store.map,store.durable,sizeof store.map);memset(store.cache,0,sizeof store.cache);
 store.action=0;store.tx=store.reset=store.rotating=false;
 reset_state=RESET_FAILED;servicing=false;
}
__attribute__((noinline,noclone,externally_visible))
void eeprom_driver_init(void){attached=my2k_nv_init(&store,(my2k_nv_io){my2k_flash32_read,my2k_flash32_word,my2k_flash32_erase});changed_at=0;servicing=false;reset_state=RESET_IDLE;}
void eeprom_driver_erase(void){if(attached&&!my2k_eeprom32_reset_pending())(void)my2k_nv_clear(&store);changed_at=timer_read32();}
void eeprom_driver_format(bool erase){if(erase)eeprom_driver_erase();}
void eeprom_read_block(void*out,const void*address,size_t n){
 if(!out||!n)return;
 memset(out,0,n);uintptr_t at=(uintptr_t)address;if(!attached||at>=MY2K_NV_BYTES)return;
 if(n>MY2K_NV_BYTES-at)n=MY2K_NV_BYTES-at;
 (void)my2k_nv_read(&store,(uint32_t)at,out,n);
}
void eeprom_write_block(const void*data,void*address,size_t n){
 uintptr_t at=(uintptr_t)address;if(!attached||my2k_eeprom32_reset_pending()||!data||!n||at>=MY2K_NV_BYTES)return;
 if(n>MY2K_NV_BYTES-at)n=MY2K_NV_BYTES-at;
 (void)my2k_nv_write(&store,(uint32_t)at,data,n);changed_at=timer_read32();
}
void my2k_eeprom_task(uint32_t now){
 if(my2k_eeprom32_reset_pending()){
  if(!attached||store.error){reset_failed();return;}
  if(reset_state==RESET_QUEUED){
   if(!my2k_nv_clear(&store)){reset_failed();return;}
   reset_state=RESET_CLEARING;
  }
  /* Intentional factory reset bypasses typing/settling gates. Foreground only,
   * bounded normal service; never restart with a pending or failed commit. */
  if(!my2k_nv_service(&store,4)){reset_failed();return;}
  if(!my2k_nv_pending(&store))reset_state=RESET_DURABLE;
  return;
 }
 if(!attached||store.error||!my2k_nv_pending(&store))return;
 if(last_input_activity_elapsed()<1000)return;
 if(!servicing&&(uint32_t)(now-changed_at)<750)return;
 servicing=true;(void)my2k_nv_service(&store,4);if(!my2k_nv_pending(&store))servicing=false;
}
bool my2k_eeprom_durable(void){return attached&&!store.error;}
bool my2k_eeprom_pending(void){return my2k_nv_pending(&store);}
my2k_journal_result my2k_eeprom_last_result(void){return store.error?MY2K_JOURNAL_IO:my2k_nv_pending(&store)?MY2K_JOURNAL_EMPTY:MY2K_JOURNAL_OK;}
bool my2k_persist_busy(void){return my2k_nv_pending(&store);}
uint32_t my2k_persist_commits(void){return store.commits;}
uint32_t my2k_persist_erases(void){return store.erases;}
uint32_t my2k_persist_error(void){return store.error;}
void my2k_persist_extension_fault(void){if(!store.error)store.error=7;}
/* Read-only board-local diagnostics, no flash write/reset command. */
bool my2k_eeprom32_hid(uint8_t*p,uint8_t n){
 if(!p||n!=32||p[0]!=253||p[1]!=234)return false;
 uint8_t op=p[2],version=p[3];memset(p,0,32);p[0]=253;p[1]=234;p[2]=0x55;p[3]=1;p[4]=op;
 if(op>1||version!=1)return true;
 if(op==1){
  p[2]=0xaa;p[5]=reset_state;p[6]=my2k_eeprom32_reset_pending();p[7]=my2k_eeprom32_reset_ready();
  for(unsigned j=0;j<4;j++)p[8+j]=(uint8_t)(store.error>>(8*j));
  return true;
 }
 p[2]=0xaa;p[5]=32;p[6]=store.bank<0?255:(uint8_t)(10+store.bank);p[7]=my2k_nv_pending(&store);
 uint32_t values[6]={store.error,store.commits,store.erases,store.words,store.evictions,store.generation};
 for(unsigned i=0;i<6;i++)for(unsigned j=0;j<4;j++)p[8+i*4+j]=(uint8_t)(values[i]>>(8*j));
 return true;
}
