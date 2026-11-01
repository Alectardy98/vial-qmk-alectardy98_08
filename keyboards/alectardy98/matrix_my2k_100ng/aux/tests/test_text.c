/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_text.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool dc, fail_send;
static unsigned page, data_bytes;
static uint8_t shown[360];
static bool line(void *ctx, aux_panel_id panel, unsigned signal, bool high) {
    (void)ctx; assert(panel == AUX_PANEL_MONO);
    if (signal == AUX_PANEL_DC) dc = high;
    return true;
}
static bool send_bytes(void *ctx, aux_panel_id panel, const uint8_t *bytes, size_t size) {
    (void)ctx; assert(panel == AUX_PANEL_MONO);
    if (fail_send) return false;
    if (!dc) { assert(size == 3); page = bytes[2] & 7; }
    else { assert(size == 72 && page < 5); memcpy(shown + page * 72, bytes, size); data_bytes += size; }
    return true;
}
static void apply(aux_text *text, const char *frame) {
    my2k_wire_message message;
    assert(my2k_wire_parse(frame, strlen(frame), &message));
    assert(aux_text_apply(text, &message));
}
static void step(aux_text *text, aux_panels *panels) {
    data_bytes = 0;
    aux_text_service(text, panels);
    aux_panels_service(panels, 0);
    assert(data_bytes <= 72);
}
static void finish(aux_text *text, aux_panels *panels) {
    unsigned n = 0;
    do { step(text, panels); assert(++n <= 10); } while (text->dirty || panels->mono_dirty);
}
static void unhex(const char *hex, uint8_t *out, unsigned size) {
    assert(strlen(hex) == size * 2);
    for (unsigned i = 0; i < size; i++) { unsigned byte; assert(sscanf(hex + 2*i, "%2x", &byte) == 1); out[i] = byte; }
}
static void original_fixture(const char *path) {
    FILE *file = fopen(path, "r"); assert(file);
    char row[256]; aux_text text = {0}; unsigned checked = 0;
    while (fgets(row, sizeof row, file)) {
        if (row[0] == '#' || !strncmp(row, "frame_without", 13)) continue;
        char frame[128], payload_hex[17], history_hex[15]; unsigned count, current, dirty;
        assert(sscanf(row, "%127[^\t]\t%16s\t%14s\t%u\t%u\t%u", frame, payload_hex, history_hex, &count, &current, &dirty) == 6);
        strcat(frame, "\n");
        my2k_wire_message message;
        assert(my2k_wire_parse(frame, strlen(frame), &message));
        uint8_t payload[8], history[7]; unhex(payload_hex, payload, 8); unhex(history_hex, history, 7);
        assert(message.type == MY2K_TX && !memcmp(message.v, payload, 8));
        char encoded[32]; assert(my2k_wire_encode(&message, encoded, sizeof encoded) == (int)strlen(frame));
        assert(!strcmp(encoded, frame));
        assert(aux_text_apply(&text, &message));
        assert(!memcmp(text.history, history, 7) && text.count == count && text.current == current && text.dirty == dirty);
        checked++;
    }
    assert(!ferror(file) && checked == 15); fclose(file);
}
int main(int argc, char **argv) {
    assert(argc == 2); original_fixture(argv[1]);
    aux_text text = {0};
    aux_panels panels = {.mono_ready=true, .mono_enabled=true, .bus={.signal=line, .send=send_bytes}};
    memset(panels.mono, 0xa5, sizeof panels.mono);
    apply(&text, "TX:A;\n"); finish(&text, &panels);
    assert(panels.mono_overlay_enabled && !memcmp(shown, panels.mono_overlay, 360));
    /* Enlarged QMK A begins at (44,20); untouched asset and unused upper pages. */
    for (unsigned i = 0; i < 144; i++) assert(!shown[i]);
    unsigned lit = 0; for (unsigned i = 0; i < 360; i++) { lit += shown[i] != 0; assert(panels.mono[i] == 0xa5); }
    assert(lit > 0);
    /* New events wait for a complete flush; they cannot restart page zero
     * indefinitely under a fast stream. The latest bounded state is rendered. */
    for (unsigned i = 0; i < 30; i++) { apply(&text, "TX:B;\n"); step(&text, &panels); assert(page == i % 5); }
    finish(&text, &panels); assert(text.current == 'B' && text.count == 7);
    assert(aux_panel_request_mono_visible(&panels, false)); finish(&text, &panels);
    for (unsigned i = 0; i < 360; i++) assert(!shown[i]);
    apply(&text, "TX:C;\n"); finish(&text, &panels);
    for (unsigned i = 0; i < 360; i++) assert(!shown[i]);
    assert(aux_panel_request_mono_visible(&panels, true)); finish(&text, &panels);
    assert(!memcmp(shown, panels.mono_overlay, 360));
    apply(&text, "TX:OFF;\n"); finish(&text, &panels);
    assert(!panels.mono_overlay_enabled && !memcmp(shown, panels.mono, 360));
    /* Invalid direct messages are atomic; parser rejects ambiguous forms. */
    aux_text before = text; my2k_wire_message invalid = {.type=MY2K_TX, .v={'A',0,'B'}};
    assert(!aux_text_apply(&text, &invalid) && !memcmp(&text, &before, sizeof text));
    apply(&text, "TX:Z;\n"); fail_send = true; step(&text, &panels);
    assert(!panels.mono_ready && !panels.mono_dirty);
    puts("AUX original TX parser/dispatcher fixtures, bounded history/rendering, flush fairness, power precedence and asset restoration: PASS");
    return 0;
}
