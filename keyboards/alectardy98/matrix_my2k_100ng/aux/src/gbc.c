/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Peanut-GB CGB core: local execution, bounded ROM cache and line presenter. */
#include "aux_gbc.h"
#include "aux_nes_library.h"
#include "aux_animation_files.h"
#include "aux_disk_policy.h"
#include "aux_msc.h"
#include "aux_work.h"
#include "aux_platform.h"
#include "my2k_gbc_rom.h"
#include "my2k_gbc_mbc7.h"
#include "my2k_oled_config.h"
#include "ff.h"
#include <setjmp.h>
#include <string.h>
struct gb_s;
static __attribute__((always_inline)) inline uint8_t rom_read(struct gb_s *,uint_fast32_t);
#define MY2K_GB_ROM_READ(g,a) rom_read(g,a)
static uint8_t cart_read(struct gb_s *,uint16_t);
static void cart_write(struct gb_s *,uint16_t,uint8_t);
#define MY2K_GB_MBC7 1
#define MY2K_GB_CART_READ(g,a) cart_read(g,a)
#define MY2K_GB_CART_WRITE(g,a,v) cart_write(g,a,v)
static void checkpoint(void);
static unsigned steps;
#define MY2K_GB_STEP_YIELD() do { if(!(++steps&255u))checkpoint(); } while(0)
#define __not_in_flash_func(x) x
#define RGB555_TO_RGB444(v) ((((v)>>1)&0xfu)|(((v)>>2)&0xf0u)|(((v)>>3)&0xf00u))
#define PEANUT_FULL_GBC_SUPPORT 1
#define PEANUT_GB_USE_INTRINSICS 0
#include "../vendor/peanutgb/peanut_gb.h"
static struct gb_s core;
#define ROM_LINE_BYTES 1024u
#define ROM_CACHE_SETS 32u
#define ROM_CACHE_WAYS 3u
#define ROM_CACHE_LINES (ROM_CACHE_SETS*ROM_CACHE_WAYS)
static uint8_t store[131072];
static uint8_t *ram,*cache;
static unsigned cache_ways;
static uint16_t pixels[256];
static struct {uint32_t bank,stamp;} tags[ROM_CACHE_LINES];
static unsigned clock_stamp,hot_bank,hot_slot,pixel_count,lines;
static uint32_t frames,simulated,last_ms,max_ms,completed_hash,hash,next_frame,fraction,rtc_at;
static uint8_t fault,running,pad,dirty;
static bool opened,draw;
static FIL file;
static char name[96];
static aux_panels *panel;
static my2k_gbc_rom_info metadata;
static jmp_buf escape;
static __attribute__((noreturn)) void die(unsigned why){fault=(uint8_t)why;longjmp(escape,1);}
static void checkpoint(void){
 if(steps>2000000u)die(15);
 aux_work_checkpoint();
}
static uint8_t *hot_data;
static __attribute__((noinline)) uint8_t rom_slow(uint_fast32_t addr){
 unsigned page=(unsigned)addr>>10;
 unsigned first=((page^(page>>5)^(page>>10))&(ROM_CACHE_SETS-1u))*cache_ways;
 unsigned slot=ROM_CACHE_LINES,oldest=first;
 for(unsigned i=first;i<first+cache_ways;i++){
  if(tags[i].stamp&&tags[i].bank==page){slot=i;break;}
  if(tags[i].stamp<tags[oldest].stamp)oldest=i;
 }
 if(slot==ROM_CACHE_LINES){slot=oldest;UINT got=0;uint32_t at=page*ROM_LINE_BYTES;
  if((f_tell(&file)!=at&&f_lseek(&file,at)!=FR_OK)||f_read(&file,cache+slot*ROM_LINE_BYTES,ROM_LINE_BYTES,&got)!=FR_OK||got!=ROM_LINE_BYTES)die(12);
  tags[slot].bank=page;
 }
 tags[slot].stamp=++clock_stamp;hot_bank=page;hot_slot=slot;hot_data=cache+slot*ROM_LINE_BYTES;return hot_data[addr&(ROM_LINE_BYTES-1u)];
}
static __attribute__((always_inline)) inline uint8_t rom_read(struct gb_s *g,uint_fast32_t addr){
 (void)g;if(addr>=metadata.rom_bytes)die(12);
 if(hot_bank==addr>>10)return hot_data[addr&(ROM_LINE_BYTES-1u)];
 return rom_slow(addr);
}
static void code_load(void){
#if defined(__arm__) && MY2K_GBC_ITCM
 extern uint32_t __gbc_itcm_start[],__gbc_itcm_end[],__gbc_itcm_load[];
 *(volatile uint32_t *)0xe000ef90u|=1u;
 __asm__ volatile("dsb\nisb" ::: "memory");
 volatile uint32_t *dst=__gbc_itcm_start;const uint32_t *src=__gbc_itcm_load;
 while(dst<__gbc_itcm_end)*dst++=*src++;
 __asm__ volatile("dsb\nisb" ::: "memory");
#endif
}
static uint8_t cart_read(struct gb_s *g,uint16_t at){return g->enable_cart_ram?my2k_mbc7_read(at):255;}
static void cart_write(struct gb_s *g,uint16_t at,uint8_t v){
 if(at<0x2000){g->enable_cart_ram=(v&15u)==10u;return;}
 if(at<0x4000){g->selected_rom_bank=(v&127u)&g->num_rom_banks_mask;return;}
 if(g->enable_cart_ram)my2k_mbc7_write(at,v);
}
static uint8_t ram_read(struct gb_s *g,uint_fast32_t addr){(void)g;return addr<metadata.ram_bytes?ram[addr]:0xff;}
static void ram_write(struct gb_s *g,uint_fast32_t addr,uint8_t v){(void)g;if(addr<metadata.ram_bytes){if(ram[addr]!=v)dirty=1;ram[addr]=v;}}
static void error(struct gb_s *g,enum gb_error_e e,uint16_t addr){(void)g;(void)addr;die(20u+(unsigned)e);}
static void append(uint16_t v){
 hash=(hash^v)*16777619u;pixels[pixel_count++&255u]=v;
 if(!(pixel_count&255u)&&!aux_panel_color_pixels(panel,128,128,pixel_count-256u,pixels,256))die(6);
}
static void row(struct gb_s *g,const uint8_t *p,uint_fast8_t line){
 if(!draw||line>=144||((line+1u)*115u/144u)==line*115u/144u)return;
 unsigned first=(6u+(line+1u)*115u/144u-1u)*128u;
 if(pixel_count>first){pixel_count=lines=0;hash=2166136261u;}
 while(pixel_count<first)append(0);
 static const uint16_t gray[4]={0xffff,0xad55,0x52aa,0};
 /* A scaled row fills exactly half of the256-pixel staging buffer. Keep the
  * checksum/index local, then publish and flush once rather than calling the
  * single-pixel padding helper128times. Pixel order/palette remain exact. */
 unsigned at=pixel_count&255u;uint32_t next_hash=hash;
 for(unsigned x=0;x<128;x++){
  unsigned i=p[x*160u/128u];uint16_t v;
  if(g->cgb.cgbMode){if(i>=64)die(16);v=g->cgb.fixPalette[i];v=(uint16_t)(((v&0x7fe0u)<<1)|((v>>4)&32u)|(v&31u));}
  else v=gray[i&3u];
  pixels[at+x]=v;next_hash=(next_hash^v)*16777619u;
 }
 hash=next_hash;pixel_count+=128u;
 if(!(pixel_count&255u)&&!aux_panel_color_pixels(panel,128,128,pixel_count-256u,pixels,256))die(6);
 lines++;
}
bool aux_gbc_active(void){return running!=0;}
bool aux_gbc_runnable(uint32_t now){return running&&(int32_t)(now-next_frame)>=0;}
void aux_gbc_buttons(uint8_t b){my2k_mbc7_buttons(b);pad=(uint8_t)~((b&15u)|((b&16u)<<2)|((b&32u)<<2)|((b&64u)>>1)|((b&128u)>>3));}
unsigned aux_gbc_stop(void){
 if(panel&&panel->color_streaming&&running){
  memset(pixels,0,sizeof pixels);
  for(unsigned at=panel->stream_next;at<16384u;at+=256u)if(!aux_panel_color_pixels(panel,128,128,at,pixels,256))break;
 }
 if(running&&!fault&&metadata.battery&&(dirty||metadata.rtc||(metadata.mbc==7&&my2k_mbc7_dirty()))){
  if(aux_msc_export_active()||aux_msc_media_writing()||aux_gbc_library_ram(name,ram,(uint8_t *)cache,metadata.ram_bytes,core.cart_rtc,true)!=1)fault=13;
 }
 if(opened){if(f_close(&file)!=FR_OK){aux_nes_library_close_failed();fault=13;}opened=false;}
 running=0;aux_disk_game_reads(false);return fault;
}
unsigned aux_gbc_start(aux_panels *p){
 code_load();panel=p;fault=0;running=0;opened=false;steps=0;hot_bank=~0u;clock_stamp=0;dirty=0;
 if(aux_animation_files_busy()||aux_msc_export_active()||aux_msc_media_writing()||!aux_assets_media_service())return 2;
 char path[160]={0};bool selected=aux_gbc_library_selected(path);
 if(!aux_nes_library_ready())return 13;
 if(!selected){
  DIR d;FILINFO i;if(f_opendir(&d,"0:/gbc")!=FR_OK)return 3;
  for(unsigned n=0;n<256&&f_readdir(&d,&i)==FR_OK&&i.fname[0];n++){
   const char *e=strrchr(i.fname,'.');
   if(!(i.fattrib&AM_DIR)&&e&&(!strcmp(e,".gb")||!strcmp(e,".gbc")||!strcmp(e,".GB")||!strcmp(e,".GBC"))&&strlen(i.fname)<96){strcpy(path,"0:/gbc/");strcat(path,i.fname);break;}
  }
  if(f_closedir(&d)!=FR_OK){aux_nes_library_close_failed();return 13;}
  if(!path[0])return 3;
 }
 if(f_open(&file,path,FA_READ)!=FR_OK)return 3;
 opened=true;uint8_t header[336];UINT got=0;
 if(f_read(&file,header,sizeof header,&got)!=FR_OK||got!=sizeof header){fault=4;return aux_gbc_stop();}
 fault=(uint8_t)my2k_gbc_rom_parse(header,(uint32_t)f_size(&file),&metadata);
 if(fault)return aux_gbc_stop();
 unsigned reserve=metadata.ram_bytes>32768?65536u:32768u;
 ram=store;cache=store+reserve;cache_ways=reserve==65536?2u:3u;
 if(metadata.ram_bytes>reserve||sizeof store-reserve<metadata.ram_bytes){fault=9;return aux_gbc_stop();}
 memset(&core,0,sizeof core);memset(ram,metadata.mbc==7?255:0,reserve);memset(tags,0,sizeof tags);my2k_mbc7_init(ram);strcpy(name,path+7);aux_disk_game_reads(true);
 if(setjmp(escape))return aux_gbc_stop();
 if(gb_init(&core,rom_read,ram_read,ram_write,error,NULL)!=GB_INIT_NO_ERROR){fault=8;return aux_gbc_stop();}
 if(metadata.battery&&aux_gbc_library_ram(name,ram,(uint8_t *)cache,metadata.ram_bytes,core.cart_rtc,false)<0){fault=13;return aux_gbc_stop();}
 /* Save reads borrow the cache only after emulation stops; the load path writes RAM directly. */
 gb_init_lcd(&core,row);core.direct.frame_skip=1;core.direct.interlace=0;aux_gbc_buttons(0);
 frames=simulated=last_ms=max_ms=completed_hash=fraction=0;next_frame=rtc_at=aux_millis();running=1;return 0;
}
void aux_gbc_service(uint32_t now){
 if(!running)return;
 if(aux_msc_export_active()||aux_msc_media_writing()){(void)aux_gbc_stop();return;}
 if(panel->color_blanked||aux_panel_color_busy(panel)){next_frame=now;return;}
 if((int32_t)(now-next_frame)<0)return;
 if((int32_t)(now-next_frame)>100)next_frame=now;
 fraction+=743;next_frame+=16+fraction/1000u;fraction%=1000;
 uint32_t began=aux_millis();steps=0;pixel_count=lines=0;hash=2166136261u;draw=true;core.direct.joypad=pad;
 if(setjmp(escape)){(void)aux_gbc_stop();return;}
 gb_run_frame(&core);simulated++;
 if(lines){while(pixel_count<16384)append(0);frames++;completed_hash=hash;}
 else if(!(core.hram_io[IO_LCDC]&128u)&&(simulated&1u)){while(pixel_count<16384)append(0xffff);frames++;completed_hash=hash;}
 while((uint32_t)(now-rtc_at)>=1000u){if(metadata.rtc)gb_tick_rtc(&core);rtc_at+=1000u;}
 last_ms=aux_millis()-began;if(last_ms>max_ms)max_ms=last_ms;
}
uint32_t aux_gbc_stat(unsigned p){return p==0?frames:p==1?simulated:p==2?(last_ms|(max_ms<<16)):p==3?(fault|((uint32_t)(sizeof core+sizeof store)<<8)):completed_hash;}
