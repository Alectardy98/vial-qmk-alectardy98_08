/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_performance_mode.h"
#include "main_usb_performance.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t word=MY2K_BRIGHTNESS_MARKER,now,error;
static bool durable=true,save_pending,busy,idle=true,exported,unlocked=true,write_fail;
static unsigned writes,restarts,fallbacks;
uint32_t my2k_performance_platform_read(void){return word;}
void my2k_performance_platform_write(uint32_t v){writes++;if(!write_fail)word=v;}
bool my2k_performance_platform_durable(void){return durable;}
bool my2k_performance_platform_pending(void){return save_pending;}
bool my2k_performance_platform_busy(void){return busy;}
bool my2k_performance_platform_idle(void){return idle;}
bool my2k_performance_platform_export(void){return exported;}
bool my2k_performance_platform_unlocked(void){return unlocked;}
uint32_t my2k_performance_platform_error(void){return error;}
uint32_t my2k_performance_platform_now(void){return now;}
void my2k_performance_platform_restart(void){restarts++;}
void my2k_performance_platform_usb_fallback(void){fallbacks++;}
static uint8_t report[32];
static unsigned u16(unsigned p){return report[p]|(unsigned)report[p+1]<<8;}
static uint32_t u32(unsigned p){return report[p]|(uint32_t)report[p+1]<<8|(uint32_t)report[p+2]<<16|(uint32_t)report[p+3]<<24;}
static bool command(unsigned c,unsigned v){memset(report,0xa5,sizeof report);report[0]=0xfd;report[1]=c;report[2]=v;assert(my2k_performance_reply(report));assert(report[3]==1);for(unsigned i=28;i<32;i++)assert(!report[i]);return report[2]==0xaa;}
static void tick(uint32_t n){now=n;my2k_performance_task(n);}
static const uint8_t config[]={9,2,64,0,3,1,0,128,50,9,4,0,0,1,3,1,1,0,7,5,129,3,8,0,1,9,4,1,0,2,3,0,0,0,7,5,130,3,32,0,1,7,5,2,3,32,0,1,9,4,2,0,1,3,0,0,0,7,5,131,3,32,0,1};
static void descriptors(void){
 uint8_t out[66],base[64];size_t n=999;
 for(unsigned high=0;high<2;high++)for(unsigned other=0;other<2;other++){
  memset(out,0xa5,sizeof out);assert(my2k_input_usb_configuration(config,64,high,other,1,3,out,64,&n));assert(n==64&&out[1]==(other?7:2)&&out[64]==0xa5);
  assert(out[24]==1&&out[63]==1&&out[40]==(high?4:1)&&out[47]==(high?4:1));
 }
 assert(!my2k_input_usb_configuration(NULL,64,0,0,1,3,out,64,&n)&&!n);
 assert(!my2k_input_usb_configuration(config,64,0,0,1,3,NULL,64,&n)&&!n);
 assert(!my2k_input_usb_configuration(config,64,0,0,1,3,out,64,NULL));
 assert(!my2k_input_usb_configuration(config,8,0,0,1,3,out,64,&n)&&!n);
 assert(!my2k_input_usb_configuration(config,64,0,0,1,3,out,63,&n)&&!n);
 const unsigned offsets[]={0,1,2,9,10,18,19,21,22,23,24};
 const uint8_t values[]={8,7,63,0,5,8,4,2,0,1,0};
 for(unsigned i=0;i<sizeof offsets/sizeof offsets[0];i++){
  memcpy(base,config,64);base[offsets[i]]=values[i];memset(out,0xa5,sizeof out);n=99;
  assert(!my2k_input_usb_configuration(base,64,1,0,1,3,out,64,&n));assert(n==0);for(unsigned j=0;j<66;j++)assert(out[j]==0xa5);
 }
 memcpy(base,config,64);base[14]=8;base[21]=2;assert(!my2k_input_usb_configuration(base,64,1,0,1,3,out,64,&n));assert(my2k_input_usb_configuration(base,64,0,0,1,3,out,64,&n));
 /* Deterministic bounded mutation fuzz catches descriptor-chain OOB under ASan. */
 for(unsigned i=0;i<64;i++)for(unsigned v=0;v<256;v++){
  memcpy(base,config,64);base[i]=(uint8_t)v;memset(out,0xa5,sizeof out);n=99;
  bool ok=my2k_input_usb_configuration(base,64,1,0,1,3,out,64,&n);assert(out[64]==0xa5&&out[65]==0xa5);assert(n==(ok?64:0));
 }
}
int main(int argc,char**argv){assert(argc==2);int s=atoi(argv[1]);
 if(s==30){descriptors();puts("PASS descriptors bounds/speed/other HID/no HSbulk/fuzz");return 0;}
 if(s>=10&&s<=16)word|=MY2K_PERFORMANCE_BIT;
 if(s==15)exported=true;
 if(s==17)word=0xffffffff;
 my2k_performance_init();assert(command(75,0));
 if(s==0){assert(!my2k_performance_hs_requested());assert(report[5]==255&&u16(8)==0&&u16(10)==1000);my2k_performance_usb_enumerated(3);my2k_performance_usb_configured(true);assert(command(75,0)&&report[7]==1&&u16(8)==1000);assert(command(76,0)&&!writes&&!restarts);}
 else if(s==1){assert(command(76,1));assert(word==(MY2K_BRIGHTNESS_MARKER|MY2K_PERFORMANCE_BIT)&&writes==1&&(report[6]&1));assert(command(76,1)&&writes==1);assert(!command(76,0));tick(499);assert(!restarts);save_pending=true;tick(500);assert(!restarts);save_pending=false;busy=true;tick(501);assert(!restarts);busy=false;idle=false;tick(502);assert(!restarts);idle=true;tick(503);tick(504);assert(restarts==1);}
 else if(s>=2&&s<=6){if(s==2)unlocked=false;if(s==3)exported=true;if(s==4)busy=true;if(s==5)save_pending=true;if(s==6)durable=false;assert(!command(76,1)&&!writes&&!restarts);}
 else if(s==7){write_fail=true;assert(!command(76,1)&&u32(20)==3&&!restarts);}
 else if(s==8){assert(command(76,1));error=77;tick(500);assert(command(75,0)&&u32(20)==77&&!(report[6]&1)&&!restarts);}
 else if(s==9){assert(command(76,1));save_pending=true;tick(10001);assert(command(75,0)&&u32(20)==4&&!(report[6]&1)&&!restarts);}
 else if(s==10){my2k_performance_usb_enumerated(0);my2k_performance_usb_configured(true);assert(command(75,0)&&report[5]==1&&report[7]==2&&u16(8)==125);tick(10000);assert(!fallbacks);}
 else if(s==11){tick(2999);assert(!fallbacks);tick(3000);tick(9000);assert(fallbacks==1&&!my2k_performance_hs_requested()&&my2k_performance_high_cpu());assert(word&MY2K_PERFORMANCE_BIT);my2k_performance_usb_enumerated(3);my2k_performance_usb_configured(true);assert(command(75,0)&&(report[6]&32)&&report[7]==1);}
 else if(s==12){my2k_performance_usb_enumerated(0);my2k_performance_usb_configured(true);now=50000;my2k_performance_usb_reset();tick(50001);tick(52999);assert(!fallbacks);tick(53000);assert(fallbacks==1);}
 else if(s==13){my2k_performance_usb_enumerated(0);now=50000;my2k_performance_usb_reset();tick(52999);my2k_performance_usb_enumerated(0);my2k_performance_usb_configured(true);tick(60000);assert(!fallbacks&&my2k_performance_hs_requested());}
 else if(s==14){my2k_performance_usb_fault(8);tick(30000);assert(!fallbacks&&my2k_performance_usb_start_failed());}
 else if(s==15){assert(!my2k_performance_hs_requested()&&!my2k_performance_high_cpu()&&(report[6]&64)&&report[4]==1);assert(!command(76,0)&&!writes);tick(30000);assert(!fallbacks);}
 else if(s==16){my2k_performance_usb_enumerated(2);my2k_performance_usb_configured(true);assert(command(75,0)&&report[7]==0&&u32(12)==16);}
 else if(s==17){assert(!my2k_performance_hs_requested());assert(!command(76,1)&&u32(20)==2&&!writes);}
 else if(s==18){assert(command(76,1));durable=false;tick(500);assert(command(75,0)&&u32(20)==5&&!restarts);}
 else if(s==19){now=0xffffff00;assert(command(76,1));tick(0x000000f3);assert(!restarts);tick(0x000000f4);assert(restarts==1);}
 else if(s==20){assert(!command(76,2)&&!writes);idle=false;assert(!command(76,1)&&!writes);}
 else assert(0);
 printf("PASS production performance scenario %d\n",s);return 0;
}
