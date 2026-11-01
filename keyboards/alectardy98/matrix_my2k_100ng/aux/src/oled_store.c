/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_oled_store.h"
#include "aux_animation_files.h"
#if !MY2K_ANIMATION_PROFILE
#    define aux_animation_files_busy() false
#endif
#include "aux_stock_parity.h"
#include "my2k_oled_config.h"
#include "ff.h"
#include "aux_msc.h"
#include "aux_assets.h"
#include <string.h>
/* Two independently CRC-bearing records in separate logical sectors. Existing
 * Only the requested BW_TEXT.ABW idle image is additionally replaced.
 * FAT is not journalled: sync/CRC and
 * alternation do not guarantee filesystem survival through physical power loss. */
#define PATH "0:/R1OLED.CFG"
static bool     writing, valid;
uint32_t        aux_oled_store_error, aux_oled_store_saves;
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (i * 8));
}
bool aux_oled_store_writing(void) {
    return writing;
}
bool aux_oled_store_valid(void) {
    return valid;
}
static bool read_record(FIL *f, unsigned slot, uint8_t *config, uint32_t *seq, bool *empty) {
    uint8_t sector[512];
    UINT    n = 0;
    if (f_lseek(f, slot * 512u) != FR_OK || f_read(f, sector, 512, &n) != FR_OK || n != 512) {
        aux_oled_store_error = 2;
        return false;
    }
    *empty = true;
    for (unsigned i = 0; i < 512; i++)
        if (sector[i] != 255)
            *empty = false;
    if (memcmp(sector, "R1OD", 4) || memcmp(sector + 492, "CMIT", 4) ||
        get32(sector + 488) != my2k_oled_crc(sector, 488) || !my2k_oled_valid(sector + 8))
        return false;
    memcpy(config, sector + 8, MY2K_OLED_BYTES);
    *seq = get32(sector + 4);
    return true;
}
static bool newest(FIL *f, uint8_t *config, uint32_t *seq, unsigned *slot, bool *blank) {
    uint8_t  a[MY2K_OLED_BYTES], b[MY2K_OLED_BYTES];
    uint32_t x = 0, y = 0;
    bool     ea = false, eb = false;
    bool     va = read_record(f, 0, a, &x, &ea), vb = read_record(f, 1, b, &y, &eb);
    *blank = ea && eb;
    if (!va && !vb)
        return false;
    *slot = vb && (!va || (int32_t)(y - x) > 0);
    *seq  = *slot ? y : x;
    memcpy(config, *slot ? b : a, MY2K_OLED_BYTES);
    return true;
}
bool aux_oled_store_load(uint8_t config[MY2K_OLED_BYTES]) {
    FIL     f;
    FRESULT r            = f_open(&f, PATH, FA_READ);
    valid                = false;
    aux_oled_store_error = 0;
    if (r == FR_NO_FILE)
        return false;
    if (r != FR_OK) {
        aux_oled_store_error = 1;
        return false;
    }
    uint32_t seq;
    unsigned slot;
    bool     blank;
    bool     ok = f_size(&f) == 1024 && newest(&f, config, &seq, &slot, &blank);
    if (f_close(&f) != FR_OK)
        ok = false;
    if (!ok)
        aux_oled_store_error = 2;
    valid = ok;
    return ok;
}
static bool save_abw(const uint8_t *config) {
    if (!(config[1] & MY2K_OLED_BITMAP))
        return true;
    if (!aux_assets_release_mono())
        return false;
    uint8_t image[382] = {0};
    memcpy(image, "ABIT", 4);
    image[4]  = 20;
    image[6]  = 22;
    image[8]  = 126;
    image[9]  = 1;
    image[12] = 72;
    image[14] = 40;
    image[16] = 2;
    image[18] = 1;
    for (unsigned y = 0; y < 40; y++)
        for (unsigned x = 0; x < 72; x++)
            if (config[116 + y / 8 * 72 + x] & (1u << (y % 8)))
                image[22 + (y * 72 + x) / 8] |= 1u << ((y * 72 + x) % 8);
    FIL  f;
    UINT n = 0;
    if (f_open(&f, "0:/BW_TEXT.ABW", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
        return false;
    bool ok = f_write(&f, image, sizeof image, &n) == FR_OK && n == sizeof image && f_sync(&f) == FR_OK;
    if (f_close(&f) != FR_OK)
        ok = false;
    if (!ok)
        return false;
    if (f_open(&f, "0:/BW_TEXT.ABW", FA_READ) != FR_OK)
        return false;
    uint8_t check[382];
    n  = 0;
    ok = f_size(&f) == sizeof check && f_read(&f, check, sizeof check, &n) == FR_OK && n == sizeof check &&
         !memcmp(check, image, sizeof check);
    if (f_close(&f) != FR_OK)
        ok = false;
    return ok;
}
bool aux_oled_store_save(const uint8_t config[MY2K_OLED_BYTES]) {
    if (!my2k_oled_valid(config) || writing || aux_animation_files_busy() || aux_msc_media_writing())
        return false;
    writing              = true;
    aux_oled_store_error = 0;
    FIL      f;
    bool     opened = false, ok = false;
    uint8_t  old[MY2K_OLED_BYTES], sector[512];
    uint32_t seq   = 0;
    unsigned slot  = 1;
    bool     blank = false;
    FRESULT  r     = f_open(&f, PATH, FA_READ | FA_WRITE);
    if (r == FR_NO_FILE) {
        r = f_open(&f, PATH, FA_CREATE_NEW | FA_WRITE | FA_READ);
        if (r == FR_OK) {
            opened = true;
            memset(sector, 255, 512);
            UINT n = 0;
            if (f_write(&f, sector, 512, &n) != FR_OK || n != 512 || f_write(&f, sector, 512, &n) != FR_OK ||
                n != 512 || f_sync(&f) != FR_OK)
                goto finish;
        }
    }
    if (r != FR_OK)
        goto finish;
    opened = true;
    if (f_size(&f) != 1024)
        goto finish;
    bool found = newest(&f, old, &seq, &slot, &blank);
    if (!found && !blank)
        goto finish;
    if ((!found || memcmp(old + 116, config + 116, 360) || memcmp(old + 4, config + 4, 60)) && !save_abw(config))
        goto finish;
    if (found && !memcmp(old, config, MY2K_OLED_BYTES)) {
        ok = true;
        goto finish;
    }
    slot = found ? slot ^ 1u : 0;
    seq  = found ? seq + 1u : 0;
    memset(sector, 255, 512);
    memcpy(sector, "R1OD", 4);
    put32(sector + 4, seq);
    memcpy(sector + 8, config, MY2K_OLED_BYTES);
    put32(sector + 488, my2k_oled_crc(sector, 488));
    memcpy(sector + 492, "CMIT", 4);
    UINT n = 0;
    if (f_lseek(&f, slot * 512u) != FR_OK || f_write(&f, sector, 512, &n) != FR_OK || n != 512 || f_sync(&f) != FR_OK)
        goto finish;
    /* Retire FIL cache before readback. */
    if (f_close(&f) != FR_OK) {
        opened = false;
        goto finish;
    }
    opened = false;
    if (f_open(&f, PATH, FA_READ) != FR_OK)
        goto finish;
    opened = true;
    uint32_t actual;
    bool     empty;
    if (!read_record(&f, slot, old, &actual, &empty) || actual != seq || memcmp(old, config, MY2K_OLED_BYTES))
        goto finish;
    ok = true;
    aux_oled_store_saves++;
finish:
    if (opened && f_close(&f) != FR_OK)
        ok = false;
    if (!ok)
        aux_oled_store_error = 3;
    else
        valid = true;
    writing = false;
    return ok;
}
