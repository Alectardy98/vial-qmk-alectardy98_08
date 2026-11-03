#!/usr/bin/env python3
"""Test real rendering/SPI packets/button hooks on the host and export previews.

Usage: python3 tools/test_display.py /tmp/adbkal-display-previews
The tests use ASan/UBSan and require a host C compiler, not attached hardware.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

keyboard = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/adbkal-display-previews").resolve()
output.mkdir(parents=True, exist_ok=True)

with tempfile.TemporaryDirectory(prefix="adbkal-display-test-") as directory:
    work = Path(directory)
    (work / "quantum.h").write_text("""
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef union {
    uint8_t raw;
    struct { uint8_t num_lock:1, caps_lock:1, scroll_lock:1, reserved:5; };
} led_t;
typedef struct { struct { struct { uint8_t row, col; } key; bool pressed; } event; } keyrecord_t;
#define ADBK_DISPLAY_CS_PIN 1
#define NO_PIN 255
#define ADBK_SCREEN 0x7e00
led_t host_keyboard_led_state(void);
uint8_t get_current_wpm(void);
bool wpm_keycode(uint16_t keycode);
bool pre_process_record_user(uint16_t keycode, keyrecord_t *record);
bool process_record_user(uint16_t keycode, keyrecord_t *record);
void keyboard_post_init_user(void);
void housekeeping_task_user(void);
void wait_us(unsigned us);
""")
    (work / "gpio.h").write_text("""
#include "quantum.h"
void setPinOutput(unsigned pin);
void writePinLow(unsigned pin);
void writePinHigh(unsigned pin);
""")
    (work / "timer.h").write_text("""
#include <stdint.h>
uint32_t timer_read32(void);
""")
    (work / "spi_master.h").write_text("""
#include "quantum.h"
typedef int spi_status_t;
#define SPI_STATUS_SUCCESS 0
void spi_init(void);
bool spi_start(unsigned pin, bool lsb, unsigned mode, unsigned divider);
void spi_stop(void);
spi_status_t spi_transmit(const uint8_t *data, uint16_t size);
""")
    (work / "main.c").write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "display.c"
#include "adbkal.c"
static uint32_t now;
static uint8_t speed;
static led_t leds;
static bool cs, started, fail_spi;
static unsigned packets, user_pre, user_process;
static uint8_t packet[FRAME_LEN];
led_t host_keyboard_led_state(void) { return leds; }
uint8_t get_current_wpm(void) { return speed; }
uint32_t timer_read32(void) { return now; }
bool wpm_keycode(uint16_t code) { return code >= 4 && code <= 29; }
bool pre_process_record_user(uint16_t code, keyrecord_t *record) { (void)code; (void)record; ++user_pre; return true; }
bool process_record_user(uint16_t code, keyrecord_t *record) { (void)code; (void)record; ++user_process; return true; }
void keyboard_post_init_user(void) {}
void housekeeping_task_user(void) {}
void wait_us(unsigned us) { (void)us; }
void setPinOutput(unsigned pin) { assert(pin == 1); }
void writePinLow(unsigned pin) { assert(pin == 1); cs = false; }
void writePinHigh(unsigned pin) { assert(pin == 1); cs = true; }
void spi_init(void) {}
bool spi_start(unsigned pin, bool lsb, unsigned spi_mode, unsigned divider) {
    assert(pin == NO_PIN && !lsb && spi_mode == 0 && divider == 128);
    assert(!started && !cs); started = true; return true;
}
void spi_stop(void) { assert(started && !cs); started = false; }
spi_status_t spi_transmit(const uint8_t *data, uint16_t size) {
    assert(cs && started && size == FRAME_LEN);
    memcpy(packet, data, size); ++packets;
    assert(data[0] == (0x80 | (vcom ? 0x40 : 0)));
    for (int y = 0; y < H; ++y) {
        size_t offset = 1 + y * 22;
        assert(data[offset] == reverse8(y + 1));
        for (int x = 0; x < 20; ++x)
            assert(data[offset + 1 + x] == reverse8((uint8_t)~fb[H - 1 - y][19 - x]));
        assert(data[offset + 21] == 0);
    }
    assert(data[FRAME_LEN - 1] == 0);
    return fail_spi ? -1 : 0;
}
static void dump(const char *name) {
    // Reconstruct the displayed pixels from the actual outgoing SPI transaction.
    // This tests wire order, not just the graphics framebuffer.
    FILE *file = fopen(name, "wb"); assert(file);
    fprintf(file, "P4\n160 68\n");
    for (int y = 0; y < H; ++y) for (int x = 0; x < 20; ++x)
        fputc((uint8_t)~packet[2 + y * 22 + x], file);
    fclose(file);
}
static void advance(uint32_t ms) { now += ms; adbk_display_task(); }
int main(void) {
    adbk_display_init();
    assert(mode == SCREEN_NORMAL && packets == 1 && !dirty);
    dump("normal-boot.pbm");
    unsigned previous = packets;
    advance(100); assert(packets == previous);
    advance(400); assert(packets == previous + 1); // periodic VCOM refresh
    for (int i = 0; i < 36; ++i) { now += 50; adbk_display_note_key(); }
    assert(adbk_display_wpm() == 86);
    speed = 86; leds.raw = 7; advance(100); dump("normal-locks-on.pbm");

    keyrecord_t power = {.event = {.key = {10,5}, .pressed = true}};
    assert(!pre_process_record_kb(0xa5, &power)); // legacy persisted KC_PWR
    assert(mode == SCREEN_CAT && user_pre == 0);
    assert(!pre_process_record_kb(0xa5, &power)); // duplicate down does not toggle
    assert(mode == SCREEN_CAT);
    power.event.pressed = false;
    assert(!pre_process_record_kb(0xa5, &power)); assert(mode == SCREEN_CAT);
    speed = 0; advance(1100); dump("cat-idle.pbm");
    keyrecord_t key = {.event = {.key = {4,0}, .pressed = true}};
    assert(pre_process_record_kb(4, &key)); assert(has_typed && user_pre == 1);
    for (int i = 0; i < 32; ++i) { now += 25; adbk_display_note_key(); }
    speed = 80; advance(100); animation_frame = 0; dirty = true; advance(50); dump("cat-tap-0.pbm");
    animation_frame = 1; dirty = true; advance(50); dump("cat-tap-1.pbm");

    power.event.pressed = true;
    assert(!pre_process_record_kb(0, &power)); assert(mode == SCREEN_APPLE);
    power.event.pressed = false; assert(!pre_process_record_kb(0, &power));
    advance(50); dump("apple-0.pbm");
    for (int i = 1; i <= 12; ++i) {
        advance(150);
        if (i == 4) dump("apple-4.pbm");
        if (i == 8) dump("apple-8.pbm");
        if (i == 12) dump("apple-12.pbm");
    }
    power.event.pressed = true;
    assert(!pre_process_record_kb(0xa5, &power)); assert(mode == SCREEN_NORMAL);
    power.event.pressed = false; assert(!pre_process_record_kb(0xa5, &power));
    assert(!process_record_kb(ADBK_SCREEN, &key)); assert(mode == SCREEN_CAT);
    key.event.pressed = false; assert(!process_record_kb(ADBK_SCREEN, &key));
    assert(process_record_kb(4, &key) && user_process == 1);

    adbk_display_init(); assert(mode == SCREEN_NORMAL); // always normal on boot
    for (int i = 0; i < 5; ++i) { now += 50; adbk_display_note_key(); }
    assert(adbk_display_wpm() == 12); // five quick taps cannot spike to 120
    advance(1000); assert(adbk_display_wpm() == 0);
    for (int i = 0; i < 25; ++i) { now += 200; adbk_display_note_key(); }
    assert(adbk_display_wpm() == 60); // five chars/sec = 60 words/minute
    for (int i = 0; i < 25; ++i) { now += 200; adbk_display_note_key(); }
    assert(adbk_display_wpm() == 60); // rolling bucket expiration stays stable
    advance(1000); assert(adbk_display_wpm() == 0);
    adbk_display_note_key(); assert(adbk_display_wpm() == 2); // fresh burst
    memset(fb, 0, sizeof(fb)); pixel(0,0,true);
    assert(send_frame());
    // Logical top-left black pixel becomes the final physical bottom-right bit.
    assert(packet[2] == 0xff && packet[FRAME_LEN - 3] == 0xfe);
    bool phase = vcom; fail_spi = true; dirty = true; advance(50);
    assert(dirty && vcom == phase); fail_spi = false; advance(50);
    assert(!dirty && vcom != phase);
    puts("PASS: boot default, mode/button handling, five-second WPM normalization (5 taps=12 WPM; sustained 5 chars/sec=60 WPM), one-second idle clear, WPM/locks, animation, 180-degree SPI output/VCOM and retry; ASan/UBSan clean");
    return 0;
}
''')
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-g",
                    "-fsanitize=address,undefined", '-DQMK_KEYBOARD_H="quantum.h"',
                    "-I", str(work), "-I", str(keyboard), str(work / "main.c"),
                    "-o", str(work / "test")], check=True)
    subprocess.run([str(work / "test")], cwd=output, check=True)

print("Previews:", output)
