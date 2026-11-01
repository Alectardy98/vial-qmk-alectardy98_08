/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_msc.h"
#include "aux_platform.h"
#include "tusb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t media[4][512],sense;static unsigned reads,writes;static bool fail_write,cancel_write,requeue_write;static uint8_t new_source[512];
extern bool tud_msc_is_writable_cb(uint8_t);
uint32_t aux_sd_sectors(void){return 4;}
bool aux_sd_read(uint32_t lba,uint8_t data[512]){reads++;if(lba>=4)return false;memcpy(data,media[lba],512);return true;}
bool aux_sd_write(uint32_t lba,const uint8_t data[512]){
 writes++;assert(lba<4);memcpy(media[lba],data,fail_write?4:512);
 if(cancel_write){cancel_write=false;aux_msc_cancel();if(requeue_write){requeue_write=false;assert(aux_msc_queue_write(0,3,0,new_source,512)==-2);}}
 return !fail_write;
}
bool tud_msc_set_sense(uint8_t lun,uint8_t key,uint8_t asc,uint8_t ascq){(void)lun;(void)asc;(void)ascq;sense=key;return true;}
static bool sync_media(void){uint8_t cmd[16]={0x35};return tud_msc_scsi_cb(0,cmd,NULL,0)==0;}
int main(void){
 for(unsigned l=0;l<4;l++)for(unsigned i=0;i<512;i++)media[l][i]=(uint8_t)(l+i);
 uint8_t source[512],expected[512];for(unsigned i=0;i<512;i++)source[i]=expected[i]=(uint8_t)(i^0xa5);
 assert(tud_msc_is_writable_cb(0)&&!tud_msc_is_writable_cb(1));
 unsigned before=writes;assert(tud_msc_write10_cb(1,0,0,source,512)==-1 && writes==before);assert(tud_msc_write10_cb(0,3,500,source,24)==-1 && writes==before);
 uint32_t gen=aux_msc_media_generation();assert(tud_msc_write10_cb(0,1,0,source,512)==-2 && aux_msc_media_writing() && writes==before);assert(!sync_media());
 memset(source,0xcc,512);aux_msc_worker_service();assert(!memcmp(media[1],expected,512));int32_t n;assert(aux_msc_take_completion(&n)&&n==512);assert(source[0]==0xcc);
 assert(aux_msc_media_generation()==gen && sync_media() && !aux_msc_media_writing() && aux_msc_media_generation()==gen+1);
 uint8_t old[512];memcpy(old,media[2],512);memset(source,0x17,4);before=reads;assert(tud_msc_write10_cb(0,2,10,source,4)==-2);aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==4 && reads==before+1);assert(!memcmp(media[2],old,10)&&!memcmp(media[2]+10,source,4)&&!memcmp(media[2]+14,old+14,498));assert(sync_media());
 /* Old worker stack owns write bytes despite reset+newUSBrequest reusing globals. */
 memset(source,0x42,512);memset(new_source,0x73,512);gen=aux_msc_media_generation();assert(tud_msc_write10_cb(0,0,0,source,512)==-2);cancel_write=requeue_write=true;aux_msc_worker_service();assert(!aux_msc_take_completion(&n)&&media[0][0]==0x42 && aux_msc_media_writing());aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==512 && media[3][0]==0x73);assert(sync_media()&&aux_msc_media_generation()==gen+1);
 fail_write=true;assert(tud_msc_write10_cb(0,0,0,source,512)==-2);aux_msc_worker_service();assert(aux_msc_take_completion(&n)&&n==-1);assert(!sync_media() && !aux_msc_media_writing() && sense==SCSI_SENSE_MEDIUM_ERROR);fail_write=false;
 gen=aux_msc_media_generation();cancel_write=true;assert(tud_msc_write10_cb(0,0,0,source,512)==-2);aux_msc_worker_service();assert(!aux_msc_take_completion(&n)&&!aux_msc_media_writing()&&aux_msc_media_generation()==gen+1);
 uint8_t command[16]={0x04};before=writes;assert(tud_msc_scsi_cb(0,command,NULL,0)==-1 && writes==before);
 puts("PASS: actualAUXWRITE10 async owned buffers,partial-sectorRMW,bounds,SDfailures,SYNC35media lease,reset/requeue epochs andno format");
}
