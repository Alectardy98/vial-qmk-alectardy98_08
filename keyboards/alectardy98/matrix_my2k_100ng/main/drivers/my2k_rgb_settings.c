/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_rgb_settings.h"
#include <string.h>
#if MY2K_STOCK_PARITY_PROFILE
#include "my2k_stock_effects.h"
#include "my2k_stock_color.h"
static bool valid_mode(unsigned group,uint8_t mode){return my2k_stock_effect_valid(group?1:3,mode);}
static uint8_t migrate_mode(unsigned group,uint8_t mode){return mode==2?(group?4:9):mode==3?(group?6:4):mode==4?(group?17:15):mode;}
#endif
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t crc(const uint8_t *p) {
    uint32_t v = 0xffffffffu;
    for (unsigned i = 0; i < MY2K_RGB_SETTINGS_SIZE; i++) {
        if (i >= 12 && i < 16) continue;
        v ^= p[i];
        for (unsigned bit = 0; bit < 8; bit++)
            v = (v >> 1) ^ ((v & 1) ? 0xedb88320u : 0);
    }
    return ~v;
}
static void encode_config(uint8_t *p, my2k_rgb_config c) {
    p[0] = c.h;
    p[1] = c.s;
    p[2] = c.v;
    p[3] = c.flags;
}
static my2k_rgb_config decode_config(const uint8_t *p) {
    return (my2k_rgb_config){p[0], p[1], p[2], p[3]};
}
bool my2k_rgb_settings_export(const my2k_amk_rgb_runtime *r, uint8_t *p, size_t n) {
    if (!r || !p || n < MY2K_RGB_SETTINGS_SIZE) return false;
    /* Validate before writing output, including externally mutated state. */
    for (unsigned i = 0; i < 8; i++)
#if MY2K_STOCK_PARITY_PROFILE
        if(!valid_mode(i,r->groups[i].mode))return false;
    if(r->debounce_press>10 || r->debounce_release>10 || r->aux_mode>3 || r->aux_mode==2)return false;
#else
        if (r->groups[i].mode >= MY2K_EFFECT_COUNT) return false;
#endif
    memcpy(p, "MYRG", 4);
    p[4] = 1;
    p[5] = 0;
    p[6] = 16;
    p[7] = 0;
    put32(p + 8, MY2K_RGB_SETTINGS_PAYLOAD);
#if MY2K_DEV3_PROFILE
    /* Same592byte allocation: format2 uses formerly fixed header metadata for
     * eight brightness bytes, and spare mode bits for sync/custom. No NVM move. */
    memcpy(p,"MRG2",4);memcpy(p+4,r->brightness,8);
#if MY2K_STOCK_PARITY_PROFILE
    memcpy(p,"MRG3",4);
#endif
#endif
    unsigned off = 16;
    for (unsigned i = 0; i < 8; i++) {
        my2k_rgb_group g = r->groups[i];
        p[off++]         = g.mode;
#if MY2K_DEV3_PROFILE
        p[off-1]|=(r->sync[i]?0x40:0)|(r->use_custom[i]?0x80:0);
        if(i==0 && !r->enabled)p[off-1]|=0x20;
#endif
        p[off++]         = g.speed;
#if MY2K_STOCK_PARITY_PROFILE
        /* Keep full8bit wire speeds. Seven unused strip-mode bits and the
         * grid's unused fifth mode bit hold press/release debounce. Grid has
         * no sync/custom-color feature; those two bits hold the AUX mode. */
        uint8_t debounce=r->debounce_press|(r->debounce_release<<4);
        uint8_t *mode=&p[off-2];
        if(i==0){*mode=(*mode&0x2f)|((debounce&1u)<<4)|(r->aux_mode<<6);}
        else *mode|=((debounce>>i)&1u)<<5;
#endif
        p[off++]         = g.color.r;
        p[off++]         = g.color.g;
        p[off++]         = g.color.b;
    }
    for (unsigned i = 0; i < 128; i++, off += 4)
        encode_config(p + off, r->rgb.config[i]);
    for (unsigned i = 0; i < 6; i++, off += 4)
        encode_config(p + off, r->indicators[i]);
    put32(p + 12, crc(p));
    return true;
}
bool my2k_rgb_settings_import(my2k_amk_rgb_runtime *r, const uint8_t *p, size_t n) {
    if (!r || !p || n != MY2K_RGB_SETTINGS_SIZE) return false;
    bool version3=!memcmp(p,"MRG3",4);
#if !MY2K_STOCK_PARITY_PROFILE
    if(version3)return false;
#endif
    bool version2=version3 || !memcmp(p,"MRG2",4);
    if ((!version2 && (memcmp(p, "MYRG", 4) || p[4] != 1 || p[5] || p[6] != 16 || p[7] || get32(p + 8) != MY2K_RGB_SETTINGS_PAYLOAD)) || get32(p + 12) != crc(p)) return false;
#if !MY2K_DEV3_PROFILE
    if(version2)return false;
#endif
    for (unsigned i = 0; i < 8; i++)
#if MY2K_STOCK_PARITY_PROFILE
        if(version3?!valid_mode(i,p[16+i*5]&(i?0x1f:0x0f)):(p[16+i*5]&(version2?0x1f:0xff))>=MY2K_EFFECT_COUNT)return false;
    uint8_t debounce=(p[16]>>4)&1u;
    if(version3){for(unsigned i=1;i<8;i++)debounce|=((p[16+i*5]>>5)&1u)<<i;
        if((debounce&15)>10 || (debounce>>4)>10 || (p[16]>>6)==2)return false;
    }
#else
        if ((p[16 + i * 5] & (version2?0x1f:0xff)) >= MY2K_EFFECT_COUNT) return false;
#endif
    /* No remaining fallible operation: validation completes before first mutation. */
    unsigned off = 16;
    r->enabled=!version2 || !(p[16]&0x20);
    for (unsigned i = 0; i < 8; i++) {
        r->brightness[i]=version2?p[4+i]:255;
        r->sync[i]=version2 && (p[off]&0x40);r->use_custom[i]=version2 && (p[off]&0x80);
        r->groups[i] = (my2k_rgb_group){.mode = p[off] & (version2?0x1f:0xff), .speed = p[off + 1], .color = {p[off + 2], p[off + 3], p[off + 4]}};
#if MY2K_STOCK_PARITY_PROFILE
        if(version3 && i==0){r->groups[i].mode&=15;r->sync[i]=r->use_custom[i]=0;}
        if(!version3){r->groups[i].mode=migrate_mode(i,r->groups[i].mode);r->groups[i].color=my2k_stock_rgb_to_hsv(r->groups[i].color);}
#endif
        off += 5;
    }
    for (unsigned i = 0; i < 128; i++, off += 4)
        r->rgb.config[i] = decode_config(p + off);
    for (unsigned i = 0; i < 6; i++, off += 4)
        r->indicators[i] = decode_config(p + off);
#if MY2K_STOCK_PARITY_PROFILE
    r->debounce_press=version3?(debounce&15):5;r->debounce_release=version3?(debounce>>4):5;r->aux_mode=version3?p[16]>>6:0;
#endif
    r->configuration_revision++;
    r->dirty = true;
    return true;
}
