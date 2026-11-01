/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_AMK_H
#define MY2K_AMK_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MY2K_AMK_REPORT_SIZE 32u
/* Protocol types0 matrix,1strip,2indicator,3grid; index is global LED except
 * indicator config uses indicator ordinal. Reset/updater/file operations reject. */
typedef struct {
    uint8_t config, enabled, mode, custom, rows, cols;
} my2k_rgb_info;
/* Optional opt-in RAM mutations. Callbacks must validate the whole request before
 * modifying state and return false without changes on rejection. No persistence,
 * calibration, filesystem, reset or peripheral-settings I/O is authorized here.
 * A runtime backend may release existing USB key reports before a report-format
 * change (e.g. NKRO); that does not authorize persistent setting writes. */
typedef struct {
    bool (*scalar)(void *, uint8_t set_command, uint8_t value);
    bool (*led)(void *, uint8_t rgb_type, uint16_t index, const uint8_t hsv_flags[4]);
    bool (*mode)(void *, uint8_t rgb_type, uint8_t index, uint8_t mode);
    bool (*param)(void *, uint8_t rgb_type, uint8_t parameter, uint8_t index, const uint8_t *value, size_t size);
} my2k_amk_ram_writes;
typedef struct {
    void    *context;
    uint16_t total_leds;
    bool (*scalar)(void *, uint8_t command, uint8_t *value);
    bool (*count)(void *, uint8_t rgb_type, uint8_t *value);
    bool (*info)(void *, uint8_t rgb_type, uint8_t index, my2k_rgb_info *value);
    bool (*led)(void *, uint8_t rgb_type, uint16_t index, uint8_t hsv_flags[4]);
    bool (*rendered)(void *, uint16_t global_index, uint8_t rgb[3]);
    bool (*param)(void *, uint8_t rgb_type, uint8_t parameter, uint8_t index, uint8_t value[3]);
    const my2k_amk_ram_writes *ram_writes; /* NULL means read-only. */
} my2k_amk_backend;
/* true means FD packet handled in-place (successAA/failure55). false leaves
 * non-FD packets untouched for standard Vial/VIA routing. Exactly32bytes only.
 * Read callbacks use cached RAM. Optional SET callbacks mutate RAM only; caller
 * enforces capability/lock policy. This module never persists or sends a reply. */
bool my2k_amk_dispatch(const my2k_amk_backend *backend, uint8_t *report, size_t size);
#endif
