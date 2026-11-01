/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_nes.h"
#if MY2K_GBC_PROFILE
#include "aux_gbc.h"
extern unsigned char __gbc_bss_start[],__gbc_bss_end[];
#endif
#if MY2K_DOOM_PROFILE
#include "aux_doom.h"
extern unsigned char __nes_bss_start[],__nes_bss_end[];
#endif
#include "my2k_nes_core.h"
#include "aux_animation_files.h"
#include "aux_assets.h"
#include "aux_msc.h"
#include "aux_platform.h"
#include "aux_work.h"
#include "aux_disk_policy.h"
#include "aux_nes_library.h"
#include "my2k_oled_config.h"
#include "ff.h"
#include <string.h>
static uint8_t rom[MY2K_NES_ROM_MAX]
#if MY2K_DOOM_PROFILE
 __attribute__((section(".nes_bss.rom")))
#endif
;
static FIL rom_file;
static bool rom_open;
static char rom_name[96];
static bool battery_ready;
static uint32_t battery_crc;
static uint16_t pixels[256];
static aux_panels *panel;
static uint8_t active,fault;
static uint32_t frames,simulated,last_ms,max_ms,next_frame,completed_hash,drawing_hash;
static unsigned fraction,write_at,pixels_used;
static bool failed;
bool aux_nes_active(void){return active!=0;}
bool aux_nes_runnable(uint32_t now){
#if MY2K_GBC_PROFILE
 if(active==3)return aux_gbc_runnable(now);
#endif
 return active&&
#if MY2K_DOOM_PROFILE
 (active==2?aux_doom_runnable(now):
#endif
 (int32_t)(now-next_frame)>=0
#if MY2K_DOOM_PROFILE
 )
#endif
 ;}
static void append(const uint16_t *p,unsigned n){
 if(failed)return;
 for(unsigned i=0;i<n;i++){
  uint16_t value=p?p[i]:0;drawing_hash=(drawing_hash^value)*16777619u;
  pixels[pixels_used++]=value;
  if(pixels_used==256){
   if(!aux_panel_color_pixels(panel,128,128,write_at,pixels,256))failed=true;
   write_at+=256;pixels_used=0;
  }
 }
}
static void row(unsigned y,const uint16_t *p){(void)y;append(p,128);}
static void stop(void){
#if MY2K_GBC_PROFILE
 if(active==3)fault=(uint8_t)aux_gbc_stop();else
#endif
#if MY2K_DOOM_PROFILE
 if(active==2)aux_doom_stop();else
#endif
 if(active==1){
  uint8_t *ram=my2k_nes_battery_ram();
  if(!fault&&battery_ready&&ram&&my2k_oled_crc(ram,8192)!=battery_crc){
   if(aux_msc_export_active()||aux_msc_media_writing()||aux_nes_library_ram(rom_name,ram,rom,true)!=1)fault=13;
  }
  my2k_nes_core_stop();
 }
 battery_ready=false;
 if(rom_open){if(f_close(&rom_file)!=FR_OK){aux_nes_library_close_failed();fault=13;}rom_open=false;}
 aux_disk_game_reads(false);active=0;
}
static int read_rom(uint32_t offset,void *dst,unsigned n){
 if(!rom_open||offset>f_size(&rom_file)||n>f_size(&rom_file)-offset)return 0;
 if(f_tell(&rom_file)!=offset&&f_lseek(&rom_file,offset)!=FR_OK)return 0;
 UINT got=0;int ok=f_read(&rom_file,dst,n,&got)==FR_OK&&got==n;aux_work_checkpoint();return ok;
}
static int load_rom(void){
 if(aux_animation_files_busy()||aux_msc_export_active()||aux_msc_media_writing()||!aux_assets_media_service())return 2;
 DIR dir;FILINFO info;char path[160]={0};
 bool configured=aux_nes_library_selected(path);
 if(!aux_nes_library_ready())return 13;
 if(!configured){
 if(f_opendir(&dir,"0:/nes")!=FR_OK)return 3;
 for(unsigned examined=0;examined<256&&f_readdir(&dir,&info)==FR_OK&&info.fname[0];examined++){
  size_t n=strlen(info.fname);
  if(!(info.fattrib&AM_DIR)&&n>4&&n+8<sizeof path&&(!strcmp(info.fname+n-4,".nes")||!strcmp(info.fname+n-4,".NES"))){
   memcpy(path,"0:/nes/",7);memcpy(path+7,info.fname,n+1);break;
  }
 }
 bool closed=f_closedir(&dir)==FR_OK;if(!closed)aux_nes_library_close_failed();if(!path[0]||!closed)return 3;
 }
 if(f_open(&rom_file,path,FA_READ)!=FR_OK)return 3;
 rom_open=true;
 size_t size=f_size(&rom_file);uint8_t header[16];my2k_nes_rom_info metadata;
 if(!read_rom(0,header,16)){stop();return 4;}
 int error=my2k_nes_rom_parse(header,(uint32_t)size,&metadata);if(error){stop();return error;}
 my2k_nes_core_callbacks(row,aux_work_checkpoint);
 if(size<=sizeof rom){
  for(unsigned off=0;off<size;){unsigned n=(unsigned)(size-off);if(n>4096)n=4096;
   if(!read_rom(off,rom+off,n)){stop();return 4;}off+=n;
  }
  if(f_close(&rom_file)!=FR_OK){aux_nes_library_close_failed();rom_open=false;return 4;}rom_open=false;
  error=my2k_nes_core_load(rom,size);
 }else{
  aux_disk_game_reads(true); /* Demand reads avoid random-bank read amplification. */
  error=my2k_nes_core_load_stream(header,(uint32_t)size,rom,sizeof rom,read_rom);
 }
 if(error){my2k_nes_core_stop();stop();return error;}
 strcpy(rom_name,path+7);
 uint8_t *ram=my2k_nes_battery_ram();battery_ready=false;
 if(ram){
  if(aux_nes_library_ram(rom_name,ram,rom,false)<0){my2k_nes_core_stop();stop();return 13;}
  battery_crc=my2k_oled_crc(ram,8192);battery_ready=true;
 }
 return 0;
}
bool aux_nes_request(const my2k_wire_message *q,my2k_wire_message *r){
 if(!q||!r||q->type!=MY2K_AC||q->v[0]<15||q->v[0]>18)return false;
 *r=(my2k_wire_message){.type=MY2K_AR};memcpy(r->v,q->v,3);r->v[3]=1;
 if(!q->v[1]||q->v[3]!=1)return true;
 if(q->v[0]==15){if(q->v[2])return true;memcpy(r->v+4,
#if MY2K_GBC_PROFILE
 "GAM3",
#elif MY2K_DOOM_PROFILE
 "GAM2",
#else
 "NES1",
#endif
 4);r->v[3]=0;return true;}
 if(q->v[0]==16){
  if(q->v[2]>
#if MY2K_GBC_PROFILE
 4
#elif MY2K_DOOM_PROFILE
 3
#else
 2
#endif
 )return true;
  if(!q->v[2])stop();
#if MY2K_GBC_PROFILE
  else if(q->v[2]==4){
   if(active||!panel||panel->color_streaming||panel->color_blanked||aux_panel_color_busy(panel)){r->v[3]=2;return true;}
   memset(__gbc_bss_start,0,(size_t)(__gbc_bss_end-__gbc_bss_start));
   fault=(uint8_t)aux_gbc_start(panel);if(fault){r->v[3]=fault;return true;}active=3;
  }
#endif
#if MY2K_DOOM_PROFILE
  else if(q->v[2]==3){
   if(active&&active!=2){r->v[3]=2;return true;}
   fault=(uint8_t)aux_doom_start(panel);if(fault){r->v[3]=fault;return true;}active=2;
  }
#endif
  else if(q->v[2]==1&&!active){
   /* Starts only at a complete display boundary, never midway through GRAM. */
   if(!panel||panel->color_streaming||panel->color_blanked||aux_panel_color_busy(panel)){r->v[3]=2;return true;}
#if MY2K_DOOM_PROFILE
   memset(__nes_bss_start,0,(size_t)(__nes_bss_end-__nes_bss_start));
#endif
   fault=(uint8_t)load_rom();if(fault){r->v[3]=fault;return true;}
   active=1;frames=simulated=last_ms=max_ms=completed_hash=0;next_frame=aux_millis();fraction=0;
  }
  r->v[3]=0;r->v[4]=active;r->v[5]=fault;r->v[6]=1;return true;
 }
 if(q->v[0]==17){if(!active){r->v[3]=3;return true;}
#if MY2K_GBC_PROFILE
 if(active==3){aux_gbc_buttons(q->v[2]);}else
#endif
#if MY2K_DOOM_PROFILE
 if(active==2){aux_doom_buttons(q->v[2]);}else
#endif
 {my2k_nes_core_buttons(q->v[2]);}
 r->v[3]=0;return true;}
 if(q->v[2]>4)return true;
#if MY2K_GBC_PROFILE
 if(active==3){uint32_t v=aux_gbc_stat(q->v[2]);for(unsigned i=0;i<4;i++)r->v[4+i]=(uint8_t)(v>>(8*i));r->v[3]=0;return true;}
#endif
#if MY2K_DOOM_PROFILE
 if(active==2){uint32_t v=aux_doom_stat(q->v[2]);for(unsigned i=0;i<4;i++)r->v[4+i]=(uint8_t)(v>>(8*i));r->v[3]=0;return true;}
#endif
 uint32_t value=q->v[2]==0?frames:q->v[2]==1?simulated:q->v[2]==2?(last_ms|(max_ms<<16)):q->v[2]==3?(fault|my2k_nes_core_heap_used()<<8):completed_hash;
 for(unsigned i=0;i<4;i++)r->v[4+i]=(uint8_t)(value>>(8*i));
 r->v[3]=0;return true;
}
void aux_nes_service(aux_panels *p,uint32_t now){
 panel=p;if(!active)return;
#if MY2K_GBC_PROFILE
 if(active==3){aux_gbc_service(now);if(!aux_gbc_active()){active=0;fault=(uint8_t)aux_gbc_stat(3);}return;}
#endif
#if MY2K_DOOM_PROFILE
 if(active==2){aux_doom_service(now);if(!aux_doom_active()){active=0;fault=(uint8_t)aux_doom_stat(3);}return;}
#endif
 if(aux_msc_export_active()||aux_msc_media_writing()){stop();return;}
 if(p->color_blanked||aux_panel_color_busy(p)){next_frame=now;return;}
 if((int32_t)(now-next_frame)<0)return;
 if((int32_t)(now-next_frame)>100)next_frame=now;
 unsigned step=16;fraction+=40;if(fraction>=60){fraction-=60;step++;}next_frame+=step;
 bool draw=simulated&&(simulated&1u);uint32_t began=aux_millis();failed=false;write_at=pixels_used=0;drawing_hash=2166136261u;
 if(draw)append(NULL,512); /* 4 black rows:256x240 fits128x120. */
 my2k_nes_core_step(draw);simulated++;
 if(draw){append(NULL,512);if(!failed&&write_at==16384){frames++;completed_hash=drawing_hash;}else failed=true;}
 last_ms=(uint32_t)(aux_millis()-began);if(last_ms>max_ms)max_ms=last_ms;
 if(failed||my2k_nes_core_fault()){fault=my2k_nes_core_fault()?(uint8_t)my2k_nes_core_fault():6;stop();}
}
