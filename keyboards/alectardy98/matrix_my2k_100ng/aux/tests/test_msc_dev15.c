/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aux_platform.h"
#include "aux_msc.h"
#include "aux_msc_limits.h"
#include "tusb.h"
static unsigned reads,fail_at;
static bool cancel_requeue;
static uint8_t newer[AUX_MSC_READ_MAX_BYTES];
volatile uint32_t aux_fault_code;
extern void tud_msc_inquiry_cb(uint8_t,uint8_t[8],uint8_t[16],uint8_t[4]);
uint32_t aux_sd_sectors(void){return 32;}
bool aux_sd_read(uint32_t lba,uint8_t block[512]){
 reads++;
 if(cancel_requeue){
  cancel_requeue=false;aux_msc_cancel();
  assert(aux_msc_queue(0,16,0,newer,sizeof newer)==TUD_MSC_RET_ASYNC);
 }
 if(lba>=32||(fail_at&&reads==fail_at))return false;
 for(unsigned i=0;i<512;i++)block[i]=(uint8_t)(lba*37u+i*3u);
 return true;
}
bool tud_msc_set_sense(uint8_t lun,uint8_t key,uint8_t asc,uint8_t ascq){(void)lun;(void)key;(void)asc;(void)ascq;return true;}
static void check(const uint8_t *bytes,uint32_t lba,uint32_t offset,uint32_t size){
 for(unsigned i=0;i<size;i++){
  unsigned at=offset+i;assert(bytes[i]==(uint8_t)((lba+at/512u)*37u+(at%512u)*3u));
 }
}
int main(void){
 _Static_assert(AUX_MSC_READ_MAX_BYTES==4096u,"D015 capability must agree with tests");
 _Static_assert(CFG_TUD_MSC_EP_BUFSIZE==AUX_MSC_READ_MAX_BYTES,"TinyUSB callback bound");
 uint8_t guard[AUX_MSC_READ_MAX_BYTES+32];int32_t n;
 uint8_t *b=guard+16;
 memset(guard,0xa5,sizeof guard);
 assert(aux_msc_queue(0,2,0,b,AUX_MSC_READ_MAX_BYTES)==TUD_MSC_RET_ASYNC);
 assert(reads==0&&!aux_msc_take_completion(&n));
 assert(aux_msc_queue(0,3,0,b,512)==TUD_MSC_RET_ERROR);
 aux_msc_worker_service();assert(reads==8&&b[0]==0xa5);
 assert(aux_msc_take_completion(&n)&&n==4096);check(b,2,0,4096);
 for(unsigned i=0;i<16;i++)assert(guard[i]==0xa5&&guard[4112+i]==0xa5);
 assert(!aux_msc_take_completion(&n));
 /* Every alignment/length combination near sector and transfer bounds. */
 for(unsigned offset=0;offset<512;offset+=73){
  const unsigned sizes[]={1,511,512,513,4096};
  for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++){
   assert(aux_msc_queue(0,1,offset,b,sizes[i])==TUD_MSC_RET_ASYNC);
   aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==(int32_t)sizes[i]);
   check(b,1,offset,sizes[i]);
  }
 }
 unsigned before=reads;
 assert(aux_msc_queue(0,0,0,b,4097)==TUD_MSC_RET_ERROR);
 assert(aux_msc_queue(0,25,0,b,4096)==TUD_MSC_RET_ERROR);
 assert(aux_msc_queue(0,24,1,b,4096)==TUD_MSC_RET_ERROR);
 assert(aux_msc_queue(0,0,512,b,1)==TUD_MSC_RET_ERROR);
 assert(aux_msc_queue(1,0,0,b,1)==TUD_MSC_RET_ERROR);
 assert(reads==before);
 assert(aux_msc_queue(0,24,0,b,4096)==TUD_MSC_RET_ASYNC);
 aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==4096);check(b,24,0,4096);
 /* Fail after an earlier sector succeeded: borrowed USB buffer untouched. */
 memset(b,0x7b,4096);fail_at=reads+4;
 assert(aux_msc_queue(0,0,0,b,4096)==TUD_MSC_RET_ASYNC);
 aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==-1);fail_at=0;
 for(unsigned i=0;i<4096;i++)assert(b[i]==0x7b);
 /* A reset may queue a new command while the old worker yields. Its static
  * staging is private; stale completion cannot overwrite either USB buffer. */
 memset(b,0x6c,4096);memset(newer,0x3d,sizeof newer);cancel_requeue=true;
 assert(aux_msc_queue(0,0,0,b,4096)==TUD_MSC_RET_ASYNC);
 aux_msc_worker_service();assert(!aux_msc_take_completion(&n));
 for(unsigned i=0;i<4096;i++)assert(b[i]==0x6c&&newer[i]==0x3d);
 aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==4096);check(newer,16,0,4096);
 before=reads;assert(aux_msc_write(0,0,0,b,4096)==-1);
 assert(aux_msc_queue_write(0,0,0,b,4096)==-1);
 assert(tud_msc_write10_cb(0,0,0,b,4096)==-1&&!tud_msc_is_writable_cb(0));assert(reads==before);
 uint8_t vendor[8],product[16],revision[4];tud_msc_inquiry_cb(0,vendor,product,revision);
 assert(!memcmp(vendor,"MY2K-EXP",8)&&!memcmp(product,"Read-only assets",16)&&!memcmp(revision,"D015",4));
 assert(aux_msc_io_idle());
 puts("D0154KiB reads/alignment/bounds/failure atomicity/reset epochs/write rejection/identity: PASS");
 return 0;
}
