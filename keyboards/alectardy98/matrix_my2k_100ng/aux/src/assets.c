/* SPDX-License-Identifier: GPL-2.0-or-later */
#if MY2K_DEV22_PROFILE
#include "aux_perf22.h"
#endif
#include "aux_assets.h"
#include "my2k_asset.h"
#include "ff.h"
#include "aux_animation_files.h"
#include "aux_animation_clock.h"
#include "aux_display_test.h"
#include "aux_platform.h"
#include <limits.h>
#include <string.h>
#if AUX_STOCK_PARITY
#include "aux_msc.h"
static uint32_t media_epoch;
static char selected_color[100];
#endif
static FATFS volume;
static bool mounted,paused;
static bool color_enabled=true,mono_enabled=true;
typedef struct {FIL file;my2k_asset asset;uint16_t frame;uint32_t last;bool ready,started,present,decoding,output_ok;uint32_t next_pixel,read_errors,output_failures;
    uint32_t deadline;uint16_t rate_remainder;bool rate_clock_valid;
#if AUX_STOCK_PARITY
    bool retry_pending;uint32_t retry_at;
#endif
} channel;
static channel color,mono;
static aux_animation_clock color_clock;
#ifndef AUX_FRAME_RATE_DEFAULT
#define AUX_FRAME_RATE_DEFAULT 0
#endif
static uint8_t frame_rate=AUX_FRAME_RATE_DEFAULT;
#ifndef AUX_ORIGINAL_TIMING_DEFAULT
#define AUX_ORIGINAL_TIMING_DEFAULT 0
#endif
static bool original_timing=AUX_ORIGINAL_TIMING_DEFAULT;
bool aux_assets_set_frame_rate(uint8_t rate){
    if(rate!=30&&rate!=60)return false;
    frame_rate=rate;color.rate_clock_valid=false;color.rate_remainder=0;return true;
}
uint8_t aux_assets_frame_rate(void){return frame_rate;}
bool aux_assets_set_timing(uint8_t policy){
    if(policy>1u)return false;
    bool original=policy==0;
    if(original!=original_timing){original_timing=original;color_clock.started=false;color.rate_clock_valid=false;}
    return true;
}
uint8_t aux_assets_timing(void){return original_timing?0u:1u;}
void aux_assets_set_test(bool enabled){
    if(enabled==aux_display_test_active())return;
    aux_display_test_set(enabled,aux_millis());
    color.decoding=false;color.started=false;color.next_pixel=0;color.rate_clock_valid=false;color_clock.started=false;
}
static uint32_t mount_attempts,mount_failures,mono_open_failures,color_open_failures;
static uint8_t last_mount_result,last_mono_open_result,last_color_open_result;
volatile uint32_t aux_decoded_color_frames,aux_decoded_mono_frames;
volatile uint32_t aux_render_failures,aux_asset_errors;
static bool read_at_impl(void *context,uint32_t offset,void *dst,size_t size){
    FIL *f=context;UINT got=0;
#if AUX_STOCK_PARITY
    if(aux_msc_media_writing()||aux_msc_media_generation()!=media_epoch)return false;
#endif
    if(size>UINT_MAX)return false;
#if MY2K_ASSET_BATCH128
    if(f_tell(f)!=offset)
#endif
    {
#if MY2K_DEV22_PROFILE
        aux_perf22_count(PERF22_SEEK_CALL);
#endif
        if(f_lseek(f,offset)!=FR_OK)return false;
    }
    bool ok=f_read(f,dst,(UINT)size,&got)==FR_OK&&got==size;
#if AUX_STOCK_PARITY
    if(aux_msc_media_writing()||aux_msc_media_generation()!=media_epoch)return false;
#endif
    return ok;
}
static bool read_at(void *context,uint32_t offset,void *dst,size_t size){
#if MY2K_DEV22_PROFILE
    aux_perf22_count(PERF22_FILE_CALL);bool scope=aux_perf22_enter(PERF22_FILE);
#endif
    bool ok=read_at_impl(context,offset,dst,size);
#if MY2K_DEV22_PROFILE
    aux_perf22_leave(scope);
#endif
    return ok;
}
static bool open_asset(channel *c,const char *path,my2k_asset_kind expected){
    FRESULT opened=f_open(&c->file,path,FA_READ);
    if(expected==MY2K_ASSET_ABIT){last_mono_open_result=(uint8_t)opened;if(opened!=FR_OK)mono_open_failures++;}
    else{last_color_open_result=(uint8_t)opened;if(opened!=FR_OK)color_open_failures++;}
    if(opened!=FR_OK)return false;
    if(f_size(&c->file)>UINT32_MAX){f_close(&c->file);return false;}
    my2k_reader reader={&c->file,(uint32_t)f_size(&c->file),read_at};
    if(!my2k_asset_open(&reader,&c->asset)||c->asset.kind!=expected){f_close(&c->file);return false;}
    if(c==&color){
        aux_animation_clock_clear(&color_clock);
        if(c->asset.frames>AUX_TIMELINE_FRAMES){f_close(&c->file);return false;}
        for(unsigned i=0;i<c->asset.frames;i++){
            uint16_t delay;
            if(!my2k_asset_frame_interval(&c->asset,(uint16_t)i,&delay)||!aux_animation_clock_add(&color_clock,delay)){f_close(&c->file);return false;}
        }
    }
    c->ready=true;c->present=true;c->started=false;c->decoding=false;c->next_pixel=0;c->frame=0;c->last=0;return true;
}
static bool is_crs(const char *p){
    size_t n=0;while(p[n])n++;
    if(n<4)return false;
    return p[n-4]=='.'&&(p[n-3]=='c'||p[n-3]=='C')&&
        (p[n-2]=='r'||p[n-2]=='R')&&(p[n-1]=='s'||p[n-1]=='S');
}
bool aux_assets_init(void){
#if AUX_STOCK_PARITY
    if(aux_msc_media_writing())return false;
    if(color.present)(void)f_close(&color.file);
    if(mono.present)(void)f_close(&mono.file);
    color=(channel){0};mono=(channel){0};media_epoch=aux_msc_media_generation();
#endif
    mounted=false;
    mount_attempts++;last_mount_result=(uint8_t)f_mount(&volume,"0:",1);
    if(last_mount_result!=FR_OK){mount_failures++;return false;}
    mounted=true;
    (void)open_asset(&mono,"0:/BW_TEXT.ABW",MY2K_ASSET_ABIT);
#if AUX_STOCK_PARITY
    if(selected_color[0]&&open_asset(&color,selected_color,MY2K_ASSET_ANIM))return color.ready||mono.ready;
#endif
    DIR dir;FILINFO entry;
    if(f_opendir(&dir,"0:/")==FR_OK){
        for(unsigned n=0;n<256;n++){
            if(f_readdir(&dir,&entry)!=FR_OK||!entry.fname[0])break;
            if((entry.fattrib&AM_DIR)||!is_crs(entry.fname))continue;
            char path[100]="0:/";size_t i=0;
            while(entry.fname[i]&&i<96){path[3+i]=entry.fname[i];i++;}path[3+i]=0;
            if(open_asset(&color,path,MY2K_ASSET_ANIM)){
#if AUX_STOCK_PARITY
                memcpy(selected_color,path,strlen(path)+1);
#endif
                break;
            }
        }
        f_closedir(&dir);
    }
#if MY2K_ANIMATION_PROFILE
    /* After an explicit legacy import the root may contain no CRS. Discover
     * an existing numbered slot without writing or requiring Main EEPROM. */
    if(!color.ready)for(unsigned slot=1;slot<=10;slot++){
        char path[64];if(!aux_animation_slot_path(slot,path))continue;
        if(open_asset(&color,path,MY2K_ASSET_ANIM)){memcpy(selected_color,path,strlen(path)+1);break;}
    }
#endif
    return color.ready||mono.ready;
}
bool aux_assets_select_color(const char *filename){
    if(!mounted||!filename)return false;
    char path[100]="0:/";size_t n=0;
    while(filename[n]){
        if(n>=96||filename[n]=='\\'||filename[n]==':'||(filename[n]=='/'&&(n!=10||strncmp(filename,"anamations/",11))))return false;
        path[3+n]=filename[n];n++;
    }
    path[3+n]=0;
    if(!is_crs(path))return false;
    /* Validate before disturbing an in-progress animation. No borrowed stack reader escapes. */
    channel probe={0};
    if(!open_asset(&probe,path,MY2K_ASSET_ANIM))return false;
    f_close(&probe.file);
    if(color.present)f_close(&color.file);
    color=(channel){0};
    bool ok=open_asset(&color,path,MY2K_ASSET_ANIM);
#if AUX_STOCK_PARITY
    if(ok)memcpy(selected_color,path,strlen(path)+1);
#endif
    return ok;
}
#if MY2K_ANIMATION_PROFILE
bool aux_assets_release_color(void){if(color.present&&f_close(&color.file)!=FR_OK)return false;color=(channel){0};return true;}
const char *aux_assets_selected_color(void){return selected_color[0]?selected_color+3:NULL;}
#endif
#if AUX_STOCK_PARITY
bool aux_assets_media_service(void){
#if MY2K_RELEASE1_PROFILE
 if(aux_msc_export_active())return false;
#endif
    if(aux_msc_media_writing())return false;
    if(aux_msc_media_generation()!=media_epoch){
        /* Called only between worker FatFs operations. Reopen FIL metadata and
         * FAT windows after the host's completed write+SYNC generation. */
        return aux_assets_init();
    }
    return mounted;
}
bool aux_assets_reload_mono(void){
    if(!aux_assets_media_service())return false;
    /* Drop the shared sector cache as the host may have replaced BW_TEXT's
     * directory entry, FAT chain and size. Do not borrow a stack FIL reader. */
    volume.winsect=(LBA_t)-1;
    channel probe={0};
    if(!open_asset(&probe,"0:/BW_TEXT.ABW",MY2K_ASSET_ABIT))return false;
    bool dimensions=probe.asset.width<=72&&probe.asset.height<=40;
    (void)f_close(&probe.file);if(!dimensions)return false;
    if(mono.present)(void)f_close(&mono.file);
    mono=(channel){0};return open_asset(&mono,"0:/BW_TEXT.ABW",MY2K_ASSET_ABIT);
}
#endif
static void render(channel *c,uint32_t now,aux_pixel_sink sink,void *ctx,volatile uint32_t *count){
    if(!c->ready)return;
    if(!c->decoding){
        uint16_t interval;
        bool natural=c==&color&&frame_rate&&original_timing;
        uint16_t timed_frame=0;
        if(c==&color&&frame_rate){
            if(c->started&&c->rate_clock_valid&&(int32_t)(now-c->deadline)<0)return;
            if(natural){
                timed_frame=aux_animation_clock_frame(&color_clock,now);
                if(timed_frame==UINT16_MAX)return;
                if(c->started&&timed_frame==c->frame)return; /* hold without rereading/resending */
                c->deadline=now;
            }else if(!c->rate_clock_valid||(int32_t)(now-c->deadline)>100)c->deadline=now;
            c->rate_clock_valid=true;
            interval=1000u/frame_rate;c->rate_remainder+=(uint16_t)(1000u%frame_rate);
            if(c->rate_remainder>=frame_rate){c->rate_remainder-=frame_rate;interval++;}
            c->deadline+=interval;
        }else if(!my2k_asset_frame_interval(&c->asset,c->frame,&interval)){
            aux_asset_errors++;c->read_errors++;c->ready=false;
#if AUX_STOCK_PARITY
            if(c==&color){c->retry_pending=true;c->retry_at=now+1000u;}
#endif
            return;
        }
        if(!(c==&color&&frame_rate)&&c->started&&!my2k_asset_due(now,c->last,interval))return;
        if(natural)c->frame=timed_frame;
        else if(c->started)c->frame=(uint16_t)((c->frame+1u)%c->asset.frames);
        c->next_pixel=0;c->decoding=true;c->output_ok=sink!=0;c->last=now;
#if MY2K_DEV22_PROFILE
        if(c==&color)aux_perf22_begin(interval);
#endif
    }
    uint32_t total=(uint32_t)c->asset.width*c->asset.height;
#ifndef AUX_RENDER_PIXELS
#define AUX_RENDER_PIXELS 128u
#endif
    uint16_t pixels[AUX_RENDER_PIXELS];
    unsigned size=total-c->next_pixel>AUX_RENDER_PIXELS?AUX_RENDER_PIXELS:(unsigned)(total-c->next_pixel);
#if MY2K_DEV22_PROFILE
    bool scope=false;if(c==&color){aux_perf22_count(PERF22_PIXEL_CALL);scope=aux_perf22_enter(PERF22_DECODE);}
#endif
    bool decoded=my2k_asset_pixels(&c->asset,c->frame,c->next_pixel,size,pixels);
#if MY2K_DEV22_PROFILE
    aux_perf22_leave(scope);
#endif
    if(!decoded){
#if MY2K_DEV22_PROFILE
        if(c==&color)aux_perf22_end(false);
#endif
        aux_asset_errors++;c->read_errors++;c->ready=false;c->decoding=false;
#if AUX_STOCK_PARITY
        if(c==&color){c->retry_pending=true;c->retry_at=now+1000u;}
#endif
        return;
    }
    if(!sink||!sink(ctx,c->asset.width,c->asset.height,c->next_pixel,pixels,size))c->output_ok=false;
    c->next_pixel+=size;
    if(c->next_pixel==total){
        (*count)++;if(!c->output_ok){aux_render_failures++;c->output_failures++;}
#if MY2K_DEV22_PROFILE
        if(c==&color)aux_perf22_end(c->output_ok);
#endif
        c->decoding=false;c->started=true;
        if(c->asset.kind==MY2K_ASSET_ABIT)c->ready=false; /* One immutable text asset per boot. */
    }
}
void aux_assets_step(uint32_t now,const aux_render_sinks *sinks){
#if MY2K_ANIMATION_PROFILE
    /* FatFs may flush a dirty shared FAT window even on another FIL's read.
     * Keep the last displayed frame while a staged animation upload owns the
     * local filesystem. Commit/abort flushes it before ordinary reads resume. */
    if(aux_animation_files_busy())return;
#endif
    if(aux_display_test_active()&&!paused){
        if(color_enabled)aux_display_test_step(now,sinks?sinks->color:0,sinks?sinks->context:0);
        return; /* Native diagnostic needs neither a mounted filesystem nor an asset. */
    }
#if AUX_STOCK_PARITY
    if(!aux_assets_media_service())return;
#endif
    if(paused)return;
#if AUX_STOCK_PARITY
    if(color_enabled&&color.retry_pending&&(int32_t)(now-color.retry_at)>=0&&aux_msc_io_idle()){
        /* A transient file read must not permanently kill animation. Reopen
         * only the selected color FIL; never reset card, volume or mono image.
         * media_service above already excludes an active host-write lease. */
        color.retry_at=now+1000u;
        if(color.present)(void)f_close(&color.file);
        color.present=false;
        if(selected_color[0]&&open_asset(&color,selected_color,MY2K_ASSET_ANIM))color.retry_pending=false;
    }
#endif
    if(color_enabled)render(&color,now,sinks?sinks->color:0,sinks?sinks->context:0,&aux_decoded_color_frames);
    if(mono_enabled)render(&mono,now,sinks?sinks->mono:0,sinks?sinks->context:0,&aux_decoded_mono_frames);
}
bool aux_assets_busy(void){return !paused&&((color_enabled&&aux_display_test_runnable(aux_millis()))||(color_enabled&&color.decoding)||(mono_enabled&&mono.decoding));}
void aux_assets_set_outputs(bool color_on,bool mono_on,uint32_t now){
    if(color_on&&!color_enabled&&color.ready){
        color.next_pixel=0;color.decoding=true;color.output_ok=true;color.last=now;
    }
    color_enabled=color_on;mono_enabled=mono_on;
}
void aux_assets_set_paused(bool pause,uint32_t now){
    if(paused&&!pause){color.last=now;mono.last=now;}
    paused=pause;
}
void aux_assets_status(aux_asset_status *out){
    if(!out)return;
    *out=(aux_asset_status){
        .mounted=mounted,.paused=paused,.color_present=color.present,.mono_present=mono.present,
        .color_active=color.ready,.mono_active=mono.ready,.color_decoding=color.decoding,.mono_decoding=mono.decoding,
        .color_frame=color.frame,.color_width=color.asset.width,.color_height=color.asset.height,
        .mono_width=mono.asset.width,.mono_height=mono.asset.height,
        .color_next_pixel=color.next_pixel,.mono_next_pixel=mono.next_pixel,
        .color_completed=aux_decoded_color_frames,.mono_completed=aux_decoded_mono_frames,
        .output_failures=aux_render_failures,.read_errors=aux_asset_errors,
        .mono_read_errors=mono.read_errors,.mono_output_failures=mono.output_failures,
        .color_read_errors=color.read_errors,.color_output_failures=color.output_failures,
        .mount_attempts=mount_attempts,.mount_failures=mount_failures,
        .mono_open_failures=mono_open_failures,.color_open_failures=color_open_failures,
        .last_mount_result=last_mount_result,.last_mono_open_result=last_mono_open_result,.last_color_open_result=last_color_open_result};
}

#if MY2K_RELEASE1_PROFILE
bool aux_assets_export_begin(void){
 if(aux_msc_export_active())return true;
#if MY2K_ANIMATION_PROFILE
 if(!aux_animation_files_export_ready())return false;
#endif
 if(color.present){if(f_close(&color.file)!=FR_OK)return false;color.present=false;}
 if(mono.present){if(f_close(&mono.file)!=FR_OK)return false;mono.present=false;}
 paused=true;color.ready=mono.ready=false;
 if(f_mount(NULL,"0:",0)!=FR_OK)return false;
 mounted=false;aux_msc_set_export(true);return true;
}
#endif

#if MY2K_RELEASE1_PROFILE
bool aux_assets_release_mono(void){if(mono.present&&f_close(&mono.file)!=FR_OK)return false;mono.present=false;mono.ready=false;mono.decoding=false;mono.started=false;mono.next_pixel=0;return true;}
#endif
