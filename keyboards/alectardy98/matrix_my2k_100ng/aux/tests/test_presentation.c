/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Actual asset/animation modules; a generated RAM filesystem, no device I/O. */
#include "aux_animation.h"
#include "aux_assets.h"
#include "aux_msc.h"
#include "aux_display_test.h"
#include "ff.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned color_calls,mono_calls;
static uint16_t first_pixel;
uint32_t aux_millis(void){return 0;}
bool aux_msc_export_active(void){return false;}
bool aux_msc_media_writing(void){return false;}
bool aux_msc_io_idle(void){return true;}
uint32_t aux_msc_media_generation(void){return 1;}
bool aux_animation_files_busy(void){return false;}
bool aux_animation_slot_path(unsigned slot,char path[64]){if(slot!=2)return false;strcpy(path,"0:/SLOT02.CRS");return true;}
bool aux_display_test_active(void){return false;}
bool aux_display_test_runnable(uint32_t now){(void)now;return false;}
void aux_display_test_set(bool on,uint32_t now){(void)on;(void)now;}
void aux_display_test_step(uint32_t now,aux_pixel_sink sink,void *ctx){(void)now;(void)sink;(void)ctx;}
void aux_display_test_status(uint8_t out[4]){memset(out,0,4);}
static unsigned file_id(const char *p){return !strcmp(p,"0:/BW_TEXT.ABW")?1:!strcmp(p,"0:/MEW.CRS")?2:!strcmp(p,"0:/SLOT02.CRS")?3:0;}
static unsigned file_size(unsigned id){return id==1?23:32790;}
FRESULT f_mount(FATFS *fs,const TCHAR *path,BYTE opt){(void)fs;(void)path;(void)opt;return FR_OK;}
FRESULT f_open(FIL *f,const TCHAR *path,BYTE mode){(void)mode;unsigned id=file_id(path);if(!id)return FR_NO_FILE;memset(f,0,sizeof *f);f->clust=id;f->obj.objsize=file_size(id);return FR_OK;}
FRESULT f_close(FIL *f){(void)f;return FR_OK;}
FRESULT f_lseek(FIL *f,FSIZE_t at){if(at>f_size(f))return FR_INT_ERR;f->fptr=at;return FR_OK;}
FRESULT f_read(FIL *f,void *out,UINT n,UINT *got){
 unsigned id=f->clust,size=file_size(id);uint8_t *b=out;
 if(n>size-f->fptr)n=(UINT)(size-f->fptr);
 uint8_t h[22]={'A',id==1?'B':'N',id==1?'I':'I',id==1?'T':'M',20,0,22,0,0,0,0,0,id==1?8:128,0,id==1?1:128,0,2,0,1,0,16,0};
 for(unsigned i=0;i<4;i++)h[8+i]=(uint8_t)(size>>(8*i));
 for(unsigned i=0;i<n;i++){unsigned at=(unsigned)f->fptr+i;b[i]=at<22?h[at]:id==1?1:id==2?0x11:0x22;}
 f->fptr+=n;*got=n;return FR_OK;
}
FRESULT f_opendir(DIR *d,const TCHAR *p){(void)p;memset(d,0,sizeof *d);return FR_OK;}
FRESULT f_readdir(DIR *d,FILINFO *i){memset(i,0,sizeof *i);if(!d->dptr){strcpy(i->fname,"MEW.CRS");i->fsize=32790;}d->dptr++;return FR_OK;}
FRESULT f_closedir(DIR *d){(void)d;return FR_OK;}
FRESULT f_stat(const TCHAR *p,FILINFO *i){unsigned id=file_id(p);if(!id)return FR_NO_FILE;memset(i,0,sizeof *i);i->fsize=file_size(id);strcpy(i->fname,p+3);return FR_OK;}
static bool color(void *ctx,uint16_t w,uint16_t h,uint32_t first,const uint16_t *p,unsigned n){(void)ctx;assert(w==128&&h==128&&n);if(!first)first_pixel=p[0];color_calls++;return true;}
static bool mono(void *ctx,uint16_t w,uint16_t h,uint32_t first,const uint16_t *p,unsigned n){(void)ctx;(void)p;assert(w==8&&h==1&&!first&&n==8);mono_calls++;return true;}
static void pump(void){aux_render_sinks s={color,mono,NULL};for(unsigned i=0;i<140;i++)aux_assets_step(0,&s);}
static my2k_wire_message request(unsigned op,unsigned arg,unsigned part){my2k_wire_message q={.type=MY2K_AC,.v={op,1,arg,part}},r;assert(aux_animation_request(&q,&r));return r;}
int main(int argc,char **argv){
 assert(argc==2&&aux_assets_init());pump();assert(!color_calls&&mono_calls==1&&!aux_assets_busy());
 my2k_wire_message r=request(8,1,!strcmp(argv[1],"legacy")?0:5);
 assert(!r.v[3]&&!memcmp(r.v+4,!strcmp(argv[1],"legacy")?"ANI4":"ANI5",4));
 if(!strcmp(argv[1],"legacy")){pump();assert(color_calls&&first_pixel==0x1111);return 0;}
 pump();assert(!color_calls);
 r=request(11,60,0);assert(!r.v[3]);pump();assert(!color_calls);
 r=request(9,7,0);assert(r.v[3]==3);pump();assert(!color_calls);
 r=request(9,!strcmp(argv[1],"default")?0:2,0);assert(!r.v[3]);pump();assert(color_calls&&first_pixel==(!strcmp(argv[1],"default")?0x1111:0x2222));
 /* Remounts preserve the chosen file, not a newly discovered legacy asset. */
 assert(aux_assets_init());pump();assert(first_pixel==(!strcmp(argv[1],"default")?0x1111:0x2222));
 unsigned before=color_calls;aux_assets_hold_color();assert(aux_assets_init());pump();assert(color_calls==before&&!aux_assets_busy());
 aux_assets_allow_color();pump();assert(color_calls>before&&first_pixel==(!strcmp(argv[1],"default")?0x1111:0x2222));
 puts("PASS: boot gate, OLED independence, ANI5 restore/default/invalid slot, legacy compatibility and remount selection");
}
