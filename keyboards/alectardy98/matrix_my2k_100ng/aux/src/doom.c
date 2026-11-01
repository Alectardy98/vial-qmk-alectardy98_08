/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Supervisor/platform boundary for the separately licensed embeddedDOOM core.
 * No engine pointer crosses this boundary; NES and Doom RAM are exclusive. */
#include "aux_doom.h"
#include "aux_platform.h"
#include "aux_work.h"
#include "aux_assets.h"
#include "aux_animation_files.h"
#include "aux_msc.h"
#include "aux_disk_policy.h"
#include "aux_text.h"
#include "ff.h"
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
extern unsigned char __doom_bss_start[],__doom_bss_end[];
extern unsigned char __doom_data_start[],__doom_data_end[],__doom_data_load[];
extern jmp_buf my2k_doom_error_jump;
extern char my2k_doom_error_text[96];
extern void D_DoomMain(void),D_MY2KTick(void),D_Display(void),my2k_doom_set_buttons(unsigned);
extern unsigned my2k_doom_detail(void);
extern void my2k_doom_cache_buffers(uint8_t *,uint8_t *,uint8_t *);
extern int myargc;extern char **myargv;
static FIL game_files[3];
static DWORD link_maps[3][16];
volatile uint32_t doom_fast_seek_mask,doom_seek_ms,doom_data_ms;
static bool opened[3],active;
static bool cache_loan;
static aux_panels *panel;
static uint16_t pixels[256];
static uint32_t frames,tics,last_ms,max_ms,next_tic,completed_hash,read_calls,read_bytes,read_ms;
static unsigned fraction,fault;
static char last_error[96];
/* Diagnostics contain no copyrighted asset bytes. */
int my2k_doom_printf(const char *fmt,...){(void)fmt;return 0;}
int my2k_doom_puts(const char *s){(void)s;return 0;}
void my2k_doom_setbuf(void *f,char *p){(void)f;(void)p;}
bool aux_doom_active(void){return active;}
bool aux_doom_runnable(uint32_t now){return active&&(int32_t)(now-next_tic)>=0;}
void aux_doom_buttons(unsigned b){if(active)my2k_doom_set_buttons(b);}
static void close_files(void){for(unsigned i=0;i<3;i++)if(opened[i]){(void)f_close(&game_files[i]);opened[i]=false;}}
void aux_doom_stop(void){active=false;close_files();aux_disk_game_reads(false);if(cache_loan){aux_msc_game_release();cache_loan=false;}}
int my2k_doom_file_read(unsigned id,unsigned offset,void *dst,unsigned bytes){
 if(id>2||!opened[id]||offset>f_size(&game_files[id])||bytes>f_size(&game_files[id])-offset)return 0;
 uint32_t began=aux_millis();UINT got=0;
 bool okay=true;
 if(f_tell(&game_files[id])!=offset)okay=f_lseek(&game_files[id],offset)==FR_OK&&f_tell(&game_files[id])==offset;
 uint32_t after_seek=aux_millis();doom_seek_ms+=after_seek-began;
 if(okay)okay=f_read(&game_files[id],dst,bytes,&got)==FR_OK&&got==bytes;
 doom_data_ms+=aux_millis()-after_seek;
 read_calls++;read_bytes+=got;read_ms+=aux_millis()-began;aux_work_checkpoint();return okay;
}
int my2k_doom_present(const uint8_t *src,const uint16_t *palette){
 if(!panel||panel->color_blanked||aux_panel_color_busy(panel))return 0;
 uint32_t hash=2166136261u;
 for(unsigned off=0;off<16384;off+=256){
  for(unsigned i=0;i<256;i++){uint16_t c=palette[src[off+i]];pixels[i]=c;hash=(hash^c)*16777619u;}
  if(!aux_panel_color_pixels(panel,128,128,off,pixels,256))return 0;
  aux_work_checkpoint();
 }
 frames++;completed_hash=hash;return 1;
}
static bool validate_file(unsigned id,uint32_t expected){
 uint32_t crc=0xffffffffu;unsigned offset=0,total=(unsigned)f_size(&game_files[id]);
 /* Games are exclusive. Borrow4KiB of the game arena before initialization,
  * keeping validation off both the Main and worker stacks. */
 uint8_t *buffer=__doom_bss_start;
 while(offset<total){unsigned n=total-offset;if(n>4096)n=4096;UINT got=0;
  if(f_read(&game_files[id],buffer,n,&got)!=FR_OK||got!=n)return false;
  for(unsigned i=0;i<n;i++){crc^=buffer[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^((crc&1u)?0xedb88320u:0);}
  offset+=n;aux_work_checkpoint();
 }
 return (crc^0xffffffffu)==expected;
}
/* One bounded, SPI-completed splash before the read-only asset validation.
 * Reuse the existing glyph renderer and256-pixel output buffer. This writes
 * only the color panel: no asset upload, OLED state or persistent setting. */
static bool loading_screen(aux_panels *p){
 uint8_t bitmap[360]={0};
 const char *line[2]={"LOADING","DOOM..."};
 for(unsigned row=0;row<2;row++)for(unsigned i=0;line[row][i];i++)
  aux_text_glyph(bitmap,15u+6u*i,12u+12u*row,(uint8_t)line[row][i],1);
 for(unsigned off=0;off<16384;off+=256){
  for(unsigned i=0;i<256;i++){
   unsigned x=(off+i)%128,y=(off+i)/128;
   bool ink=false;
   if(x>=28&&x<100&&y>=44&&y<84){unsigned yy=y-44;ink=(bitmap[yy/8*72+x-28]>>(yy&7))&1u;}
   pixels[i]=ink?0xffffu:0x0841u;
  }
  if(!aux_panel_color_pixels(p,128,128,off,pixels,256))return false;
  aux_work_checkpoint();
 }
 return true;
}
int aux_doom_start(aux_panels *p){
 if(active)return 0;
 if(!p||p->color_streaming||p->color_blanked||aux_panel_color_busy(p)||aux_animation_files_busy()||aux_msc_export_active()||aux_msc_media_writing()||!aux_msc_io_idle()||!aux_assets_media_service())return 2;
 if(!loading_screen(p))return 8;
 close_files();doom_fast_seek_mask=doom_seek_ms=doom_data_ms=0;
 const char *names[]={"0:/doom/my2k.wad","0:/doom/columns.bin","0:/doom/opaque.bin"};const unsigned sizes[]={1527746,675136,1378304};
 for(unsigned i=0;i<3;i++){
  if(f_open(&game_files[i],names[i],FA_READ)!=FR_OK){close_files();return 3;}opened[i]=true;
  if(f_size(&game_files[i])!=sizes[i]||!validate_file(i,i==2?0xb49c613cu:i?0xf3f08b25u:0xa58b5b2bu)){close_files();return 4;}
 }
 /* The boundedCLMT eliminates repeated backward FAT-chain traversal. An
  * excessively fragmented file retains the safe ordinary-seek fallback. */
 for(unsigned i=0;i<3;i++){
  link_maps[i][0]=16;game_files[i].cltbl=link_maps[i];
  FRESULT result=f_lseek(&game_files[i],CREATE_LINKMAP);
  if(result==FR_OK)doom_fast_seek_mask|=1u<<i;
  else{game_files[i].cltbl=NULL;if(result!=FR_NOT_ENOUGH_CORE){close_files();return 4;}}
 }
 /* Engine globals, allocator, caches and framebuffer start fresh each entry.
  * Initialized globals are copied from an immutable flash template. */
 memset(__doom_bss_start,0,(size_t)(__doom_bss_end-__doom_bss_start));
 memcpy(__doom_data_start,__doom_data_load,(size_t)(__doom_data_end-__doom_data_start));
 uint8_t *cache_first,*cache_second;
 if(!aux_msc_game_buffers(&cache_first,&cache_second)){close_files();return 2;}
 /* The game supervisor excludes GBC while Doom is active. Its ITCM code
  * is loaded anew on every GBC start, so12KiB can be borrowed for CPU-only
  * cache copies here. Never pass ITCM to USB/SD DMA. Region checked at link. */
 extern uint32_t __gbc_itcm_start[];
 *(volatile uint32_t *)0xe000ef90u|=1u;
 __asm__ volatile("dsb\nisb" ::: "memory");
 cache_loan=true;my2k_doom_cache_buffers(cache_first,cache_second,(uint8_t *)__gbc_itcm_start);
 panel=p;frames=tics=last_ms=max_ms=read_calls=read_bytes=read_ms=completed_hash=0;fraction=fault=0;last_error[0]=0;
 static char *args[]={"my2k-doom","-warp","1","1","-skill","2"};myargc=6;myargv=args;
 int result=setjmp(my2k_doom_error_jump);
 if(result){fault=7;memcpy(last_error,my2k_doom_error_text,sizeof last_error);aux_doom_stop();return 7;}
 aux_disk_game_reads(true);D_DoomMain();active=true;next_tic=aux_millis();return 0;
}
void aux_doom_service(uint32_t now){
 if(!active)return;
 if(aux_msc_export_active()||aux_msc_media_writing()){aux_doom_stop();return;}
 if(panel->color_blanked||aux_panel_color_busy(panel)){next_tic=now;return;}
 if((int32_t)(now-next_tic)<0)return;
 if((int32_t)(now-next_tic)>1000)next_tic=now;
 uint32_t began=aux_millis();
 int result=setjmp(my2k_doom_error_jump);
 if(result){fault=result==2?0:7;if(fault)memcpy(last_error,my2k_doom_error_text,sizeof last_error);aux_doom_stop();return;}
 /* Fixed35Hz game time, independently limited by the actual renderer. At
  * most8catch-up tics; a large scheduling stall is bounded and re-anchored. */
 unsigned catchup=0;
 while((int32_t)(aux_millis()-next_tic)>=0&&catchup++<8){
  D_MY2KTick();tics++;
  unsigned step=28;fraction+=20;if(fraction>=35){fraction-=35;step++;}next_tic+=step;
 }
 if((int32_t)(aux_millis()-next_tic)>=0)next_tic=aux_millis()+29;
 D_Display();last_ms=aux_millis()-began;if(last_ms>max_ms)max_ms=last_ms;

}
uint32_t aux_doom_stat(unsigned n){return n==0?frames:n==1?tics:n==2?(last_ms|(max_ms<<16)):n==3?(fault|(my2k_doom_detail()<<8)):n==4?completed_hash:n==5?read_calls:n==6?read_bytes:n==7?read_ms:0;}
