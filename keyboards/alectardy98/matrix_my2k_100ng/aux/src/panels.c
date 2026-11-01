/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Independent behavioral implementation, not decompiled application code.
 * Mono command facts match upstream u8g2 SSD1306_72x40_ER. Color command facts
 * follow the official release's 128x128 DCS-compatible initialization tables.
 * Controller compatibility is established statically; fitted part marks are not. */
#if MY2K_DEV22_PROFILE
#    include "aux_perf22.h"
#endif
#include "aux_panels.h"
#if MY2K_DEV21_PROFILE
#    include "aux_perf21.h"
#    include "aux_platform.h"
#endif
static bool signal(aux_panels *p, aux_panel_id id, unsigned line, bool high) {
    return p->bus.signal(p->bus.context, id, line, high);
}
static bool bytes(aux_panels *p, aux_panel_id id, bool data, const uint8_t *b, size_t n) {
    return signal(p, id, AUX_PANEL_DC, data) && p->bus.send(p->bus.context, id, b, n);
}
static bool command(aux_panels *p, aux_panel_id id, uint8_t cmd, const uint8_t *args, size_t n) {
    if (!bytes(p, id, false, &cmd, 1))
        return false;
    return !n || bytes(p, id, true, args, n);
}
typedef struct {
    uint8_t  command, size, data[16];
    uint16_t delay;
} setting;
static const setting color_settings[] = {
    {0x01, 0, {0}, 150},
    {0x11, 0, {0}, 500},
    {0xb1, 3, {1, 44, 45}, 0},
    {0xb2, 3, {1, 44, 45}, 0},
    {0xb3, 6, {1, 44, 45, 1, 44, 45}, 0},
    {0xb4, 1, {7}, 0},
    {0xc0, 3, {0xa2, 2, 0x84}, 0},
    {0xc1, 1, {0xc5}, 0},
    {0xc2, 2, {10, 0}, 0},
    {0xc3, 2, {0x8a, 0x2a}, 0},
    {0xc4, 2, {0x8a, 0xee}, 0},
    {0xc5, 1, {14}, 0},
    {0x20, 0, {0}, 0},
    {0x36, 1, {0xc8}, 0},
    {0x3a, 1, {5}, 0},
    {0x2a, 4, {0, 0, 0, 127}, 0},
    {0x2b, 4, {0, 0, 0, 127}, 0},
    {0x21, 0, {0}, 0},
    {0xf0, 14, {31, 40, 4, 62, 42, 46, 32, 0, 12, 6, 0, 28, 31, 15}, 0},
    {0xf1, 14, {0, 45, 47, 60, 111, 28, 11, 0, 0, 0, 7, 13, 17, 15}, 0},
};
static const setting color_finish[] = {{0x13, 0, {0}, 10}, {0x29, 0, {0}, 100}};
/* The OLED treats arguments as command-stream bytes: keep D/C low throughout. */
static const uint8_t mono_settings[] = {0xae, 0xd5, 0x80, 0xa8, 0x27, 0xd3, 0,    0xad, 0x30, 0x8d,
                                        0x14, 0x40, 0xa6, 0xa4, 0x20, 0,    0xa1, 0xc8, 0xda, 0x12,
                                        0x81, 0xaf, 0xd9, 0x22, 0xdb, 0x20, 0x2e, 0xaf};
/* Original AUX 0x08002bd8 clears the complete visible 128x128 window before
 * normal/display-on commands. Chunk the independently generated black pixels;
 * no proprietary image data or full-frame buffer is needed. */
static bool color_clear_before_enable(aux_panels *p) {
    const uint8_t columns[] = {0, 2, 0, 129}, rows[] = {0, 1, 0, 128}, black[128] = {0};
    if (!command(p, AUX_PANEL_COLOR, 0x2a, columns, sizeof columns) ||
        !command(p, AUX_PANEL_COLOR, 0x2b, rows, sizeof rows) || !command(p, AUX_PANEL_COLOR, 0x2c, 0, 0))
        return false;
    for (unsigned sent = 0; sent < 128u * 128u * 2u; sent += sizeof black)
        if (!bytes(p, AUX_PANEL_COLOR, true, black, sizeof black))
            return false;
    return true;
}
bool aux_panels_init(aux_panels *p, const aux_panel_bus *bus) {
    if (!p || !bus || !bus->configure || !bus->signal || !bus->send || !bus->delay)
        return false;
    *p = (aux_panels){.bus = *bus, .mono_enabled = true};
    if (!bus->configure(bus->context))
        return false;
    bool ok = signal(p, AUX_PANEL_COLOR, AUX_PANEL_POWER, false) && signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, false) &&
              signal(p, AUX_PANEL_COLOR, AUX_PANEL_RESET, false);
    ok      = ok && bus->delay(bus->context, 5) && signal(p, AUX_PANEL_COLOR, AUX_PANEL_RESET, true);
    for (unsigned i = 0; ok && i < sizeof color_settings / sizeof color_settings[0]; i++) {
        const setting *s = &color_settings[i];
        ok               = command(p, AUX_PANEL_COLOR, s->command, s->data, s->size);
        if (ok && s->delay)
            ok = bus->delay(bus->context, s->delay);
    }
    if (ok)
        ok = color_clear_before_enable(p);
    for (unsigned i = 0; ok && i < sizeof color_finish / sizeof color_finish[0]; i++) {
        const setting *s = &color_finish[i];
        ok               = command(p, AUX_PANEL_COLOR, s->command, s->data, s->size);
        if (ok && s->delay)
            ok = bus->delay(bus->context, s->delay);
    }
    bool released    = signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true);
    p->color_ready   = ok && released;
    p->color_capable = p->color_on = p->color_target = p->color_ready;
    ok            = signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, true) && signal(p, AUX_PANEL_MONO, AUX_PANEL_RESET, false);
    ok            = ok && bus->delay(bus->context, 100) && signal(p, AUX_PANEL_MONO, AUX_PANEL_RESET, true) &&
                    bus->delay(bus->context, 100);
    ok            = ok && signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, false) &&
                    bytes(p, AUX_PANEL_MONO, false, mono_settings, sizeof mono_settings);
    released      = signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, true);
    p->mono_ready = ok && released;
    return p->color_ready && p->mono_ready;
}
/* Runtime color power follows the recovered enable/reset/settings/clear and
 * clear/power-off order (stock 0x0800284c), with cooperative waits. The initial
 * boot initializer above still waits synchronously. No new controller opcodes. */
enum {
    COLOR_IDLE,
    COLOR_RESET_LOW,
    COLOR_RESET_HIGH,
    COLOR_SETTINGS,
    COLOR_WINDOW,
    COLOR_CLEAR,
    COLOR_FINISH,
    COLOR_RELEASE,
    COLOR_OFF
};
bool aux_panel_request_color_power(aux_panels *p, bool on) {
    if (!p || !p->color_capable || p->color_fault)
        return false;
    p->color_target = on;
    return true;
}
bool aux_panel_request_mono_visible(aux_panels *p, bool visible) {
    if (!p || !p->mono_ready)
        return false;
    if (p->mono_enabled != visible) {
        p->mono_enabled = visible;
        p->mono_dirty   = true;
        p->mono_page    = 0;
    }
    return true;
}
bool aux_panel_color_busy(const aux_panels *p) {
    return p && (p->color_waiting || p->color_phase != COLOR_IDLE || p->color_on != p->color_target);
}
bool aux_panels_busy(const aux_panels *p) {
    return aux_panel_color_busy(p) || (p && p->mono_ready && p->mono_dirty);
}
bool aux_panels_visual_sleep(aux_panels *p, bool asleep) {
    if (!p || !p->color_ready || !p->mono_ready || aux_panel_color_busy(p) || p->color_streaming)
        return false;
    /* ST7735S v1.3 sections10.1.18/19:28/29 retain GRAM and other state.
     * SSD1306 Rev1.1 section10.1.12:AE/AF blank/wake without reset.
     * Fitted compatibility still requires physical acceptance; no pin guessed. */
    bool okay = true;
    for (unsigned i = 0; i < 2; i++) {
        bool *blank = i ? &p->mono_blanked : &p->color_blanked;
        if (*blank == asleep)
            continue;
        aux_panel_id id = i ? AUX_PANEL_MONO : AUX_PANEL_COLOR;
        bool         ok = true;
#if AUX_VISUAL_SLEEP_POWER
        /* Use only the existing color-control signal. Physical retention/wake is
         * acceptance-tested; do not infer its electrical net from the name. */
        if (!i && !asleep)
            ok = signal(p, id, AUX_PANEL_POWER, false);
#endif
        ok = ok && signal(p, id, AUX_PANEL_CS, false) &&
             command(p, id, i ? (asleep ? 0xae : 0xaf) : (asleep ? 0x28 : 0x29), 0, 0);
        ok = signal(p, id, AUX_PANEL_CS, true) && ok;
#if AUX_VISUAL_SLEEP_POWER
        if (!i && asleep && ok)
            ok = signal(p, id, AUX_PANEL_POWER, true);
#endif
        if (ok)
            *blank = asleep;
        else
            okay = false;
    }
    return okay;
}
static void color_wait(aux_panels *p, uint32_t now, uint32_t delay) {
    p->color_deadline = now + delay;
    p->color_waiting  = delay != 0;
}
static bool color_window(aux_panels *p) {
    const uint8_t columns[] = {0, 2, 0, 129}, rows[] = {0, 1, 0, 128};
    return command(p, AUX_PANEL_COLOR, 0x2a, columns, 4) && command(p, AUX_PANEL_COLOR, 0x2b, rows, 4) &&
           command(p, AUX_PANEL_COLOR, 0x2c, 0, 0);
}
static bool color_service(aux_panels *p, uint32_t now) {
    if (p->color_fault || !p->color_capable)
        return true;
    if (p->color_waiting) {
        if ((int32_t)(now - p->color_deadline) < 0)
            return true;
        p->color_waiting = false;
    }
    switch (p->color_phase) {
        case COLOR_IDLE:
            if (p->color_on == p->color_target)
                return true;
            if (p->color_streaming) {
                if (!signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true))
                    return false;
                p->color_streaming = false;
            }
            p->color_ready   = false;
            p->color_setting = 0;
            p->color_clear   = 0;
            if (p->color_on) {
                p->color_phase = COLOR_WINDOW;
                return signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, false);
            }
            p->color_phase = COLOR_RESET_LOW;
            color_wait(p, now, 1);
            return signal(p, AUX_PANEL_COLOR, AUX_PANEL_POWER, false);
        case COLOR_RESET_LOW:
            p->color_phase = COLOR_RESET_HIGH;
            color_wait(p, now, 5);
            return signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, false) &&
                   signal(p, AUX_PANEL_COLOR, AUX_PANEL_RESET, false);
        case COLOR_RESET_HIGH:
            p->color_phase = COLOR_SETTINGS;
            return signal(p, AUX_PANEL_COLOR, AUX_PANEL_RESET, true);
        case COLOR_SETTINGS: {
            const setting *s = &color_settings[p->color_setting++];
            if (p->color_setting == sizeof color_settings / sizeof color_settings[0])
                p->color_phase = COLOR_WINDOW;
            color_wait(p, now, s->delay);
            return command(p, AUX_PANEL_COLOR, s->command, s->data, s->size);
        }
        case COLOR_WINDOW:
            p->color_phase = COLOR_CLEAR;
            return color_window(p);
        case COLOR_CLEAR: {
            const uint8_t black[128] = {0};
            p->color_clear += sizeof black;
            if (p->color_clear == 128u * 128u * 2u) {
                p->color_setting = 0;
                p->color_phase   = p->color_on ? COLOR_OFF : COLOR_FINISH;
            }
            return bytes(p, AUX_PANEL_COLOR, true, black, sizeof black);
        }
        case COLOR_FINISH: {
            const setting *s = &color_finish[p->color_setting++];
            if (p->color_setting == sizeof color_finish / sizeof color_finish[0])
                p->color_phase = COLOR_RELEASE;
            color_wait(p, now, s->delay);
            return command(p, AUX_PANEL_COLOR, s->command, s->data, s->size);
        }
        case COLOR_RELEASE:
            p->color_phase = COLOR_IDLE;
            p->color_on    = true;
            p->color_ready = true;
            return signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true);
        case COLOR_OFF:
            p->color_phase = COLOR_IDLE;
            p->color_on    = false;
            color_wait(p, now, 1);
            /* Always release chip select before removing color power. */
            return signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true) && signal(p, AUX_PANEL_COLOR, AUX_PANEL_POWER, true);
        default:
            return false;
    }
}
static bool mono_page(aux_panels *p, unsigned page, const uint8_t *data) {
    uint8_t address[] = {0x11, 0x0c, (uint8_t)(0xb0 | page)};
    bool    ok        = signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, false) && bytes(p, AUX_PANEL_MONO, false, address, 3) &&
                        bytes(p, AUX_PANEL_MONO, true, data, 72);
    return signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, true) && ok;
}
bool aux_panels_begin(aux_panels *p, const aux_panel_bus *bus, uint32_t now) {
    if (!p || !bus || !bus->configure || !bus->signal || !bus->send || !bus->delay)
        return false;
    *p = (aux_panels){.bus = *bus, .mono_enabled = true};
    if (!bus->configure(bus->context))
        return false;
    p->color_capable = p->color_target = true;
    /* Color uses the existing runtime initializer, including the black clear
     * before display-on. OLED has separate SPI/reset pins and its own deadlines. */
    bool ok = signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, true) && signal(p, AUX_PANEL_MONO, AUX_PANEL_RESET, false);
    if (ok) {
        p->mono_startup  = 1;
        p->mono_deadline = now + 100u;
    }
    return ok;
}
bool aux_panels_starting(const aux_panels *p) {
    return p && (aux_panel_color_busy(p) || p->mono_startup != 0);
}
static void mono_startup_service(aux_panels *p, uint32_t now) {
    if (!p->mono_startup || (int32_t)(now - p->mono_deadline) < 0)
        return;
    if (p->mono_startup == 1) {
        if (signal(p, AUX_PANEL_MONO, AUX_PANEL_RESET, true)) {
            p->mono_startup  = 2;
            p->mono_deadline = now + 100u;
        } else
            p->mono_startup = 0;
        return;
    }
    bool ok         = signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, false) &&
                      bytes(p, AUX_PANEL_MONO, false, mono_settings, sizeof mono_settings);
    bool released   = signal(p, AUX_PANEL_MONO, AUX_PANEL_CS, true);
    p->mono_ready   = ok && released;
    p->mono_startup = 0;
}
void aux_panels_service(aux_panels *p, uint32_t now) {
    if (!p)
        return;
    if (!color_service(p, now)) {
        (void)signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true);
        p->color_fault   = true;
        p->color_ready   = false;
        p->color_phase   = COLOR_IDLE;
        p->color_waiting = false;
        p->color_target  = p->color_on;
    }
    mono_startup_service(p, now);
    if (p->mono_ready && p->mono_dirty && !p->mono_blanked) {
        const uint8_t  blank[72] = {0};
        const uint8_t *image     = p->mono_overlay_enabled ? p->mono_overlay : p->mono;
#if MY2K_DEV22_PROFILE
        if (!p->mono_page) {
            aux_mono_perf.started_ms     = aux_millis();
            aux_mono_perf.current_cycles = 0;
        }
        uint32_t started = *(volatile uint32_t *)0xe0001004u;
#endif
        bool okay = mono_page(p, p->mono_page, p->mono_enabled ? image + p->mono_page * 72 : blank);
#if MY2K_DEV22_PROFILE
        aux_mono_perf.current_cycles += *(volatile uint32_t *)0xe0001004u - started;
        if (okay)
            aux_mono_perf.pages++;
        else
            aux_mono_perf.failures++;
#endif
        if (!okay) {
            p->mono_ready = false;
            p->mono_dirty = false;
        } else if (++p->mono_page == 5) {
            p->mono_dirty = false;
            p->mono_page  = 0;
#if MY2K_DEV22_PROFILE
            aux_mono_perf.flushes++;
            aux_mono_perf.last_cycles = aux_mono_perf.current_cycles;
            aux_mono_perf.last_ms     = aux_millis() - aux_mono_perf.started_ms;
            if (aux_mono_perf.last_cycles > aux_mono_perf.max_cycles)
                aux_mono_perf.max_cycles = aux_mono_perf.last_cycles;
            if (aux_mono_perf.last_ms > aux_mono_perf.max_ms)
                aux_mono_perf.max_ms = aux_mono_perf.last_ms;
#endif
        }
    }
}
bool aux_panel_color_pixels(void *context, uint16_t w, uint16_t h, uint32_t first, const uint16_t *pixels, unsigned n) {
    aux_panels *p = context;
    if (!p || !p->color_ready || aux_panel_color_busy(p) || !pixels || !w || w > 128 || !h || h > 128 ||
        first > (uint32_t)w * h || n > (uint32_t)w * h - first)
        return false;
    if (!n)
        return true;
#if MY2K_DEV21_PROFILE
    if (!first && n) {
        aux_display_perf.frame_started_ms = aux_millis();
        aux_display_perf.frame_active     = true;
    }
    uint32_t end_pixel = first + n;
#endif
#if MY2K_DEV22_PROFILE
    bool scope = aux_perf22_enter(PERF22_PANEL);
#endif
#if AUX_COLOR_STREAM
    if (!first || p->color_streaming) {
        bool     ok     = true;
        uint32_t finish = first + n;
        if (!first) {
            if (p->color_streaming)
                ok = signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true);
            p->color_streaming      = false;
            const uint8_t columns[] = {0, 2, 0, (uint8_t)(w + 1)}, rows[] = {0, 1, 0, (uint8_t)h};
            ok = ok && signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, false) &&
                 command(p, AUX_PANEL_COLOR, 0x2a, columns, 4) && command(p, AUX_PANEL_COLOR, 0x2b, rows, 4) &&
                 command(p, AUX_PANEL_COLOR, 0x2c, 0, 0);
            p->stream_width    = w;
            p->stream_height   = h;
            p->stream_next     = 0;
            p->color_streaming = ok;
        }
        if (!p->color_streaming || p->stream_width != w || p->stream_height != h || p->stream_next != first)
            ok = false;
        uint8_t packed[512];
        while (ok && n) {
            unsigned take = n > 256u ? 256u : n;
            for (unsigned i = 0; i < take; i++) {
                packed[2 * i]     = (uint8_t)(pixels[i] >> 8);
                packed[2 * i + 1] = (uint8_t)pixels[i];
            }
            ok = bytes(p, AUX_PANEL_COLOR, true, packed, take * 2u);
            pixels += take;
            n -= take;
        }
        if (ok)
            p->stream_next = finish;
        if (!ok || finish == (uint32_t)w * h) {
            bool released      = signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true);
            p->color_streaming = false;
            ok                 = ok && released;
        }
        if (!ok) {
            p->color_ready = false;
            p->color_fault = true;
#    if MY2K_DEV21_PROFILE
            aux_display_perf.failed_frames++;
            aux_display_perf.frame_active = false;
#    endif
        }
#    if MY2K_DEV21_PROFILE
        if (ok && aux_display_perf.frame_active && finish == (uint32_t)w * h) {
            uint32_t ms = aux_millis() - aux_display_perf.frame_started_ms;
            if (!aux_display_perf.frames)
                aux_startup.first_frame_ms = aux_millis();
            aux_display_perf.frames++;
            aux_display_perf.last_frame_ms = ms;
            aux_display_perf.total_frame_ms += ms;
            if (ms > aux_display_perf.max_frame_ms)
                aux_display_perf.max_frame_ms = ms;
            unsigned bin = ms <= 8 ? 0 : ms <= 16 ? 1 : ms <= 24 ? 2 : ms <= 33 ? 3 : 4;
            aux_display_perf.histogram[bin]++;
            aux_display_perf.frame_active = false;
        }
#    endif
#    if MY2K_DEV22_PROFILE
        aux_perf22_leave(scope);
#    endif
        return ok;
    }
#endif
    uint8_t encoded[256];
    while (n) {
        uint32_t x = first % w, y = first / w;
        unsigned take = w - x;
        if (take > n)
            take = n;
        uint8_t columns[] = {0, (uint8_t)(x + 2), 0, (uint8_t)(x + take + 1)};
        uint8_t rows[]    = {0, (uint8_t)(y + 1), 0, (uint8_t)(y + 1)};
        for (unsigned i = 0; i < take; i++) {
            encoded[2 * i]     = (uint8_t)(pixels[i] >> 8);
            encoded[2 * i + 1] = (uint8_t)pixels[i];
        }
        bool ok = signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, false) && command(p, AUX_PANEL_COLOR, 0x2a, columns, 4) &&
                  command(p, AUX_PANEL_COLOR, 0x2b, rows, 4) && command(p, AUX_PANEL_COLOR, 0x2c, 0, 0) &&
                  bytes(p, AUX_PANEL_COLOR, true, encoded, take * 2);
        if (!signal(p, AUX_PANEL_COLOR, AUX_PANEL_CS, true) || !ok) {
            p->color_ready = false;
            p->color_fault = true;
#if MY2K_DEV21_PROFILE
            aux_display_perf.failed_frames++;
            aux_display_perf.frame_active = false;
#endif
#if MY2K_DEV22_PROFILE
            aux_perf22_leave(scope);
#endif
            return false;
        }
        first += take;
        pixels += take;
        n -= take;
    }
#if MY2K_DEV21_PROFILE
    if (aux_display_perf.frame_active && end_pixel == (uint32_t)w * h) {
        uint32_t ms = aux_millis() - aux_display_perf.frame_started_ms;
        if (!aux_display_perf.frames)
            aux_startup.first_frame_ms = aux_millis();
        aux_display_perf.frames++;
        aux_display_perf.last_frame_ms = ms;
        aux_display_perf.total_frame_ms += ms;
        if (ms > aux_display_perf.max_frame_ms)
            aux_display_perf.max_frame_ms = ms;
        unsigned bin = ms <= 8 ? 0 : ms <= 16 ? 1 : ms <= 24 ? 2 : ms <= 33 ? 3 : 4;
        aux_display_perf.histogram[bin]++;
        aux_display_perf.frame_active = false;
    }
#endif
#if MY2K_DEV22_PROFILE
    aux_perf22_leave(scope);
#endif
    return true;
}
bool aux_panel_mono_pixels(void *context, uint16_t w, uint16_t h, uint32_t first, const uint16_t *pixels, unsigned n) {
    aux_panels *p = context;
    if (!p || !p->mono_ready || !pixels || !w || w > 72 || !h || h > 40 || first > (uint32_t)w * h ||
        n > (uint32_t)w * h - first)
        return false;
    if (!n)
        return true;
    if (!first)
        for (unsigned i = 0; i < sizeof p->mono; i++)
            p->mono[i] = 0;
    for (unsigned i = 0; i < n; i++) {
        uint32_t x = (first + i) % w, y = (first + i) / w;
        uint8_t  mask = (uint8_t)(1u << (y & 7));
        if (pixels[i])
            p->mono[(y / 8) * 72 + x] |= mask;
        else
            p->mono[(y / 8) * 72 + x] &= (uint8_t)~mask;
    }
    if (first + n < (uint32_t)w * h)
        return true;
    /* The service loop also handles blanking/restoring this same RAM image. */
    p->mono_dirty = true;
    p->mono_page  = 0;
    return true;
}
