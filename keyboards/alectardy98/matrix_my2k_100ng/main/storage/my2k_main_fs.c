/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_main_fs.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>
#include <limits.h>
static my2k_main_fs_backend backend;
static FATFS                fs;
static FIL                  handles[4];
static char                 names[4][13];
static bool                 used[4], mounted;
#if MY2K_STOCK_PARITY_PROFILE
static bool writable[4];
static bool delete_replay_valid;
static uint8_t delete_request[30],delete_response[32];
#endif
static uint32_t             sectors, generation;
static uint32_t             sector_budget = UINT32_MAX;
static void                 put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool current(void) {
    uint32_t n, g;
    return backend.media && backend.media(backend.context, &n, &g) && n == sectors && g == generation;
}
void my2k_main_fs_reset(void) {
    (void)f_mount(NULL, "", 0);
    memset(&fs, 0, sizeof fs);
    memset(handles, 0, sizeof handles);
    memset(used, 0, sizeof used);
    memset(names, 0, sizeof names);
#if MY2K_STOCK_PARITY_PROFILE
    memset(writable,0,sizeof writable);
    delete_replay_valid=false;
#endif
    mounted = false;
}
void my2k_main_fs_init(const my2k_main_fs_backend *b) {
    my2k_main_fs_reset();
    backend = b ? *b : (my2k_main_fs_backend){0};
}
#if MY2K_STOCK_PARITY_PROFILE
static void abort_write(void){
    /* Retire the AUX media lease after a failed/cancelled transaction. An
     * incomplete FAT operation is not rolled back; invalidate cached handles
     * so a later explicit upload can reopen rather than reusing FR_DISK_ERR. */
    if(backend.sync)(void)backend.sync(backend.context);
    my2k_main_fs_reset();
}
#endif
DSTATUS disk_status(BYTE drive) {
    return drive || !current() ? STA_NOINIT : 0;
}
DSTATUS disk_initialize(BYTE drive) {
    return disk_status(drive);
}
DRESULT disk_read(BYTE drive, BYTE *out, LBA_t sector, UINT count) {
    if (drive || !count || sector >= sectors || count > sectors - sector || !backend.read_sector) return RES_PARERR;
    for (UINT i = 0; i < count; i++) {
        if (!sector_budget) return RES_ERROR;
        sector_budget--;
        if (!current() || !backend.read_sector(backend.context, (uint32_t)sector + i, out + 512u * i) || !current()) return RES_ERROR;
    }
    return RES_OK;
}
#if MY2K_STOCK_PARITY_PROFILE
DRESULT disk_write(BYTE drive,const BYTE *data,LBA_t sector,UINT count){
    if(drive || !data || !count || sector>=sectors || count>sectors-sector || !backend.write_sector)return RES_PARERR;
    for(UINT i=0;i<count;i++){
        if(!sector_budget)return RES_ERROR;
        sector_budget--;
        if(!current() || !backend.write_sector(backend.context,(uint32_t)sector+i,data+512u*i) || !current())return RES_ERROR;
    }
    return RES_OK;
}
#endif
DRESULT disk_ioctl(BYTE drive, BYTE command, void *out) {
    if (drive || !current()) return RES_NOTRDY;
    if (command == CTRL_SYNC) {
#if MY2K_STOCK_PARITY_PROFILE
        return backend.sync && backend.sync(backend.context) && current()?RES_OK:RES_ERROR;
#else
        return RES_OK;
#endif
    }
    if (!out) return RES_PARERR;
    if (command == GET_SECTOR_COUNT) {
        *(LBA_t *)out = sectors;
        return RES_OK;
    }
    if (command == GET_SECTOR_SIZE) {
        *(WORD *)out = 512;
        return RES_OK;
    }
    if (command == GET_BLOCK_SIZE) {
        *(DWORD *)out = 1;
        return RES_OK;
    }
    return RES_PARERR;
}
static bool ensure_mount(void) {
    uint32_t n, g;
    if (!backend.media || !backend.read_sector || !backend.media(backend.context, &n, &g) || !n) {
        my2k_main_fs_reset();
        return false;
    }
    if (mounted && n == sectors && g == generation) return true;
    my2k_main_fs_reset();
    sectors    = n;
    generation = g;
    if (f_mount(&fs, "", 1) != FR_OK || !current()) return false;
    mounted = true;
    return true;
}
static bool root_info(uint16_t target, FILINFO *out, uint16_t *count) {
    DIR      dir;
    uint32_t previous_budget = sector_budget;
    sector_budget            = 256; /* Includes entries FatFs internally skips, e.g. volume labels. */
    if (f_opendir(&dir, "/") != FR_OK) {
        sector_budget = previous_budget;
        return false;
    }
    uint16_t n     = 0;
    bool     found = false, ok = true;
    FILINFO  info;
    unsigned visited = 0;
    for (;;) {
        if (++visited > 4096 || !current() || f_readdir(&dir, &info) != FR_OK) {
            ok = false;
            break;
        }
        if (!info.fname[0]) break;
        if (info.fattrib & (AM_DIR | 0x08u)) continue;
        if (n == target) {
            if (out) *out = info;
            found = true;
        }
        if (++n > 255) {
            ok = false;
            break;
        }
    }
    if (f_closedir(&dir) != FR_OK) ok = false;
    if (count) *count = n;
    sector_budget = previous_budget;
    return ok && (out ? found : true);
}
/* Read FAT entries without invoking write-enabled f_getfree(). Two adjacent
 * cache lines permit FAT12 entries straddling a sector boundary. */
static bool free_bytes(uint32_t *free_out, uint32_t *total_out) {
    uint8_t  cache[512];
    uint32_t cached = UINT32_MAX, free_clusters = 0;
    if (fs.fs_type < FS_FAT12 || fs.fs_type > FS_FAT32 || fs.n_fatent < 2) return false;
    uint64_t data_sectors = (uint64_t)(fs.n_fatent - 2) * fs.csize;
    if (!fs.csize || (uint64_t)fs.database + data_sectors > sectors || (uint64_t)fs.fatbase + fs.fsize > sectors) return false;
    uint64_t total = data_sectors * 512;
    if (total > UINT32_MAX) return false; /* GUI fields cannot encode this disk */
    for (uint32_t cluster = 2; cluster < fs.n_fatent; cluster++) {
        if ((cluster & 63u) == 2u && !current()) return false;
        uint32_t offset = fs.fs_type == FS_FAT12 ? cluster + cluster / 2 : cluster * (fs.fs_type == FS_FAT16 ? 2u : 4u);
        unsigned width  = fs.fs_type == FS_FAT32 ? 4 : 2;
        uint32_t value  = 0;
        for (unsigned i = 0; i < width; i++) {
            uint32_t lba = (uint32_t)fs.fatbase + (offset + i) / 512;
            if ((offset + i) / 512 >= fs.fsize) return false;
            if (lba != cached) {
                if (disk_read(0, cache, lba, 1) != RES_OK) return false;
                cached = lba;
            }
            value |= (uint32_t)cache[(offset + i) % 512] << (8 * i);
        }
        if (fs.fs_type == FS_FAT12)
            value = (cluster & 1) ? value >> 4 : value & 0xfff;
        else if (fs.fs_type == FS_FAT32)
            value &= 0x0fffffff;
        if (!value) free_clusters++;
    }
    uint64_t bytes = (uint64_t)free_clusters * fs.csize * 512;
    if (bytes > UINT32_MAX || total > UINT32_MAX || !current()) return false;
    *free_out  = (uint32_t)bytes;
    *total_out = (uint32_t)total;
    return true;
}
static bool filename(const uint8_t *in, char out[13]) {
    unsigned n = 0, dot = 0, base = 0, ext = 0;
    for (; n < 13 && in[n]; n++) {
        unsigned char c = in[n];
        if (c == '.') {
            if (dot || !base) return false;
            dot = 1;
        } else {
            if (c < 33 || c > 126 || strchr("\"*+,/:;<=>?[\\]|", c)) return false;
            if (dot) {
                if (++ext > 3) return false;
            } else if (++base > 8)
                return false;
        }
#if MY2K_STOCK_PARITY_PROFILE
        out[n]=(char)(c>='a' && c<='z'?c-'a'+'A':c);
#else
        out[n] = (char)c;
#endif
    }
    if (!n || n > 12 || in[n] || !base || (dot && !ext)) return false;
    out[n] = 0;
    return true;
}
bool my2k_main_fs_process(uint8_t r[32], size_t size) {
    if (!r || size != 32 || r[0] != 0xfd || r[1] < 35 || r[1] > 41) return false;
    uint8_t request[30];
    memcpy(request, r + 2, 30);
    uint8_t command = r[1];
#if MY2K_DEV7_PROFILE
    /* Defense in depth for callers other than the HID admission queue.
     * Refuse before mounting or invoking any backend operation. */
    if(command==38 || command==41 || (command==37 && request[1]==0)){
        memset(r+2,0,30);r[2]=0x55;return true;
    }
#endif
#if MY2K_STOCK_PARITY_PROFILE
    if(command==41 && delete_replay_valid && !memcmp(delete_request,request,sizeof request)){
        if(current())memcpy(r,delete_response,32);else{my2k_main_fs_reset();memset(r+2,0,30);r[2]=0x55;}
        return true;
    }
    delete_replay_valid=false;
#endif
    memset(r + 2, 0, 30);
    r[2] = 0x55;
#if !MY2K_STOCK_PARITY_PROFILE
    if (command == 38 || command == 41) return true;
#endif
    if (!ensure_mount()) return true;
    if (command == 35) {
        uint16_t count;
        uint32_t free, total;
        if (!root_info(0, NULL, &count) || !free_bytes(&free, &total)) return true;
        r[3] = (uint8_t)count;
        put32(r + 4, free);
        put32(r + 8, total);
    } else if (command == 36) {
        FILINFO info;
        if (!root_info(request[0], &info, NULL)) return true;
        const char *name = info.altname[0] ? info.altname : info.fname;
        size_t      n    = strlen(name);
        if (!n || n > 12) return true;
        memcpy(r + 3, name, n);
        put32(r + 16, (uint32_t)info.fsize);
    } else if (command == 37) {
        if (request[1] != 1
#if MY2K_STOCK_PARITY_PROFILE
            && request[1] != 0
#endif
        ) return true;
#if MY2K_STOCK_PARITY_PROFILE
        bool writing=request[1]==0;
        if(writing && (!backend.write_sector || !backend.sync))return true;
#endif
        char name[13];
        if (!filename(request + 2, name)) return true;
        uint8_t handle = request[0];
#if MY2K_STOCK_PARITY_PROFILE
        /* GUI Download passes the file-list row, not a reserved handle slot.
         * Filename is authoritative; allocate/return a free real handle. */
        handle=255;
#endif
        for (uint8_t i = 0; i < 4; i++) {
            if (used[i] && !strcmp(names[i], name) && (handle == 255 || handle == i)) {
#if MY2K_STOCK_PARITY_PROFILE
                if(writable[i]!=writing)return true;
#endif
                r[3] = i;
                goto finish;
            }
        }
        if (handle == 255) {
            for (handle = 0; handle < 4 && used[handle]; handle++)
                ;
        }
        if (handle >= 4 || used[handle]) return true;
#if MY2K_STOCK_PARITY_PROFILE
        /* A stock/unknown AUX may reject SYNC35. Prove the real flush path
         * before CREATE_ALWAYS can truncate an existing asset. */
        if(writing && (!backend.sync(backend.context) || !current()))return true;
#endif
        if (f_open(&handles[handle], name,
#if MY2K_STOCK_PARITY_PROFILE
                   writing?(FA_READ|FA_WRITE|FA_CREATE_ALWAYS):
#endif
                   FA_READ) != FR_OK) return true;
#if MY2K_STOCK_PARITY_PROFILE
        writable[handle]=writing;
        if(writing && f_sync(&handles[handle])!=FR_OK){abort_write();return true;}
#endif
        used[handle] = true;
        memcpy(names[handle], name, strlen(name) + 1);
        r[3] = handle;
    }
#if MY2K_STOCK_PARITY_PROFILE
    else if(command==38){
        uint8_t h=request[0],n=request[1];uint32_t offset=get32(request+2);
        if(h>=4 || !used[h] || !writable[h] || !n || n>24 || offset>f_size(&handles[h]) || offset>UINT32_MAX-n)return true;
        UINT actual=0;
        if(f_lseek(&handles[h],offset)!=FR_OK || f_write(&handles[h],request+6,n,&actual)!=FR_OK || actual!=n || f_sync(&handles[h])!=FR_OK){abort_write();return true;}
        r[3]=(uint8_t)actual;
    }
#endif
    else if (command == 39) {
        uint8_t  h = request[0], n = request[1];
        uint32_t offset = get32(request + 2);
        if (h >= 4 || !used[h] || n > 24 || offset > f_size(&handles[h])) return true;
        UINT actual = 0;
        if (f_lseek(&handles[h], offset) != FR_OK || f_read(&handles[h], r + 8, n, &actual) != FR_OK) return true;
        r[3] = (uint8_t)actual;
    } else if (command == 40) {
        uint8_t h = request[0];
        if (h >= 4) return true;
        if (!used[h]) {
            goto finish;
        }
        if (f_close(&handles[h]) != FR_OK) {
#if MY2K_STOCK_PARITY_PROFILE
            if(writable[h])abort_write();
#endif
            return true;
        }
        used[h] = false;
#if MY2K_STOCK_PARITY_PROFILE
        writable[h]=false;
    }else if(command==41){
        if(!backend.write_sector || !backend.sync)return true;
        FILINFO info;char name[13];
        if(!root_info(request[0],&info,NULL))return true;
        const char *source=info.altname[0]?info.altname:info.fname;
        size_t length=strlen(source);if(!length || length>12)return true;memcpy(name,source,length+1);
        for(unsigned i=0;i<4;i++)if(used[i]&&!strcmp(names[i],name))return true;
        if(!backend.sync(backend.context) || !current())return true;
        /* Remember even a failed destructive attempt: unlink may already have
         * changed the directory before a later flush fails. Replaying its row
         * must never delete the next file that shifted into that row. A fresh
         * listing/other file command explicitly retires this replay record. */
        memcpy(delete_request,request,sizeof request);
        memcpy(delete_response,r,32);
        delete_replay_valid=true;
        if(f_unlink(name)!=FR_OK || !backend.sync(backend.context))return true;
#endif
    }
finish:
    if (!current()) {
        my2k_main_fs_reset();
        memset(r + 2, 0, 30);
        r[2] = 0x55;
        return true;
    }
    r[2] = 0xaa;
#if MY2K_STOCK_PARITY_PROFILE
    if(command==41){memcpy(delete_request,request,sizeof request);memcpy(delete_response,r,32);delete_replay_valid=true;}
#endif
    return true;
}

bool my2k_main_fs_has_open_writers(void) {
#if MY2K_DEV7_PROFILE && MY2K_STOCK_PARITY_PROFILE
    for(unsigned i=0;i<4;i++)if(used[i]&&writable[i])return true;
#endif
    return false;
}
