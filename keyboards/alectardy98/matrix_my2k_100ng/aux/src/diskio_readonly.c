/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_platform.h"
#include "aux_msc.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>
#if MY2K_DOOM_PROFILE
#include "aux_disk_policy.h"
#include "aux_nes_library.h"
static bool game_reads;
#endif
#if MY2K_SD_BULK8
static uint8_t read_ahead[4096] __attribute__((aligned(4)));
static uint32_t cached_lba,cached_epoch;
static unsigned cached_count;
volatile uint32_t aux_disk_cache_hits,aux_disk_cache_misses;
#endif
#if MY2K_DOOM_PROFILE
void aux_disk_game_reads(bool enabled){
 if(game_reads!=enabled){game_reads=enabled;
#if MY2K_SD_BULK8
  cached_count=0;
#endif
 }
}
#endif
#if MY2K_RELEASE1_PROFILE
#include "aux_oled_store.h"
#if MY2K_ANIMATION_PROFILE
#include "aux_animation_files.h"
#endif
#endif
DSTATUS disk_initialize(BYTE drive){return drive==0&&aux_sd_sectors()?0:STA_NOINIT;}
DSTATUS disk_status(BYTE drive){return drive==0&&aux_sd_sectors()?0:STA_NOINIT;}
DRESULT disk_read(BYTE drive,BYTE *dst,LBA_t sector,UINT count){
    if(drive||!dst||!count||sector>=aux_sd_sectors()||count>aux_sd_sectors()-sector)return RES_PARERR;
    for(UINT n=0;n<count;n++){
        /* Raw MSC reads cannot re-enter FatFs and own SD only between sectors. */
#if MY2K_RELEASE1_PROFILE
        if(!aux_oled_store_writing()
#if MY2K_DOOM_PROFILE
           &&!aux_nes_library_writing()
#endif
#if MY2K_ANIMATION_PROFILE
           &&!aux_animation_files_writing()
#endif
           )
#endif
        aux_msc_worker_service();
#if MY2K_SD_BULK8
        uint32_t lba=(uint32_t)(sector+n),epoch=aux_sd_content_epoch();
        if(!cached_count||cached_epoch!=epoch||lba<cached_lba||lba-cached_lba>=cached_count){
            /* A multi-sector FatFs request need not start at an absolute
             * eight-sector boundary. Start its window at the requested LBA
             * so one4KiB read does not unnecessarily fetch two4KiB windows.
             * Single-sector lookups retain the existing aligned read-ahead. */
            aux_disk_cache_misses++;cached_count=0;cached_lba=count-n>1u?lba:lba&~7u;
#if MY2K_DOOM_PROFILE
            if(game_reads)cached_lba=lba;
#endif
            unsigned take=aux_sd_sectors()-cached_lba;if(take>8u)take=8u;
#if MY2K_DOOM_PROFILE
            if(game_reads&&take>count-n)take=count-n;
#endif
            if(!aux_sd_read_blocks(cached_lba,read_ahead,take)||epoch!=aux_sd_content_epoch())return RES_ERROR;
            cached_epoch=epoch;cached_count=take;
        }else aux_disk_cache_hits++;
        memcpy(dst+n*512u,read_ahead+(lba-cached_lba)*512u,512u);
#else
        if(!aux_sd_read(sector+n,dst+n*512u))return RES_ERROR;
#endif
    }
    return RES_OK;
}
DRESULT disk_ioctl(BYTE drive,BYTE command,void *buffer){
    if(drive||!aux_sd_sectors())return RES_NOTRDY;
    switch(command){
    case CTRL_SYNC:return RES_OK; /* No pending writes exist. */
    case GET_SECTOR_COUNT:if(!buffer)return RES_PARERR;*(LBA_t*)buffer=aux_sd_sectors();return RES_OK;
    case GET_SECTOR_SIZE:if(!buffer)return RES_PARERR;*(WORD*)buffer=512;return RES_OK;
    case GET_BLOCK_SIZE:if(!buffer)return RES_PARERR;*(DWORD*)buffer=1;return RES_OK;
    default:return RES_PARERR;
    }
}
/* FF_FS_READONLY removes both disk_write references and file mutation APIs. */

#if MY2K_RELEASE1_PROFILE
DRESULT disk_write(BYTE drive,const BYTE *data,LBA_t sector,UINT count){
 bool owner=aux_oled_store_writing();
#if MY2K_DOOM_PROFILE
 owner=owner||aux_nes_library_writing();
#endif
#if MY2K_ANIMATION_PROFILE
 owner=owner||aux_animation_files_writing();
#endif
 if(!owner)return RES_WRPRT;
 if(drive||!data||!count||sector>=aux_sd_sectors()||count>aux_sd_sectors()-sector)return RES_PARERR;
 #if MY2K_SD_BULK8
 for(UINT i=0;i<count;){UINT n=count-i>8u?8u:count-i;if(!aux_sd_write_blocks(sector+i,data+512u*i,n))return RES_ERROR;i+=n;}
#else
 for(UINT i=0;i<count;i++)if(!aux_sd_write(sector+i,data+512u*i))return RES_ERROR;
#endif
 return RES_OK;
}
#endif
