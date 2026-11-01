/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aux_platform.h"
#include "aux_msc.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
static unsigned reads;static bool fail;static uint8_t sense;
static bool cancel_during_read,requeue_during_read;
static uint8_t fresh[512];
volatile uint32_t aux_fault_code;
static osal_task_func_t deferred_function;static void *deferred_parameter;
static unsigned completed_calls,reset_calls;static int32_t completed_bytes;
extern void aux_msc_defer(osal_task_func_t,void *,bool);
void aux_mscd_reset_native(uint8_t rhport){(void)rhport;reset_calls++;}
bool aux_mscd_control_native(uint8_t rhport,uint8_t stage,const tusb_control_request_t *r){(void)rhport;(void)stage;(void)r;return true;}
void usbd_defer_func(osal_task_func_t fn,void *arg,bool in_isr){assert(!in_isr&&!deferred_function);deferred_function=fn;deferred_parameter=arg;}
static void drain(void){assert(deferred_function);osal_task_func_t fn=deferred_function;deferred_function=NULL;fn(deferred_parameter);}
static void completed(void *arg){completed_calls++;completed_bytes=(int32_t)(intptr_t)arg;}

uint32_t aux_sd_sectors(void){return 4;}
bool aux_sd_read(uint32_t lba,uint8_t block[512]){reads++;
 if(cancel_during_read){cancel_during_read=false;aux_msc_cancel();
  if(requeue_during_read){requeue_during_read=false;assert(aux_msc_queue(0,2,0,fresh,512)==-2);}
 }
 if(fail||lba>=4)return false;
 for(unsigned i=0;i<512;i++)block[i]=(uint8_t)(lba+i);
 return true;
}
bool tud_msc_set_sense(uint8_t lun,uint8_t key,uint8_t asc,uint8_t ascq){(void)lun;(void)asc;(void)ascq;sense=key;return true;}
int main(void){uint8_t b[600];
 assert(aux_msc_read(1,0,0,b,1)==-1);assert(aux_msc_read(0,4,0,b,1)==-1);assert(aux_msc_read(0,3,511,b,2)==-1);assert(aux_msc_read(0,0,512,b,1)==-1);assert(reads==0);
 memset(b,0xcc,sizeof b);int32_t n;
 assert(tud_msc_read10_cb(0,1,500,b,24)==TUD_MSC_RET_ASYNC&&reads==0);
 assert(!aux_msc_take_completion(&n)&&b[0]==0xcc);
 aux_msc_worker_service();assert(reads==2&&b[0]==0xcc);
 assert(aux_msc_take_completion(&n)&&n==24);
 for(unsigned i=0;i<12;i++)assert(b[i]==(uint8_t)(501+i));
 for(unsigned i=12;i<24;i++)assert(b[i]==(uint8_t)(2+i-12));
 uint32_t count;uint16_t size;tud_msc_capacity_cb(0,&count,&size);assert(count==4&&size==512);
 assert(!tud_msc_is_writable_cb(0));unsigned before=reads;
 assert(tud_msc_write10_cb(0,0,0,b,512)==-1&&sense==SCSI_SENSE_DATA_PROTECT&&reads==before);
 fail=true;assert(tud_msc_read10_cb(0,0,0,b,512)==-2);
 aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==-1);fail=false;
 /* Cancel a live read at the same seam as a worker yield, then enqueue a new
  * CBW using the reused USB buffer. Stale data never reaches either buffer. */
 memset(b,0xa5,sizeof b);memset(fresh,0x5a,sizeof fresh);
 assert(tud_msc_read10_cb(0,0,0,b,512)==-2);
 cancel_during_read=requeue_during_read=true;aux_msc_worker_service();
 assert(!aux_msc_take_completion(&n)&&b[0]==0xa5&&fresh[0]==0x5a);
 aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==512&&fresh[0]==2&&b[0]==0xa5);
 /* Bus reset, BOT reset, and a reset between completion enqueue and the
  * TinyUSB deferred function must cancel without completing a fresh CBW. */
 assert(tud_msc_read10_cb(0,0,0,b,512)==-2);mscd_reset(1);
 aux_msc_worker_service();assert(!aux_msc_take_completion(&n)&&reset_calls==1);
 tusb_control_request_t r={.bmRequestType=0x21,.bRequest=MSC_REQ_RESET};
 assert(tud_msc_read10_cb(0,0,0,b,512)==-2);
 assert(mscd_control_xfer_cb(1,CONTROL_STAGE_SETUP,&r));
 aux_msc_worker_service();assert(!aux_msc_take_completion(&n));
 aux_msc_defer(completed,(void*)(intptr_t)512,false);mscd_reset(1);drain();assert(completed_calls==0);
 aux_msc_defer(completed,(void*)(intptr_t)17,false);drain();assert(completed_calls==1&&completed_bytes==17);
 assert(!aux_fault_code);
 uint8_t const *desc=tud_descriptor_device_cb();assert(desc[0]==18&&desc[1]==1);
 const uint8_t *conf=tud_descriptor_configuration_cb(0);assert(conf&&conf[4]==1&&conf[14]==8&&conf[15]==6&&conf[16]==0x50);
 puts("AUX MSC async reads/active-reset cancellation/deferred epochs/bounds/write rejection/descriptors: PASS");return 0;}
