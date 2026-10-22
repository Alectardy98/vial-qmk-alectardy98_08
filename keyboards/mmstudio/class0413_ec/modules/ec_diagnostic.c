// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic adapter only. Sensing, scaling and calibration remain Cipulot's.
#include "ec_diagnostic.h"
#include "ec_switch_matrix.h"
#include "quantum.h"
#include "via.h"
#include "calibration.h"
#include "diag.h"
#include <string.h>
void via_config_set_value(uint8_t *data);
void via_config_get_value(uint8_t *data);

#define CELLS (MATRIX_ROWS * MATRIX_COLS)
#define RECORD_BYTES (9 + CELLS * 2)
static uint16_t raw_values[CELLS], minima[CELLS], maxima[CELLS];
static bool seen[CELLS], logical_values[CELLS];
typedef struct { uint16_t raw, baseline, bottom, press, release, low, high, peak, extremum; uint8_t flags; } diag_cell_t;
static diag_cell_t frozen[CELLS];
static uint16_t snapshot_id;
static eeprom_ec_config_t backup_copy;
static uint8_t restore_data[RECORD_BYTES], restore_seen[(RECORD_BYTES + 7) / 8];
static uint16_t restore_token;
static uint32_t restore_crc, restore_started;
static uint16_t get16(const uint8_t *b) { return ((uint16_t)b[0] << 8) | b[1]; }
static void put16(uint8_t *b, uint16_t n) { b[0] = n >> 8; b[1] = n; }
static uint32_t get32(const uint8_t *b) { return ((uint32_t)get16(b) << 16) | get16(b + 2); }
static void put32(uint8_t *b, uint32_t n) { put16(b, n >> 16); put16(b + 2, n); }
static uint32_t crc32_bytes(const uint8_t *b, uint16_t n) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < n; ++i) { crc ^= b[i]; for (uint8_t j = 0; j < 8; ++j) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0); }
    return ~crc;
}
static void serialize(uint8_t *b, bool runtime) {
    b[0] = runtime ? ec_config.actuation_mode : eeprom_ec_config.actuation_mode;
    put16(b + 1, runtime ? ec_config.mode_0_actuation_threshold : eeprom_ec_config.mode_0_actuation_threshold);
    put16(b + 3, runtime ? ec_config.mode_0_release_threshold : eeprom_ec_config.mode_0_release_threshold);
    put16(b + 5, runtime ? ec_config.mode_1_initial_deadzone_offset : eeprom_ec_config.mode_1_initial_deadzone_offset);
    b[7] = runtime ? ec_config.mode_1_actuation_offset : eeprom_ec_config.mode_1_actuation_offset;
    b[8] = runtime ? ec_config.mode_1_release_offset : eeprom_ec_config.mode_1_release_offset;
}
static void saved_header(uint8_t *b){for(unsigned i=0;i<9;++i)b[i]=ec_record_byte(&backup_copy,i);}

void ec_diagnostic_sample(uint8_t row, uint8_t col, uint16_t raw, bool logical) {
    uint16_t i = row * MATRIX_COLS + col;
    ec_cal_sample(row,col,raw,logical);
    raw_values[i] = raw; logical_values[i] = logical;
    if (!seen[i]) { minima[i] = maxima[i] = raw; seen[i] = true; }
    else { if (raw < minima[i]) minima[i] = raw; if (raw > maxima[i]) maxima[i] = raw; }
}
static void freeze(void) {
    for (uint16_t i = 0; i < CELLS; ++i) {
        uint8_t r = i / MATRIX_COLS, c = i % MATRIX_COLS;
        diag_cell_t *s = &frozen[i];
        if(is_unused_position(r,c)){memset(s,0,sizeof(*s));continue;}
        s->raw = raw_values[i]; s->baseline = ec_config.noise_floor[r][c]; s->bottom = !ec_cal_active_valid()||ec_cal_method()==EC_CAL_AUTO?0:ec_cal_bottom(r,c);
        s->press = ec_config.rescaled_mode_0_actuation_threshold[r][c]; s->release = ec_config.rescaled_mode_0_release_threshold[r][c];
        s->extremum = ec_config.extremum[r][c];
        // Publish actual decision boundaries for implemented RT mode, including its initial deadzone.
        if(ec_config.actuation_mode==1&&ec_va_dynamic())ec_rt_boundaries(r,c,&s->press,&s->release);
        else if (ec_config.actuation_mode == 1) {
            uint16_t trigger = s->extremum + ec_config.rescaled_mode_1_actuation_offset[r][c];
            s->press = trigger > ec_config.rescaled_mode_1_initial_deadzone_offset[r][c] ? trigger : ec_config.rescaled_mode_1_initial_deadzone_offset[r][c];
            int release = (int)s->extremum - ec_config.rescaled_mode_1_release_offset[r][c];
            s->release = release < 0 ? 0 : release;
        }
        s->low = minima[i]; s->high = maxima[i]; s->peak = ec_config.bottoming_reading[r][c];
        if(s->peak>1023)s->peak=0;
        s->flags = seen[i] ? 1 | (logical_values[i] ? 2 : 0) | ((matrix_get_row(r) & ((matrix_row_t)1 << c)) ? 4 : 0) | ec_cal_cell_flags(r,c) : 0;
    }
    if (++snapshot_id == 0) ++snapshot_id;
}
// This checkout calls raw_hid_receive_kb for 07/08/09 only with VIA_CUSTOM_LIGHTING_ENABLE.
static void custom_via(uint8_t *d) {
    uint8_t id = d[2];
    if (d[0] == 9 && d[1] == 0) return; // donor custom-save is intentionally a no-op
    if (d[1] != 0 || (d[0] != 7 && d[0] != 8) || id < 1 || id > 11) { d[0] = 0xFF; return; }
    if (d[0] == 8) {
        if (id != 1 && id != 2 && id != 3 && id != 5 && id != 6 && id != 7) { d[0] = 0xFF; return; }
        via_config_get_value(d + 2); return;
    }
    d[0]=0xFF; // Old calibration/configuration writes are not a compatibility mode.

}
bool ec_diagnostic_command(uint8_t *d, uint8_t length) {
    if (length != 32) return false;
    if (d[0] == 7 || d[0] == 8 || d[0] == 9) { custom_via(d); return true; }
    if (d[0] != 0xCA || memcmp(d + 1, "EC42", 4)) return false;
    uint8_t len = d[10], op = d[6], p[20];
    bool malformed = d[5] != 2 || d[9] || d[11] || len > 20;
    if (!malformed) for (uint8_t i = 12 + len; i < 32; ++i) if (d[i]) malformed = true;
    memcpy(p, d + 12, 20); memset(d + 9, 0, 23); d[11] = 1;
    if (malformed) { d[9] = 1; return true; }
    uint8_t *out = d + 12;
    if(ec_cal_protocol(op,p,len,out,d+10,d+9))return true;
    switch (op) {
        case 0x10:
            if (len) goto bad;
            memcpy(out, "QEC2", 4); out[4] = 8; put16(out + 5, MATRIX_ROWS); put16(out + 7, MATRIX_COLS); put16(out + 9, 1023); out[11] = 1; put16(out + 12, 0x76); out[14] = 5; out[15] = 1; d[10] = 16; break;
        case 0x11:
            if (len) goto bad;
            freeze(); put16(out, snapshot_id); put32(out + 2, timer_read32()); put16(out + 6, CELLS); d[10] = 8; break;
        case 0x12: {
            if (len != 5) goto bad;
            if (!snapshot_id || get16(p) != snapshot_id) goto stale;
            uint16_t start = get16(p + 2); uint8_t count = p[4];
            if (!count || count > 5 || start >= CELLS || start + count > CELLS) goto range;
            memcpy(out, p, 5); for (uint8_t i = 0; i < count; ++i) { put16(out + 5 + i * 3, frozen[start + i].raw); out[7 + i * 3] = frozen[start + i].flags; }
            d[10] = 5 + count * 3; break;
        }
        case 0x13: {
            if (len != 4) goto bad;
            if (!snapshot_id || get16(p) != snapshot_id) goto stale;
            uint16_t i = get16(p + 2); if (i >= CELLS) goto range;
            diag_cell_t *s = &frozen[i]; memcpy(out, p, 4);
            put16(out + 4, s->baseline); put16(out + 6, s->bottom); put16(out + 8, s->press); put16(out + 10, s->release); put16(out + 12, s->low); put16(out + 14, s->high); put16(out + 16, s->peak); put16(out + 18, s->extremum); d[10] = 20; break;
        }
        case 0x14:
            if (len) goto bad;
            out[0]=ec_cal_state;put16(out+1,ec_cal_session);out[3]=ec_cal_method()>3?0:ec_cal_method();out[4]=ec_config.actuation_mode;out[5]=ec_cal_record_valid(&eeprom_ec_config)&&ec_storage_ready();d[10]=6; break;
        case 0x15:
            if (len != 1 || p[0] > 1) goto bad;
            if (p[0]) serialize(out, true); else { eeconfig_read_kb_datablock(&backup_copy, 0, EECONFIG_KB_DATA_SIZE); saved_header(out); } d[10] = 9; break;
        case 0x1D: // Read-only canonical active RAM record, never a persisted backup.
        case 0x16: {
            if (len != 3) goto bad;
            uint16_t offset = get16(p); uint8_t count = p[2];
            if (!count || count > 17 || offset >= RECORD_BYTES || offset + count > RECORD_BYTES) goto range;
            if (!offset) eeconfig_read_kb_datablock(&backup_copy, 0, EECONFIG_KB_DATA_SIZE);
            memcpy(out, p, 3); for (uint8_t i = 0; i < count; ++i) out[3 + i] = ec_record_byte(op==0x1D?&eeprom_ec_config:&backup_copy,offset+i); d[10] = count + 3; break;
        }
        case 0x17:
            if (len != 6 || get16(p) != RECORD_BYTES || ec_config.bottoming_calibration) goto bad;
            restore_crc = get32(p + 2); restore_token = get16(d + 7) ^ timer_read32(); if (!restore_token) restore_token = 1;
            restore_started = timer_read32(); memset(restore_seen, 0, sizeof(restore_seen)); put16(out, restore_token); d[10] = 2; break;
        case 0x18: {
            if (len < 6 || p[4] != len - 5) goto bad;
            if (!restore_token || get16(p) != restore_token || timer_elapsed32(restore_started) > 10000) goto stale;
            uint16_t offset = get16(p + 2); if (offset >= RECORD_BYTES || offset + p[4] > RECORD_BYTES) goto range;
            for (uint8_t i = 0; i < p[4]; ++i) { restore_data[offset + i] = p[5 + i]; restore_seen[(offset + i) / 8] |= 1 << ((offset + i) % 8); }
            break;
        }
        case 0x19: {
            if (len != 2 || ec_config.bottoming_calibration) goto bad;
            if (!restore_token || get16(p) != restore_token || timer_elapsed32(restore_started) > 10000) goto stale;
            restore_token = 0; // single-use; never blindly retry an uncertain commit
            for (uint16_t i = 0; i < RECORD_BYTES; ++i) if (!(restore_seen[i / 8] & (1 << (i % 8)))) goto bad;
            if (crc32_bytes(restore_data, RECORD_BYTES) != restore_crc ) goto bad;
            eeprom_ec_config_t incoming;
            for(unsigned i=0;i<RECORD_BYTES;++i)ec_record_set_byte(&incoming,i,restore_data[i]);
            if(!ec_cal_restore_record(&incoming))goto range;
            keyboard_post_init_kb();break;
        }
        case 0x1B:
            if (len) goto bad;
            ec_calibration_cancel(); break;
        case 0x1C:
            if (len) goto bad;
            memcpy(out, "0413-EC42-2.8-r1", 16); d[10] = 16; break;
        default: d[9] = 2;
    }
    return true;
bad: d[9] = 1; return true;
range: d[9] = 3; return true;
stale: d[9] = 4; return true;
}
