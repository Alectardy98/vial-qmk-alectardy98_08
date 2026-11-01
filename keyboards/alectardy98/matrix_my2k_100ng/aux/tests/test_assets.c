/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aux_platform.h"
#include "aux_assets.h"
#include "ff.h"
#include "diskio.h"
static FILE *image;static uint32_t sectors;static bool partition_boot_read,backend_fail;static unsigned reads,colors,monos,color_pixel,expected_pixels=2;static uint16_t latest[2];static bool flipped;
bool aux_sd_read(uint32_t lba,uint8_t block[512]){reads++;if(lba==238)partition_boot_read=true;return !backend_fail&&lba<sectors&&fseek(image,(long)lba*512,SEEK_SET)==0&&fread(block,1,512,image)==512;}
uint32_t aux_sd_sectors(void){return sectors;}
static bool color(void *ctx,uint16_t w,uint16_t h,uint32_t first,const uint16_t *p,unsigned n){
 (void)ctx;assert((unsigned)w*h==expected_pixels&&first==color_pixel&&n<=128&&n>0);
 for(unsigned i=0;i<n;i++)assert(p[i]==(((colors&1)^flipped)?((first+i)&1?0xffff:0x001f):((first+i)&1?0x07e0:0xf800)));
 if(!first){latest[0]=p[0];latest[1]=p[1];}
 color_pixel+=n;if(color_pixel==expected_pixels){colors++;color_pixel=0;}return true;
}
static bool mono(void *ctx,uint16_t w,uint16_t h,uint32_t first,const uint16_t *p,unsigned n){(void)ctx;assert(w==8&&h==1&&first==0&&n==8&&p[0]==65535&&p[1]==0);monos++;return true;}
static void pump(uint32_t now,const aux_render_sinks *sinks){unsigned limit=0;do{aux_assets_step(now,sinks);assert(++limit<1024);}while(aux_assets_busy());}
void aux_msc_worker_service(void){}
#if MY2K_STOCK_PARITY_PROFILE
/* This fixture has no concurrent USB owner; the dedicated MSC suites exercise
 * epoch cancellation and ownership. Supply the idle read-only media contract
 * so the same asset tests also run against the enabled release profile. */
bool aux_msc_media_writing(void){return false;}
uint32_t aux_msc_media_generation(void){return 1;}
bool aux_msc_io_idle(void){return true;}
#endif
int main(int argc,char **argv){assert(argc>=2&&argc<=4);if(argc>=3)expected_pixels=258;
 image=fopen(argv[1],"rb");assert(image);assert(fseek(image,0,SEEK_END)==0);long bytes=ftell(image);assert(bytes>0&&bytes%512==0);sectors=(uint32_t)(bytes/512);
 uint8_t block[512];assert(disk_read(0,block,sectors,1)==RES_PARERR);assert(disk_read(0,block,sectors-1,2)==RES_PARERR);assert(disk_read(0,block,0,0)==RES_PARERR);assert(disk_read(1,block,0,1)==RES_PARERR);assert(reads==0);assert(aux_assets_init());if(sectors==247807)assert(partition_boot_read);
 aux_render_sinks sinks={color,mono,NULL};aux_asset_status status;
 assert(!aux_assets_select_color("../LIVE.CRS"));assert(!aux_assets_select_color("0:/LIVE.CRS"));assert(!aux_assets_select_color("missing.CRS"));assert(!aux_assets_select_color("BW_TEXT.ABW"));
 aux_assets_status(&status);assert(status.mounted&&status.color_present&&status.mono_present&&!status.color_completed);
 if(expected_pixels>128){
   aux_assets_step(0,&sinks);aux_assets_status(&status);
   assert(status.color_decoding&&status.color_next_pixel==128&&status.color_completed==0&&monos==1);
   if(argc==4&&!strcmp(argv[3],"fail")){backend_fail=true;aux_assets_step(0,&sinks);aux_assets_status(&status);
     assert(!status.color_active&&!status.color_decoding&&status.read_errors==1&&status.color_completed==0&&colors==0);
     unsigned at_failure=reads;aux_assets_step(100,&sinks);assert(reads==at_failure);fclose(image);
     puts("AUX mid-frame storage failure stops decoding without false completion: PASS");return 0;
   }
   aux_assets_set_paused(true,0);unsigned previous_reads=reads;aux_assets_step(0,&sinks);
   assert(reads==previous_reads&&!aux_assets_busy());aux_assets_status(&status);assert(status.paused&&status.color_next_pixel==128);
   aux_assets_set_paused(false,0);
   aux_assets_set_outputs(false,false,0);previous_reads=reads;aux_assets_step(500,&sinks);
   assert(reads==previous_reads&&!aux_assets_busy());
   aux_assets_set_outputs(true,true,0);aux_assets_status(&status);
   assert(status.color_next_pixel==0&&status.color_decoding);color_pixel=0;
 }
 pump(0,&sinks);assert(colors==1&&monos==1&&latest[0]==0xf800&&latest[1]==0x07e0);
 pump(9,&sinks);assert(colors==1);pump(10,&sinks);assert(colors==2&&latest[0]==0x001f&&latest[1]==0xffff);
 pump(29,&sinks);assert(colors==2);pump(30,&sinks);assert(colors==3&&monos==1&&latest[0]==0xf800);
 aux_assets_set_paused(true,31);pump(100,&sinks);assert(colors==3);
 aux_assets_set_paused(false,100);pump(109,&sinks);assert(colors==3);pump(110,&sinks);assert(colors==4);
 assert(aux_render_failures==0&&aux_asset_errors==0&&reads>0);
 pump(130,NULL);aux_assets_status(&status);assert(status.output_failures==1&&colors==4&&status.color_completed==5&&!status.color_decoding);
 assert(status.mono_present&&!status.mono_active&&status.mono_completed==1);
 if(argc==4&&!strcmp(argv[3],"select")){
   assert(aux_assets_select_color("NEXT.CRS"));colors=0;color_pixel=0;flipped=true;
   pump(200,&sinks);assert(colors==1&&latest[0]==0x001f&&latest[1]==0xffff);
   pump(229,&sinks);assert(colors==1);pump(230,&sinks);assert(colors==2&&latest[0]==0xf800&&latest[1]==0x07e0);
 }
 fclose(image);
 puts("AUX FAT/ANIM/ABIT cooperative chunks, pause/resume, timing, status and bounds: PASS");return 0;}
