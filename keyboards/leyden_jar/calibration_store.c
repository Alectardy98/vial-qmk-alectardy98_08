// SPDX-License-Identifier: GPL-2.0-or-later
/* Preferences only: startup references and measured levels are never saved.
 * A torn or incompatible record falls back to compiled defaults. All writes
 * use QMK's existing wear-levelled EEPROM, never raw flash operations.
 */
#include "quantum.h"
#include "eeconfig.h"
#include "calibration_store.h"
#include <stddef.h>
#include <string.h>

typedef struct {
    uint32_t magic;
    uint32_t signature;
    uint16_t saved;
    uint8_t version;
    uint8_t bins;
    int16_t offsets[LEYDEN_JAR_MAX_CAL_BINS];
    uint32_t checksum;
} calibration_record_t;

_Static_assert(sizeof(calibration_record_t) == 48, "Calibration EEPROM format changed");
_Static_assert(EECONFIG_KB_DATA_SIZE == sizeof(calibration_record_t), "Calibration EEPROM block size mismatch");

static calibration_record_t state;
static const int16_t *default_offsets;

static uint32_t checksum(const calibration_record_t *record) {
    const uint8_t *bytes = (const uint8_t *)record;
    uint32_t value = 2166136261u;
    for (size_t i = 0; i < offsetof(calibration_record_t, checksum); ++i) {
        value = (value ^ bytes[i]) * 16777619u;
    }
    return value;
}

void leyden_jar_calibration_store_load(uint32_t signature, uint8_t bins, const int16_t *defaults) {
    default_offsets = defaults;
    calibration_record_t stored;
    eeconfig_read_kb_datablock(&stored, 0, sizeof(stored));
    if (stored.magic == 0x4C4A4341 && stored.version == 1 && stored.signature == signature &&
        stored.bins == bins && bins <= LEYDEN_JAR_MAX_CAL_BINS &&
        !(stored.saved >> bins) && stored.checksum == checksum(&stored)) {
        state = stored;
        return;
    }
    memset(&state, 0, sizeof(state));
    state.magic = 0x4C4A4341;
    state.version = 1;
    state.signature = signature;
    state.bins = bins;
}

bool leyden_jar_calibration_is_saved(uint8_t bin) {
    return bin < state.bins && (state.saved & (1u << bin));
}

int16_t leyden_jar_calibration_offset(uint8_t bin) {
    return leyden_jar_calibration_is_saved(bin) ? state.offsets[bin] : default_offsets[bin];
}

static bool commit(calibration_record_t *next) {
    next->checksum = checksum(next);
    eeconfig_update_kb_datablock(next, 0, sizeof(*next));
    calibration_record_t verify;
    eeconfig_read_kb_datablock(&verify, 0, sizeof(verify));
    if (memcmp(next, &verify, sizeof(verify)) != 0) {
        return false;
    }
    state = *next;
    return true;
}

bool leyden_jar_calibration_store_save(uint8_t bin, int16_t offset) {
    if (bin >= state.bins || offset < -1023 || offset > 1023 || offset == 0) {
        return false;
    }
    if (leyden_jar_calibration_is_saved(bin) && state.offsets[bin] == offset) {
        return true; // Repeated Apply does not write EEPROM again.
    }
    calibration_record_t next = state;
    next.offsets[bin] = offset;
    next.saved |= 1u << bin;
    return commit(&next);
}

bool leyden_jar_calibration_store_reset(void) {
    if (!state.saved) {
        return true;
    }
    calibration_record_t next = state;
    next.saved = 0;
    memset(next.offsets, 0, sizeof(next.offsets));
    return commit(&next);
}
