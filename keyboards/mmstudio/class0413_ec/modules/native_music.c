// SPDX-License-Identifier: GPL-2.0-or-later
/* Reuse this checkout's native music behavior, with bounded legacy recording. */
#define process_music ec_qmk_process_music
#define music_task ec_qmk_music_task
#include "quantum/process_keycode/process_music.c"
#undef process_music
#undef music_task

bool ec_actuators_blocked(void);

bool process_music(uint16_t keycode, keyrecord_t *record) {
    /* Legacy code writes past its 16-note array when recording more notes.
     * Complete a full recording instead; also reject empty playback below. */
    if (music_sequence_recording && music_sequence_count >= sizeof(music_sequence)) {
        music_sequence_recording = false;
        music_sequence_recorded = true;
    }
    if (music_sequence_count == 0) music_sequence_recorded = false;
    return ec_qmk_process_music(keycode, record);
}

void music_task(void) {
    if (!ec_actuators_blocked()) ec_qmk_music_task();
}
