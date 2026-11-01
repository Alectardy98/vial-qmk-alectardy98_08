/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_assets.h"
#include "aux_platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static FILE *image;static uint32_t epoch,sectors;static bool writing;static unsigned reads,monos;static uint16_t first_two[2];
uint32_t aux_msc_media_generation(void){return epoch;}
bool aux_msc_media_writing(void){return writing;}
bool aux_msc_io_idle(void){return true;}
void aux_msc_worker_service(void){}
uint32_t aux_sd_sectors(void){return sectors;}
bool aux_sd_read(uint32_t lba,uint8_t out[512]){reads++;return lba<sectors&&fseek(image,(long)lba*512,SEEK_SET)==0&&fread(out,1,512,image)==512;}
static bool color(void *ctx,uint16_t w,uint16_t h,uint32_t first,const uint16_t *p,unsigned n){(void)ctx;(void)w;(void)h;(void)first;(void)p;(void)n;return true;}
static bool mono(void *ctx,uint16_t w,uint16_t h,uint32_t first,const uint16_t *p,unsigned n){(void)ctx;assert(w==8&&h==1&&first==0&&n==8);first_two[0]=p[0];first_two[1]=p[1];monos++;return true;}
static void patch(unsigned offset,const void *bytes,unsigned size){assert(fseek(image,(long)33*512+offset,SEEK_SET)==0&&fwrite(bytes,1,size,image)==size&&fflush(image)==0);}
int main(int argc,char **argv){assert(argc==2);image=fopen(argv[1],"r+b");assert(image);assert(fseek(image,0,SEEK_END)==0);sectors=(uint32_t)ftell(image)/512;assert(aux_assets_init());aux_render_sinks sinks={color,mono,0};
 aux_assets_step(0,&sinks);assert(monos==1&&first_two[0]==65535&&first_two[1]==0);
 writing=true;uint8_t pixels=2;patch(22,&pixels,1);unsigned before=reads;assert(!aux_assets_reload_mono());aux_assets_step(10,&sinks);assert(reads==before&&monos==1);
 writing=false;epoch++;aux_assets_step(20,&sinks);assert(monos==2&&first_two[0]==0&&first_two[1]==65535);
 /* Explicit SCR:AM0 must reread the file instead of redrawing retained RAM. */
 pixels=3;patch(22,&pixels,1);assert(aux_assets_reload_mono());aux_assets_step(30,&sinks);assert(monos==3&&first_two[0]==65535&&first_two[1]==65535);
 writing=true;patch(0,"FAIL",4);writing=false;epoch++;aux_assets_step(40,&sinks);assert(monos==3&&!aux_assets_reload_mono());
 fclose(image);puts("PASS: actual FatFs/ABIT reload,host-write lease pauses reads,completed-generation cache invalidation,and corrupt-upload rejection");
}
