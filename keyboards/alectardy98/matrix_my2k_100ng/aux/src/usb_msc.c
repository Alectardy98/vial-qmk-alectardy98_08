/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_platform.h"
#include "aux_msc.h"
#if MY2K_ANIMATION_PROFILE
#include "aux_animation_files.h"
#endif
#include "aux_msc_limits.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include "device/dcd.h"
#include <string.h>
/* Experimental local-only identity, not a manufacturer VID allocation.
 * 1209:0001 is a test PID; do not distribute this identity as an assigned product. */
static const tusb_desc_device_t device={
 .bLength=sizeof(tusb_desc_device_t),.bDescriptorType=TUSB_DESC_DEVICE,
 .bcdUSB=0x0200,.bDeviceClass=0,.bDeviceSubClass=0,.bDeviceProtocol=0,
 .bMaxPacketSize0=64,.idVendor=0x1209,.idProduct=0x0001,
#if MY2K_DEV15_PROFILE
 .bcdDevice=0x0015,
#else
 .bcdDevice=0x0001,
#endif
 .iManufacturer=1,.iProduct=2,.iSerialNumber=0,.bNumConfigurations=1};
static const uint8_t configuration[]={
 TUD_CONFIG_DESCRIPTOR(1,1,0,TUD_CONFIG_DESC_LEN+TUD_MSC_DESC_LEN,0,100),
 TUD_MSC_DESCRIPTOR(0,0,0x01,0x81,64)};
uint8_t const *tud_descriptor_device_cb(void){return (const uint8_t*)&device;}
uint8_t const *tud_descriptor_configuration_cb(uint8_t index){return index==0?configuration:NULL;}
uint16_t const *tud_descriptor_string_cb(uint8_t index,uint16_t language){
 (void)language;static uint16_t text[32];
 if(index==0){text[0]=(TUSB_DESC_STRING<<8)|4;text[1]=0x0409;return text;}
 const char *s=index==1?"Experimental MY2K":index==2?
#if MY2K_STOCK_PARITY_PROFILE && (!MY2K_DEV7_PROFILE || MY2K_RELEASE1_PROFILE)
 "AUX asset storage":
#else
 "AUX read-only storage":
#endif
 NULL;
 if(!s)return NULL;
 unsigned n=0;while(s[n]&&n<31){text[n+1]=(uint8_t)s[n];n++;}
 text[0]=(uint16_t)((TUSB_DESC_STRING<<8)|(2*n+2));return text;
}
void tud_msc_inquiry_cb(uint8_t lun,uint8_t vendor[8],uint8_t product[16],uint8_t rev[4]){
 (void)lun;memcpy(vendor,"MY2K-EXP",8);
#if MY2K_DEV15_PROFILE
 #if MY2K_RELEASE1_PROFILE
 memcpy(product,"R1 asset storage",16);
#else
 memcpy(product,"Read-only assets",16);
#endif
 memcpy(rev,AUX_MSC_INQUIRY_REVISION,4);
#elif MY2K_DEV7_PROFILE
 memcpy(product,"Read-only assets",16);memcpy(rev,"0007",4);
#elif MY2K_STOCK_PARITY_PROFILE
 memcpy(product,"Asset storage RW",16);
#if MY2K_DEV6_PROFILE
 memcpy(rev,"0006",4);
#else
 memcpy(rev,"0005",4);
#endif
#else
 memcpy(product,"Read-only assets",16);memcpy(rev,"0001",4);
#endif
}
bool tud_msc_test_unit_ready_cb(uint8_t lun){
 if(lun||!aux_sd_sectors()){tud_msc_set_sense(lun,SCSI_SENSE_NOT_READY,0x3a,0);return false;}return true;
}
void tud_msc_capacity_cb(uint8_t lun,uint32_t *count,uint16_t *size){*count=lun?0:aux_sd_sectors();*size=512;}
bool tud_msc_is_writable_cb(uint8_t lun){
#if MY2K_STOCK_PARITY_PROFILE && (!MY2K_DEV7_PROFILE || MY2K_RELEASE1_PROFILE)
 return !lun && aux_sd_sectors()!=0
#if MY2K_RELEASE1_PROFILE
 &&aux_msc_export_active()
#endif
 ;
#else
 (void)lun;return false;
#endif
}
int32_t tud_msc_read10_cb(uint8_t lun,uint32_t lba,uint32_t offset,void *buffer,uint32_t size){
 int32_t n=aux_msc_queue(lun,lba,offset,buffer,size);
 if(n==TUD_MSC_RET_ERROR)tud_msc_set_sense(lun,SCSI_SENSE_MEDIUM_ERROR,0x11,0);
 return n;
}
int32_t tud_msc_write10_cb(uint8_t lun,uint32_t lba,uint32_t offset,uint8_t *buffer,uint32_t size){
#if MY2K_STOCK_PARITY_PROFILE && (!MY2K_DEV7_PROFILE || MY2K_RELEASE1_PROFILE)
 int32_t n=aux_msc_queue_write(lun,lba,offset,buffer,size);
 if(n==TUD_MSC_RET_ERROR)tud_msc_set_sense(lun,SCSI_SENSE_MEDIUM_ERROR,0x0c,0);
 return n;
#else
 (void)lba;(void)offset;(void)buffer;(void)size;
 tud_msc_set_sense(lun,SCSI_SENSE_DATA_PROTECT,0x27,0);return -1;
#endif
}
int32_t tud_msc_scsi_cb(uint8_t lun,const uint8_t command[16],void *buffer,uint16_t size){
#if MY2K_ANIMATION_PROFILE
 #if MY2K_DIRECT_FILE_READ
 if(!lun&&command[0]==0xc3){
  int32_t n=aux_msc_queue_file(command,buffer,size);if(n==-1)tud_msc_set_sense(lun,SCSI_SENSE_ILLEGAL_REQUEST,0x24,0);return n;
 }
 #endif
 if(!lun&&command[0]==0xc2){
  int32_t n=aux_animation_bulk_read(command,buffer,size);if(n<0)tud_msc_set_sense(lun,SCSI_SENSE_ILLEGAL_REQUEST,0x24,0);return n;
 }
 if(!lun&&command[0]==0xc1){
  if(aux_animation_bulk_submit(command,buffer,size))return size;
  tud_msc_set_sense(lun,SCSI_SENSE_ILLEGAL_REQUEST,0x24,0);return -1;
 }
#endif
#if MY2K_STOCK_PARITY_PROFILE
 if(!lun && command[0]==0x35){
  for(unsigned i=1;i<16;i++)if(command[i]){tud_msc_set_sense(lun,SCSI_SENSE_ILLEGAL_REQUEST,0x24,0);return -1;}
#if MY2K_DEV7_PROFILE && !MY2K_RELEASE1_PROFILE
  if(!size && aux_msc_io_idle())return 0; /* No write/lease exists in read-only profile. */
#else
  if(!size && aux_msc_media_sync())return 0;
#endif
  tud_msc_set_sense(lun,SCSI_SENSE_MEDIUM_ERROR,0x0c,0);return -1;
 }
#endif
 (void)command;(void)buffer;(void)size;
 tud_msc_set_sense(lun,SCSI_SENSE_ILLEGAL_REQUEST,0x20,0);return -1;
}
void tud_msc_scsi_complete_cb(uint8_t lun,const uint8_t command[16]){
#if MY2K_ANIMATION_PROFILE
 if(!lun&&command[0]==0xc2)aux_animation_fast_ack();
#else
 (void)lun;(void)command;
#endif
}
uint32_t SystemCoreClock=216000000u;
uint32_t tusb_time_millis_api(void){return aux_millis();}
void OTG_HS_Handler(void){if(!aux_usb_failed())tud_int_handler(1);}
/* Board-local adaptation for pinned TinyUSB ea1d867. The MSC source object
 * renames only these three calls, leaving the vendor checkout untouched.
 * TinyUSB's pending_io boolean alone cannot distinguish an old deferred
 * completion from a new CBW after BOT/bus reset. */
extern void aux_mscd_reset_native(uint8_t rhport);
extern bool aux_mscd_control_native(uint8_t rhport,uint8_t stage,const tusb_control_request_t *request);
static struct {osal_task_func_t function;void *parameter;uint32_t generation;bool busy;} deferred;
static void guarded_completion(void *unused){
 (void)unused;osal_task_func_t fn=deferred.function;void *parameter=deferred.parameter;
 bool current=deferred.generation==aux_msc_generation();deferred.busy=false;
 if(current)fn(parameter);
}
void aux_msc_defer(osal_task_func_t function,void *parameter,bool in_isr){
 /* Only aux_usb_task submits completion; it drains this record before reuse. */
 if(deferred.busy||in_isr){aux_fault_code=8;return;}
 deferred.function=function;deferred.parameter=parameter;
 deferred.generation=aux_msc_generation();deferred.busy=true;
 usbd_defer_func(guarded_completion,NULL,false);
}
void mscd_reset(uint8_t rhport){
#if MY2K_ANIMATION_PROFILE
aux_animation_fast_cancel();
#endif
aux_msc_cancel();aux_mscd_reset_native(rhport);}
bool mscd_control_xfer_cb(uint8_t rhport,uint8_t stage,const tusb_control_request_t *request){
 if(stage==CONTROL_STAGE_SETUP&&request->bmRequestType_bit.type==TUSB_REQ_TYPE_CLASS&&
    request->bRequest==MSC_REQ_RESET&&request->wValue==0&&request->wLength==0)aux_msc_cancel();
 return aux_mscd_control_native(rhport,stage,request);
}
static bool usb_ready;
static bool usb_failed_cleanup(void){
 if(!aux_usb_failed())return false;
 /* Run only in foreground. Invalidate both a borrowed destination and queued
  * completion before retiring this USB instance. Do not call TinyUSB again. */
 if(usb_ready){usb_ready=false;aux_msc_cancel();deferred.busy=false;}
 return true;
}
void aux_usb_task(void){
 if(usb_failed_cleanup()||!usb_ready)return;
 tud_task();
 if(usb_failed_cleanup())return;
 int32_t bytes;
 if(!deferred.busy&&aux_msc_take_completion(&bytes)){
  if(bytes<0)tud_msc_set_sense(0,SCSI_SENSE_MEDIUM_ERROR,0x11,0);
  if(!tud_msc_async_io_done(bytes,false))aux_msc_cancel();
  if(usb_failed_cleanup())return;
  tud_task();
  (void)usb_failed_cleanup();
 }
}
bool aux_usb_init(void){
 if(aux_usb_failed())return false;
 /* Pins/clocks configured by the independently reviewed platform helper. */
 extern void aux_usb_platform_init(void);aux_usb_platform_init();
 const tusb_rhport_init_t init={.role=TUSB_ROLE_DEVICE,.speed=TUSB_SPEED_FULL};
 usb_ready=tusb_init(1,&init);
 if(!usb_ready){
  /* Failed TinyUSB init may have assigned its rhport before DCD failure.
   * Latch retirement too: another tusb_init call can otherwise return true
   * from its already-initialized-rhport shortcut. Zero means init failed
   * without a precise failed register; only system reset clears the latch. */
  aux_usb_abort(0);
  aux_msc_cancel();
 }
 return usb_ready;
}

#if MY2K_ANIMATION_PROFILE
#include "aux_animation_files.h"
static uint8_t animation_control[32];
static bool fast_reply_final;
bool tud_vendor_control_xfer_cb(uint8_t rhport,uint8_t stage,tusb_control_request_t const *request){
 if(request->wValue||request->wIndex)return false;
 if(request->bmRequestType==0xc0&&request->bRequest==0x56&&request->wLength==32){
  if(stage==CONTROL_STAGE_SETUP){uint16_t n;uint8_t *p=aux_animation_fast_reply(&n);return tud_control_xfer(rhport,request,p,32);}return true;
 }
 if(request->bmRequestType==0xc0&&request->bRequest==0x55&&request->wLength==32){
  if(stage==CONTROL_STAGE_SETUP){aux_animation_bulk_status(animation_control);return tud_control_xfer(rhport,request,animation_control,32);}return true;
 }
 if(request->bmRequestType==0x40&&request->bRequest==0x53){
  if(stage==CONTROL_STAGE_SETUP){uint8_t *p=aux_animation_fast_receive(request->wLength);return p&&tud_control_xfer(rhport,request,p,request->wLength);}
  if(stage==CONTROL_STAGE_DATA)return aux_animation_fast_submit();
  return true;
 }
 if(request->bmRequestType==0xc0&&request->bRequest==0x54){
  if(request->wLength!=4096)return false;
  if(stage==CONTROL_STAGE_SETUP){uint16_t n;uint8_t*p=aux_animation_fast_reply(&n);fast_reply_final=p[6]!=1;return tud_control_xfer(rhport,request,p,n);}
  if(stage==CONTROL_STAGE_ACK&&fast_reply_final)aux_animation_fast_ack();
  return true;
 }
 if(request->wLength!=32)return false;
 if(request->bmRequestType==0x40&&request->bRequest==0x51){
  if(stage==CONTROL_STAGE_SETUP)return tud_control_xfer(rhport,request,animation_control,32);
  if(stage==CONTROL_STAGE_DATA)return aux_animation_files_submit(animation_control);
  return true;
 }
 if(request->bmRequestType==0xc0&&request->bRequest==0x52){
  if(stage==CONTROL_STAGE_SETUP){aux_animation_files_reply(animation_control);return tud_control_xfer(rhport,request,animation_control,32);}
  return true;
 }
 return false;
}
#endif
