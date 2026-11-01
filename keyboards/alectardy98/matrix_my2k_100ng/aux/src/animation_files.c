/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_animation_files.h"
#include "my2k_rle565.h"
#include "aux_assets.h"
#include "aux_platform.h"
#include "aux_msc.h"
#include "aux_oled_store.h"
#include "ff.h"
#include "aux_nes.h"
#include "aux_nes_library.h"
#include "my2k_asset.h"
#include <string.h>
/* AFS1: bounded, slot-only file service. No arbitrary path or raw-sector write.
 * FAT is not journalled. A synced backup is retained across the rename window;
 * interrupted TMP/BAK files require explicit maintenance, never boot-time writes.
 * Only this worker owns FIL objects. USB callbacks never call FatFs. */
static uint8_t  command[32], reply[32], last[32];
static bool     pending, completed, uploading, reading, close_fault, writing;
static uint8_t *fast_buffer;
static uint16_t fast_size;
static bool fast_cancelled;
static FIL      file;
static uint8_t  upload_slot;
static uint32_t expected, position, checksum, expected_crc, last_write;
/* Two bounded queue entries borrow the existing MSC completion/staging
 * buffers. Only the unique filesystem worker changes FIL or durable offset. */
static struct {uint32_t at;uint16_t size;uint8_t token;} bulk_queue[2];
static uint8_t *bulk_buffers[2];
static uint8_t bulk_head,bulk_tail,bulk_count,bulk_error;
static bool bulk_mode;
static uint32_t bulk_end;
static char     read_path[100];
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(n >> (8 * i));
}
/* Reflected IEEE CRC32, two four-bit table steps per byte. The64-byte
 * table is immutable flash, not RAM; initial/final inversion is unchanged. */
static uint32_t crc(uint32_t c, const uint8_t *p, unsigned n) {
    static const uint32_t nibble[16]={0x00000000u,0x1db71064u,0x3b6e20c8u,0x26d930acu,0x76dc4190u,0x6b6b51f4u,0x4db26158u,0x5005713cu,0xedb88320u,0xf00f9344u,0xd6d6a3e8u,0xcb61b38cu,0x9b64c2b0u,0x86d3d2d4u,0xa00ae278u,0xbdbdf21cu};
    while(n--){c^=*p++;c=(c>>4)^nibble[c&15u];c=(c>>4)^nibble[c&15u];}
    return c;
}
static void path(unsigned slot, char p[64], char kind) {
    strcpy(p, "0:/anamations/animation_00.CRS");
    p[24] = (char)('0' + slot / 10);
    p[25] = (char)('0' + slot % 10);
    if (kind) {
        strcpy(p, kind == 'B' ? "0:/anamations/BK00.BAK" : "0:/anamations/UP00.TMP");
        p[16] = (char)('0' + slot / 10);
        p[17] = (char)('0' + slot % 10);
    }
}
static bool exists(const char *p) {
    FILINFO info;
    return f_stat(p, &info) == FR_OK;
}
bool aux_animation_slot_path(unsigned slot, char p[64]) {
    if (slot < 1 || slot > 10) return false;
    path(slot, p, 0);
    if (exists(p)) return true;
    strcpy(p, "0:/animation_00.CRS");
    p[13] = (char)('0' + slot / 10);
    p[14] = (char)('0' + slot % 10);
    return exists(p);
}
static void close_read(void) {
    if (reading) {
        if (f_close(&file) != FR_OK) close_fault = true;
        reading = false;
    }
}
bool aux_animation_files_export_ready(void) {
    if (uploading || pending || close_fault || fast_buffer || !aux_nes_library_ready()) return false;
    close_read();
    return !close_fault;
}
static bool reader(void *ctx, uint32_t at, void *out, size_t n) {
    UINT got = 0;
    return f_lseek(ctx, at) == FR_OK && f_read(ctx, out, n, &got) == FR_OK && got == n;
}
static bool valid(FIL *f) {
    my2k_reader r = {f, (uint32_t)f_size(f), reader};
    my2k_asset  a;
    return f_size(f) <= 16778260u && my2k_asset_open(&r, &a) && a.kind == MY2K_ASSET_ANIM && a.width == 128 && a.height == 128;
}
/* Prefer a valid current root legacy; if a slot is playing, require exactly
 * one valid non-slot CRS at the root. Never choose an ambiguous user asset. */
static bool legacy_candidate(const char *name) {
    size_t n = strlen(name);
    if (n < 5 || n > 96 || strchr(name, '/') || strchr(name, ':')) return false;
    const char *ext = name + n - 4;
    if (ext[0] != '.' || (ext[1] != 'C' && ext[1] != 'c') || (ext[2] != 'R' && ext[2] != 'r') || (ext[3] != 'S' && ext[3] != 's')) return false;
    if (n == 16 && !memcmp(name, "animation_", 10) && name[10] >= '0' && name[10] <= '9' && name[11] >= '0' && name[11] <= '9') return false;
    FIL  probe;
    char p[100] = "0:/";
    strcat(p, name);
    if (f_open(&probe, p, FA_READ) != FR_OK) return false;
    bool ok = valid(&probe);
    if (f_close(&probe) != FR_OK) {
        close_fault = true;
        return false;
    }
    return ok;
}
static bool legacy_path(char out[100]) {
    const char *current = aux_assets_selected_color();
    if (current && legacy_candidate(current)) {
        strcpy(out, "0:/");
        strcat(out, current);
        return true;
    }
    if (close_fault) return false;
    DIR      dir;
    FILINFO  info;
    unsigned found    = 0;
    bool     complete = false;
    if (f_opendir(&dir, "0:/") != FR_OK) return false;
    for (unsigned i = 0; i < 256; i++) {
        if (f_readdir(&dir, &info) != FR_OK) break;
        if (!info.fname[0]) {
            complete = true;
            break;
        }
        if (!(info.fattrib & AM_DIR) && legacy_candidate(info.fname)) {
            if (++found > 1) break;
            strcpy(out, "0:/");
            strcat(out, info.fname);
        }
        if (close_fault) break;
    }
    bool closed = f_closedir(&dir) == FR_OK;
    return closed && complete && found == 1 && !close_fault;
}
static bool directory(void) {
    FRESULT r = f_mkdir("0:/anamations");
    if (r == FR_OK) return true;
    FILINFO i;
    return r == FR_EXIST && f_stat("0:/anamations", &i) == FR_OK && (i.fattrib & AM_DIR);
}
static bool resolve(unsigned slot, char p[100]) {
    if (slot) return aux_animation_slot_path(slot, p);
    const char *s = aux_assets_selected_color();
    if (!s || !*s || strlen(s) > 96) return false;
    strcpy(p, "0:/");
    strcat(p, s);
    return true;
}
bool aux_animation_files_busy(void) {
    return uploading || pending || close_fault || fast_buffer || !aux_nes_library_ready();
}
bool aux_animation_files_writing(void) { return writing; }
bool aux_animation_files_submit(const uint8_t p[32]) {
    if (fast_buffer) return false;
    if (pending) return !memcmp(command, p, 32);
    if (completed && p[2] >= 3 && !memcmp(last, p, 32)) return true; /* Exact immediate retransmit is idempotent. */
    memcpy(command, p, 32);
    memset(reply, 0, 32);
    reply[0]  = 253;
    reply[1]  = 238;
    reply[2]  = 170;
    reply[3]  = 1;
    reply[4]  = p[2];
    reply[5]  = p[4];
    reply[6]  = 1;
    pending   = true;
    completed = false;
    return true;
}
void aux_animation_files_reply(uint8_t p[32]) {
    memcpy(p, reply, 32);
}
uint8_t *aux_animation_fast_receive(uint16_t size) {
    if (size < 32 || size > 4096 || fast_buffer || pending || aux_nes_active() || aux_oled_store_writing()) return NULL;
    fast_buffer = aux_msc_control_acquire();
    if (fast_buffer) { fast_size = size; fast_cancelled = false; }
    return fast_buffer;
}
bool aux_animation_fast_submit(void) {
    if (!fast_buffer || fast_size < 32) return false;
    memcpy(command, fast_buffer, 32);
    memset(reply, 0, 32);
    reply[0]=253;reply[1]=238;reply[2]=170;reply[3]=2;
    reply[4]=command[2];reply[5]=command[4];reply[6]=1;
    pending=true;completed=false;
    return true;
}
uint8_t *aux_animation_fast_reply(uint16_t *size) {
    *size=32;
    if (!fast_buffer || pending) return reply;
    *size=(uint16_t)(32u + ((unsigned)reply[30] | (unsigned)reply[31]<<8));
    memcpy(fast_buffer,reply,32);
    return fast_buffer;
}
int32_t aux_animation_bulk_read(const uint8_t cdb[16],void *buffer,uint16_t size){
 unsigned wanted=(unsigned)cdb[12]|(unsigned)cdb[13]<<8;uint16_t n;
 if(cdb[0]!=0xc2||memcmp(cdb+1,"AFR",3)||cdb[4]!=1||!cdb[6]||cdb[7]||cdb[14]||cdb[15]||!buffer||!fast_buffer||pending||!wanted||wanted>4064||size!=((32u+wanted+511u)&~511u)||command[2]!=2||command[4]!=cdb[6]||command[5]!=cdb[5]||get32(command+8)!=get32(cdb+8))return -1;
 uint8_t *p=aux_animation_fast_reply(&n);
 if(p[6]!=2||n!=32u+wanted)return -1;
 memcpy(buffer,p,n);memset((uint8_t*)buffer+n,0,size-n);return size;
}
int32_t aux_animation_direct_read(const uint8_t cdb[16],void *buffer,uint16_t size){
 unsigned n=(unsigned)cdb[12]|(unsigned)cdb[13]<<8;uint32_t at=get32(cdb+8);
 if(cdb[0]!=0xc3||memcmp(cdb+1,"AFD",3)||cdb[4]!=1||cdb[5]>10||!cdb[6]||cdb[7]||cdb[14]||cdb[15]||!buffer||!n||n>4096||size!=((n+511u)&~511u)||
    pending||uploading||fast_buffer||close_fault||aux_nes_active()||aux_msc_export_active()||aux_msc_media_writing()||aux_oled_store_writing()||!aux_assets_media_service())return -1;
 char target[100];if(!resolve(cdb[5],target))return -1;
 if(!reading||strcmp(read_path,target)){
  close_read();if(close_fault||f_open(&file,target,FA_READ)!=FR_OK)return -1;
  reading=true;strcpy(read_path,target);
 }
 UINT got=0;
 if(at>f_size(&file)||n>f_size(&file)-at||(f_tell(&file)!=at&&f_lseek(&file,at)!=FR_OK)||f_read(&file,buffer,n,&got)!=FR_OK||got!=n){close_read();return -1;}
 memset((uint8_t *)buffer+n,0,size-n);return size;
}
void aux_animation_fast_ack(void) {
    if (fast_buffer && !pending) { fast_buffer=NULL;aux_msc_control_release(); }
}
void aux_animation_fast_cancel(void) {
    fast_cancelled=true;
    if (!pending) aux_animation_fast_ack();
}
static void bulk_release(void){
    if(bulk_mode){bulk_mode=false;bulk_count=bulk_head=bulk_tail=0;bulk_buffers[0]=bulk_buffers[1]=NULL;aux_msc_control_release();}
}
static uint8_t append(const uint8_t *data,unsigned n,unsigned slot,uint32_t at){
    if(!uploading||slot!=upload_slot||!n||at!=position||n>expected-position)return 3;
    UINT wrote=0;
    if(f_write(&file,data,n,&wrote)!=FR_OK||wrote!=n)return 5;
    last_write=aux_millis();checksum=crc(checksum,data,n);position+=n;return 2;
}
bool aux_animation_bulk_submit(const uint8_t cdb[16],const void *data,uint16_t size){
    unsigned n=(unsigned)cdb[12]|(unsigned)cdb[13]<<8;
    unsigned encoded=cdb[7]?((unsigned)cdb[14]|(unsigned)cdb[15]<<8):n;
    uint32_t at=get32(cdb+8);
    if(cdb[0]!=0xc1||memcmp(cdb+1,"AFB",3)||cdb[4]!=1||!cdb[6]||cdb[7]>1||(!cdb[7]&&(cdb[14]||cdb[15]))||
       !data||!uploading||!bulk_mode||bulk_error||pending||bulk_count==2||
       cdb[5]!=upload_slot||!n||n>4096||!encoded||encoded>n||size!=((encoded+511u)&~511u)||at!=bulk_end||n>expected-bulk_end)return false;
    const uint8_t *source=data;
    for(unsigned i=encoded;i<size;i++)if(source[i])return false;
    unsigned tail=bulk_tail;
    if(cdb[7]){if(!my2k_rle565_decode(source,encoded,bulk_buffers[tail],n))return false;}
    else memcpy(bulk_buffers[tail],data,n);
    bulk_queue[tail].at=at;bulk_queue[tail].size=(uint16_t)n;bulk_queue[tail].token=cdb[6];
    bulk_tail^=1u;bulk_count++;bulk_end+=n;last_write=aux_millis();return true;
}
void aux_animation_bulk_status(uint8_t out[32]){
    memset(out,0,32);memcpy(out,"BSQ1",4);out[4]=bulk_mode;out[5]=bulk_error;out[6]=bulk_count;out[7]=bulk_mode?2u-bulk_count:0;
    put32(out+8,position);put32(out+12,bulk_end);out[16]=upload_slot;out[17]=uploading;
}
bool aux_animation_files_pending(void){return pending||bulk_count;}
void aux_animation_files_service(void) {
    /* Abandoned uploads release open handles, retaining the partial TMP for
     * explicit maintenance. Never erase/rename/retry a partial file on boot. */
    if (!pending && !bulk_count && uploading && (uint32_t)(aux_millis() - last_write) > 60000u && aux_msc_io_idle()) {
        writing = true;
        if (f_close(&file) != FR_OK) close_fault = true;
        uploading = false;
        bulk_release();
        writing = false;
    }
    if(bulk_count&&aux_msc_io_idle()){
        unsigned head=bulk_head;writing=true;
        uint8_t status=append(bulk_buffers[head],bulk_queue[head].size,upload_slot,bulk_queue[head].at);
        writing=false;
        if(status!=2){bulk_error=status;bulk_count=0;bulk_head=bulk_tail=0;}
        else{bulk_head^=1u;bulk_count--;}
        return;
    }
    if (!pending || !aux_msc_io_idle()) return;
    uint32_t service_started=aux_millis();
    uint8_t result[32];
    memcpy(result, reply, 32);
    uint8_t    *q = command, *r = result;
    const bool fast = fast_buffer != NULL;
    unsigned    op = q[2], slot = q[5], n = fast ? ((unsigned)q[30] | (unsigned)q[31]<<8) : q[7];
    uint8_t *payload = fast ? fast_buffer+32 : q+12;
    uint8_t *output = fast ? fast_buffer+32 : r+12;
    uint32_t    at = get32(q + 8);
    char        target[100], temporary[64], backup[64];
    char        previous[100] = {0};
    bool        released      = false;
    bool library=op==11||op==12;
#if MY2K_GBC_PROFILE
    library=library||op==13||op==14;
#endif
    const char *current       = aux_assets_selected_color();
    if (current && strlen(current) < sizeof previous) strcpy(previous, current);
    r[6] = 2; /* completed; 1 pending, >=3 error */
    if (q[0] != 253 || q[1] != 238 || q[3] != (fast?2:1) || !q[4] || slot > 10 || n > (fast?4064u:20u) || (op > 8 && (!fast || !library)) || (fast && (fast_size != 32u+((op==4||op==12||op==14)?n:0u)))) {
        r[6] = 3;
        goto done;
    }
    if(library){
        if(!fast||slot||((op==12||op==14)&&at)||q[6]||uploading||aux_msc_export_active()||aux_msc_media_writing()||aux_oled_store_writing()||!aux_assets_media_service()){r[6]=4;goto done;}
        close_read();if(close_fault){r[6]=5;goto done;}
        unsigned produced=0;writing=op==12||op==14;
        #if MY2K_GBC_PROFILE
        if(op>=13)r[6]=aux_gbc_library_request(op,at,payload,n,output,&produced);else
#endif
        r[6]=aux_nes_library_request(op,at,payload,n,output,&produced);
        r[30]=(uint8_t)produced;r[31]=(uint8_t)(produced>>8);goto done;
    }
    if ((op != 3 && q[6]) || (op == 3 && q[6] > 1)) {
        /* Legacy import alone also permits an explicit replacement flag. */
        if (op != 8 || q[6] > 1) {
            r[6] = 3;
            goto done;
        }
    }
    if ((op != 2 && op != 3 && op != 4 && (n || at)) || (op == 3 && n != 4) || (op == 2 && !n) || (op == 4 && !n)) {
        r[6] = 3;
        goto done;
    }
    if (!op) {
        if (slot) {
            r[6] = 3;
            goto done;
        }
        memcpy(r + 12, fast?"AFB1":"AFS1", 4);
        if(!fast)memcpy(r+8,"AFB2",4);
        r[7]  = 8;
        r[16] = 10;
        r[17] = 20;
        r[18] = aux_msc_export_active();
        r[19] = (uploading ? 1 : 0) | (close_fault ? 2 : 0);
#ifdef AUX_DEV_BUILD_TAG
        r[7]=16;memcpy(r+20,"AD01",4);put32(r+24,AUX_DEV_BUILD_TAG);
#endif
#if AUX_SD_READ_TRACE
        extern volatile uint32_t aux_sd_read_trace[14],aux_sd_write_trace[12];
        r[7]=20;r[28]=(uint8_t)aux_sd_read_trace[1];r[29]=(uint8_t)aux_sd_write_trace[3];
        r[30]=aux_assets_frame_rate();r[31]=1; /* Snapshot schema, not a reset/recovery action. */
#endif
        if (fast) { r[7]=20;r[17]=0; r[20]=0xe0;r[21]=0x0f;r[22]=61;
#if MY2K_DIRECT_FILE_READ
        r[22]|=128;
#endif
#if MY2K_GBC_PROFILE
        r[22]|=64;
#endif
        r[23]=2; } /*4064 payload, CRC before publish*/
        goto done;
    }
    if (close_fault) {
        r[6] = 5;
        goto done;
    }
    if (aux_msc_export_active() || aux_msc_media_writing() || aux_oled_store_writing() || !aux_assets_media_service()) {
        r[6] = 4;
        goto done;
    }
    writing = op >= 3 && op <= 8;
    if (op == 6) { /* Explicit abort removes only this service's temporary file. */
        if (!slot || !uploading || slot != upload_slot) {
            r[6] = 3;
            goto done;
        }
        bool ok = f_close(&file) == FR_OK;
        if (!ok) close_fault = true;
        uploading = false;
        bulk_release();
        path(slot, temporary, 'T');
        if (ok) ok = f_unlink(temporary) == FR_OK;
        if (!ok) r[6] = 5;
        goto done;
    }
    if (uploading && op != 4 && op != 5) {
        r[6] = 4;
        goto done;
    }
    if (op == 1 || op == 2) {
        if (!resolve(slot, target)) {
            r[6] = 6;
            goto done;
        }
        if (op == 1) {
            FILINFO i;
            if (f_stat(target, &i) != FR_OK || (i.fattrib & AM_DIR)) {
                r[6] = 6;
                goto done;
            }
            put32(r + 8, i.fsize);
            goto done;
        }
        if (!reading || strcmp(read_path, target)) {
            close_read();
            if (f_open(&file, target, FA_READ) != FR_OK) {
                r[6] = 5;
                goto done;
            }
            reading = true;
            strcpy(read_path, target);
        }
        UINT got = 0;
        if (!n || at > f_size(&file) || n > f_size(&file) - at || f_lseek(&file, at) != FR_OK || f_read(&file, output, n, &got) != FR_OK || got != n) {
            r[6] = 5;
            close_read();
            goto done;
        }
        if(fast){r[30]=(uint8_t)got;r[31]=(uint8_t)(got>>8);}else r[7] = got;
        put32(r + 8, at);
        goto done;
    }
    close_read();
    if (close_fault) {
        r[6] = 5;
        goto done;
    }
    if (!slot) {
        r[6] = 3;
        goto done;
    }
    path(slot, target, 0);
    path(slot, temporary, 'T');
    path(slot, backup, 'B');
    if (op == 3) {
        uint32_t size = at;
        if (size < 32790u || size > 16778260u || n != 4 || q[6] > 1) {
            r[6] = 3;
            goto done;
        }
        if (!directory()) {
            r[6] = 5;
            goto done;
        }
        if (exists(temporary) || exists(backup)) {
            r[6] = 9;
            goto done;
        }
        if (exists(target) && !q[6]) {
            r[6] = 7;
            goto done;
        }
        if (f_open(&file, temporary, FA_WRITE | FA_CREATE_NEW | FA_READ) != FR_OK) {
            r[6] = 5;
            goto done;
        }
        DWORD  free_clusters;
        FATFS *fs;
        if (f_getfree("0:", &free_clusters, &fs) != FR_OK || (uint64_t)free_clusters * fs->csize * 512u < (uint64_t)size + 65536u) {
            if (f_close(&file) == FR_OK)
                (void)f_unlink(temporary);
            else
                close_fault = true;
            r[6] = 11;
            goto done;
        }
        if(q[27]==0xb2){
            bulk_buffers[0]=aux_msc_control_acquire();bulk_buffers[1]=aux_msc_control_second();
            if(!bulk_buffers[0]||!bulk_buffers[1]){
                if(bulk_buffers[0])aux_msc_control_release();
                if(f_close(&file)==FR_OK)(void)f_unlink(temporary);else close_fault=true;
                r[6]=4;goto done;
            }
            bulk_mode=true;bulk_count=bulk_head=bulk_tail=bulk_error=0;bulk_end=0;
        }
        last_write   = aux_millis();
        uploading    = true;
        upload_slot  = slot;
        expected     = size;
        expected_crc = get32(q + 12);
        position     = 0;
        checksum     = ~0u;
        goto done;
    }
    if (op == 4) {
        r[6]=bulk_mode?4:append(payload,n,slot,at);
        if(r[6]==2)put32(r+8,position);
        goto done;
    }
    if (op == 5) {
        if(bulk_error){r[6]=bulk_error;goto done;}
        if (!uploading || slot != upload_slot || position != expected || (checksum ^ ~0u) != expected_crc) {
            r[6] = 8;
            goto done;
        }
        if (f_sync(&file) != FR_OK || !valid(&file)) {
            r[6] = 8;
            goto done;
        }
        if(fast){
            uint32_t stored=~0u, remaining=expected;
            if(f_lseek(&file,0)!=FR_OK){r[6]=5;goto done;}
            while(remaining){
                UINT got=0, amount=remaining>4064?4064:remaining;
                if(f_read(&file,output,amount,&got)!=FR_OK||got!=amount){r[6]=5;goto done;}
                stored=crc(stored,output,got);remaining-=got;
            }
            stored^=~0u;
            if(stored!=expected_crc){r[6]=8;goto done;}
            put32(r+16,stored);
        }
        if(bulk_mode){put32(r+16,expected_crc);r[7]=2;} /* Received CRC + sync, not full reread. */
        bool ok = f_close(&file) == FR_OK;
        if (!ok) close_fault = true;
        uploading = false;
        bulk_release();
        if (!ok) {
            r[6] = 5;
            goto done;
        }
        if (!aux_assets_release_color()) {
            r[6] = 5;
            goto done;
        }
        released = true;
        bool had = exists(target);
        if (had && f_rename(target, backup) != FR_OK) {
            r[6] = 5;
            goto done;
        }
        if (f_rename(temporary, target) != FR_OK) {
            if (had) (void)f_rename(backup, target);
            r[6] = 5;
            goto done;
        }
        if (had && f_unlink(backup) != FR_OK) {
            r[6] = 9;
            goto done;
        }
        (void)aux_assets_select_color(previous[0] ? previous : target + 3);
        released = false;
        put32(r + 8, position);
        goto done;
    }
    if (op == 7) {
        if (!aux_animation_slot_path(slot, target)) {
            r[6] = 6;
            goto done;
        }
        if (!aux_assets_release_color() || f_unlink(target) != FR_OK) {
            r[6] = 5;
            goto done;
        }
        (void)aux_assets_init();
        goto done;
    }
    if (op == 8) { /* User explicitly imports the current legacy root animation. */
        char source[100];
        if (slot != 1 || !legacy_path(source)) {
            r[6] = close_fault ? 5 : 3;
            goto done;
        }
        if (!directory() || exists(backup) || exists(temporary)) {
            r[6] = 9;
            goto done;
        }
        if (exists(target) && !q[6]) {
            r[6] = 7;
            goto done;
        }
        if (!aux_assets_release_color()) {
            r[6] = 5;
            goto done;
        }
        released = true;
        bool had = exists(target);
        if (had && f_rename(target, backup) != FR_OK) {
            r[6] = 5;
            goto done;
        }
        if (f_rename(source, target) != FR_OK) {
            if (had) (void)f_rename(backup, target);
            r[6] = 5;
            goto done;
        }
        if (had && f_unlink(backup) != FR_OK) {
            r[6] = 9;
            goto done;
        }
        (void)aux_assets_select_color(target + 3);
        goto done;
    }
    r[6] = 3;
done:
    if(fast&&op)put32(result+20,aux_millis()-service_started);
    writing = false;
    if (released && previous[0]) (void)aux_assets_select_color(previous);
    memcpy(reply, result, 32);
    memcpy(last, command, 32);
    completed = true;
    pending   = false;
    if(fast_cancelled)aux_animation_fast_ack();
}
