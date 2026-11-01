/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise the actual game supervisor with RAM-only core/filesystem fakes. */
#include "aux_nes.h"
#include "aux_gbc.h"
#include "aux_doom.h"
#include "my2k_nes_core.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
unsigned char __nes_bss_start[16],__gbc_bss_start[16];
__asm__(".global __nes_bss_end\n.set __nes_bss_end,__nes_bss_start+16\n.global __gbc_bss_end\n.set __gbc_bss_end,__gbc_bss_start+16");
static unsigned engine,starts,stops;
static bool held,fail_start,busy,fail_save;
uint32_t aux_millis(void){return 100;}
void aux_work_checkpoint(void){}
void aux_disk_game_reads(bool enabled){(void)enabled;}
bool aux_animation_files_busy(void){return busy;}
bool aux_msc_export_active(void){return false;}
bool aux_msc_media_writing(void){return false;}
bool aux_assets_media_service(void){return true;}
void aux_assets_hold_color(void){held=true;}
void aux_assets_allow_color(void){held=false;}
bool aux_panel_color_busy(const aux_panels *p){(void)p;return false;}
bool aux_panel_color_pixels(void *p,uint16_t w,uint16_t h,uint32_t first,const uint16_t *pixels,unsigned count){(void)p;(void)w;(void)h;(void)first;(void)pixels;(void)count;return true;}
bool aux_nes_library_selected(char path[160]){strcpy(path,"0:/nes/test.nes");return true;}
bool aux_nes_library_ready(void){return true;}
void aux_nes_library_close_failed(void){}
int aux_nes_library_ram(const char *n,uint8_t ram[8192],uint8_t scratch[8192],bool save){(void)n;(void)ram;(void)scratch;(void)save;return 0;}
FRESULT f_open(FIL *f,const TCHAR *path,BYTE mode){(void)path;(void)mode;memset(f,0,sizeof *f);f->obj.objsize=16;return FR_OK;}
FRESULT f_close(FIL *f){(void)f;return FR_OK;}
FRESULT f_read(FIL *f,void *out,UINT n,UINT *got){memset(out,0,n);f->fptr+=n;*got=n;return FR_OK;}
FRESULT f_lseek(FIL *f,FSIZE_t at){f->fptr=at;return FR_OK;}
FRESULT f_opendir(DIR *d,const TCHAR *path){(void)d;(void)path;return FR_NO_PATH;}
FRESULT f_readdir(DIR *d,FILINFO *i){(void)d;(void)i;return FR_NO_FILE;}
FRESULT f_closedir(DIR *d){(void)d;return FR_OK;}
int my2k_nes_rom_parse(const uint8_t h[16],uint32_t size,my2k_nes_rom_info *i){(void)h;(void)size;memset(i,0,sizeof *i);return 0;}
void my2k_nes_core_callbacks(my2k_nes_line_sink s,void (*yield)(void)){(void)s;(void)yield;}
int my2k_nes_core_load(uint8_t *rom,size_t n){(void)rom;(void)n;assert(!engine&&held);engine=1;starts++;return 0;}
int my2k_nes_core_load_stream(const uint8_t h[16],uint32_t n,uint8_t *w,size_t size,my2k_nes_rom_reader r){(void)h;(void)n;(void)w;(void)size;(void)r;return 4;}
uint8_t *my2k_nes_battery_ram(void){return NULL;}
void my2k_nes_core_stop(void){assert(engine==1);engine=0;stops++;}
void my2k_nes_core_step(int draw){(void)draw;assert(engine==1);}
void my2k_nes_core_buttons(uint8_t b){(void)b;}
unsigned my2k_nes_core_fault(void){return 0;}
unsigned my2k_nes_core_heap_used(void){return 0;}
unsigned aux_gbc_start(aux_panels *p){(void)p;assert(!engine&&held);if(fail_start)return 3;engine=3;starts++;return 0;}
unsigned aux_gbc_stop(void){assert(engine==3);engine=0;stops++;return fail_save?13:0;}
bool aux_gbc_active(void){return engine==3;}
bool aux_gbc_runnable(uint32_t n){(void)n;return false;}
void aux_gbc_service(uint32_t n){(void)n;}
void aux_gbc_buttons(uint8_t b){(void)b;}
uint32_t aux_gbc_stat(unsigned n){(void)n;return 0;}
int aux_doom_start(aux_panels *p){(void)p;assert(!engine&&held);if(fail_start)return 3;engine=2;starts++;return 0;}
void aux_doom_stop(void){assert(engine==2);engine=0;stops++;}
bool aux_doom_active(void){return engine==2;}
bool aux_doom_runnable(uint32_t n){(void)n;return false;}
void aux_doom_service(uint32_t n){(void)n;}
void aux_doom_buttons(unsigned b){(void)b;}
uint32_t aux_doom_stat(unsigned n){(void)n;return 0;}
static my2k_wire_message request(unsigned arg,unsigned part){my2k_wire_message q={.type=MY2K_AC,.v={16,1,arg,part}},r;assert(aux_nes_request(&q,&r));return r;}
int main(void){
 aux_panels p={0};aux_nes_service(&p,0);
 my2k_wire_message r=request(2,1);assert(!r.v[3]&&(r.v[6]&MY2K_GAME_ATOMIC_HANDOFF));
 /* Invalid commands and a partial animation transfer must not seize GRAM. */
 r=request(5,2);assert(r.v[3]&&!starts&&!held);
 r=request(2,2);assert(r.v[3]&&!starts&&!held);
 p.color_streaming=true;r=request(1,2);assert(r.v[3]==2&&!held&&!starts);p.color_streaming=false;
 const unsigned arg[]={1,4,3,4,1,3,1},mode[]={1,3,2,3,1,2,1};
 for(unsigned i=0;i<7;i++){r=request(arg[i],2);assert(!r.v[3]&&r.v[4]==mode[i]&&engine==mode[i]&&aux_nes_active()&&held);}
 assert(starts==7&&stops==6);
 r=request(0,2);assert(!r.v[3]&&!engine&&!held&&!aux_nes_active());
 /* A failed target load retains ownership: never expose a GIF on failure. */
 r=request(4,2);assert(!r.v[3]);fail_start=true;r=request(3,2);assert(r.v[3]==3&&!engine&&held);fail_start=false;
 r=request(0,2);assert(!r.v[3]&&!held);
 /* Save failures stop the handoff instead of launching another game. */
 r=request(4,2);assert(!r.v[3]);fail_save=true;unsigned before=starts;r=request(3,2);assert(r.v[3]==13&&starts==before&&held&&!engine);fail_save=false;
 r=request(0,2);assert(!r.v[3]&&!held);
 puts("PASS AUX all six game handoffs, GRAM boundary, protocol rejection, failed load/save hold and explicit normal return");
}
