// Landscape 160x68 nice!view status and animation screens.
// The framebuffer is MSB-left with 1=black; send_frame converts its polarity.
#include "quantum.h"
#include "gpio.h"
#include "spi_master.h"
#include "timer.h"
#include "display.h"
#include "display_assets.h"
#include <string.h>

#define W 160
#define H 68
#define BYTES_PER_ROW (W / 8)
#define CS ADBK_DISPLAY_CS_PIN
#define FRAME_LEN (1 + H * (1 + BYTES_PER_ROW + 1) + 1)
#define TASK_MS 50
#define REFRESH_MS 500

static uint8_t spi_frame[FRAME_LEN];
static uint8_t fb[H][BYTES_PER_ROW];
static bool vcom;

enum screen_mode { SCREEN_NORMAL, SCREEN_CAT, SCREEN_APPLE, SCREEN_COUNT };
static enum screen_mode mode;
static bool dirty;
static uint32_t last_task, last_frame, last_animation, last_key;
static bool has_typed;
static uint8_t animation_frame, shown_wpm, shown_locks;

// Twenty 250-ms buckets cover five seconds. Always normalize to the full
// window, including at the beginning of a typing burst: a few quick taps must
// not look like sustained 120-WPM typing. The independent idle timeout lets
// both the number and the cat stop promptly without shortening that window.
#define TYPING_BUCKETS 20
#define TYPING_BUCKET_MS 250
#define TYPING_IDLE_MS 1000
static uint8_t typing_buckets[TYPING_BUCKETS], typing_bucket;
static uint16_t typing_total;
static uint32_t typing_epoch;

static void reset_typing(uint32_t now) {
    memset(typing_buckets, 0, sizeof(typing_buckets));
    typing_bucket = 0;
    typing_total = 0;
    typing_epoch = now;
}

static void advance_typing(uint32_t now) {
    uint32_t steps = (now - typing_epoch) / TYPING_BUCKET_MS;
    if (steps >= TYPING_BUCKETS) {
        reset_typing(now);
        return;
    }
    while (steps--) {
        typing_bucket = (typing_bucket + 1) % TYPING_BUCKETS;
        typing_total -= typing_buckets[typing_bucket];
        typing_buckets[typing_bucket] = 0;
        typing_epoch += TYPING_BUCKET_MS;
    }
}

uint8_t adbk_display_wpm(void) {
    uint32_t now = timer_read32();
    if (!has_typed || now - last_key >= TYPING_IDLE_MS) {
        reset_typing(now);
        has_typed = false;
        return 0;
    }
    advance_typing(now);
    // 60 seconds / (five-second window * five characters per word).
    uint32_t value = (uint32_t)typing_total * 12 / 5;
    return value > 240 ? 240 : (uint8_t)value;
}

static uint8_t reverse8(uint8_t x) {
    x = (uint8_t)(((x & 0x55) << 1) | ((x & 0xAA) >> 1));
    x = (uint8_t)(((x & 0x33) << 2) | ((x & 0xCC) >> 2));
    return (uint8_t)((x << 4) | (x >> 4));
}

static void pixel(int x, int y, bool black) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    const uint8_t mask = (uint8_t)(0x80u >> (x & 7));
    if (black) fb[y][x >> 3] |= mask;
    else fb[y][x >> 3] &= (uint8_t)~mask;
}

static void line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        pixel(x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// Compact 3x5 font for the status display. Bit 2 is the leftmost pixel.
static const uint8_t *glyph(char c) {
    static const uint8_t digits[10][5] = {
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
        {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}
    };
    static const uint8_t letters[26][5] = {
        {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},{7,4,6,4,7},
        {7,4,6,4,4},{3,4,5,5,3},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,2},
        {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{2,5,5,5,2},
        {6,5,6,4,4},{2,5,5,7,3},{6,5,6,5,5},{3,4,2,1,6},{7,2,2,2,2},
        {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7}
    };
    static const uint8_t space[5] = {0,0,0,0,0};
    static const uint8_t dash[5] = {0,0,7,0,0};
    if (c >= '0' && c <= '9') return digits[c-'0'];
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return letters[c-'A'];
    if (c == '-') return dash;
    return space;
}

static void str(int x, int y, const char *s, int scale) {
    // Keep the fine background dither out of glyph counters and spacing.
    // The reflective panel's uninked areas provide the light text backdrop.
    int width = ((int)strlen(s) * 4 - 1) * scale;
    for (int yy = y - 1; yy <= y + 5 * scale; ++yy)
        for (int xx = x - 1; xx <= x + width; ++xx) pixel(xx, yy, false);
    for (; *s; ++s, x += 4*scale) {
        const uint8_t *g = glyph(*s);
        for (int row=0; row<5; ++row) for (int col=0; col<3; ++col)
            if (g[row] & (1 << (2-col)))
                for (int yy=0; yy<scale; ++yy) for (int xx=0; xx<scale; ++xx)
                    pixel(x+col*scale+xx,y+row*scale+yy,true);
    }
}

static void number(int x, int y, uint16_t n, int scale) {
    char s[6]; uint8_t i=0;
    if (n > 9999) n=9999;
    do { s[i++] = (char)('0' + n%10); n/=10; } while(n && i<5);
    for (uint8_t j=0; j<i/2; ++j) {char t=s[j]; s[j]=s[i-j-1]; s[i-j-1]=t;}
    s[i]=0; str(x,y,s,scale);
}

// A fixed 4x4 dither gives the monochrome panel a light gray paper background.
// It stays fixed between frames, so static areas do not shimmer.
static bool paper_black(int x, int y) {
    return ((y & 3) == 0 && (x & 3) == 0) || ((y & 3) == 2 && (x & 3) == 2);
}

static void clear_paper(void) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < BYTES_PER_ROW; ++x)
            fb[y][x] = (y & 3) == 0 ? 0x88 : (y & 3) == 2 ? 0x22 : 0;
}

static void box(int x, int y, int width, int height, bool filled) {
    for (int yy = y; yy < y + height; ++yy)
        for (int xx = x; xx < x + width; ++xx)
            if (filled || xx == x || xx == x + width - 1 || yy == y || yy == y + height - 1)
                pixel(xx, yy, true);
}

static void centered_number(int left, int width, int y, uint8_t value, int scale) {
    int digits = value >= 100 ? 3 : value >= 10 ? 2 : 1;
    number(left + (width - (digits * 4 - 1) * scale) / 2, y, value, scale);
}

static void lock_indicator(int y, const char *label, bool on) {
    box(91, y, 66, 18, false);
    str(96, y + 4, label, 2);
    box(148, y + 7, 5, 5, on);
}

static void draw_normal(uint8_t wpm, led_t leds) {
    str(28, 8, "WPM", 2);
    centered_number(4, 80, 29, wpm, 5);
    line(86, 6, 86, 61);
    lock_indicator(3, "CAPS", leds.caps_lock);
    lock_indicator(25, "NUM", leds.num_lock);
    lock_indicator(47, "SCROLL", leds.scroll_lock);
}

static void draw_cat(uint8_t wpm, uint32_t now) {
    str(7, 7, "BONGO CAT", 1);
    str(116, 8, "WPM", 2);
    centered_number(106, 50, 27, wpm, 4);
    bool typing = wpm >= 20 || (has_typed && now - last_key < 500);
    const uint8_t *frame = typing ? bongo_tap[animation_frame % 2]
                                  : wpm > 10 ? bongo_prep[0] : bongo_idle[animation_frame % 5];
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 96; ++x)
            pixel(x + 4, y + 24, frame[y * 12 + x / 8] & (0x80u >> (x & 7)));
    // A small speed bar complements the large WPM number.
    box(7, 61, 146, 4, false);
    uint16_t fill = wpm >= 160 ? 142 : (uint16_t)wpm * 142 / 160;
    if (fill) box(9, 62, fill, 2, true);
}

static void draw_apple(void) {
    static const int8_t bob[16] = {0,0,1,1,2,2,1,1,0,0,-1,-1,-2,-2,-1,-1};
    int top = 7 + bob[animation_frame % 16];
    int shine = (animation_frame % 16) * 4 - 8;
    for (int y = 0; y < 48; ++y) {
        for (int x = 0; x < 48; ++x) {
            if (!(apple_logo[0][y * 6 + x / 8] & (0x80u >> (x & 7)))) continue;
            // A gray highlight travels across the floating black silhouette.
            bool highlight = y - x / 4 >= shine && y - x / 4 < shine + 3;
            pixel(x + 56, y + top, highlight ? paper_black(x + 56, y + top) : true);
        }
    }
    line(10, 34, 41, 34);
    line(118, 34, 149, 34);
    str(71, 60, "APPLE", 1);
}

static void draw_ui(uint32_t now) {
    clear_paper();
    switch (mode) {
        case SCREEN_NORMAL: draw_normal(adbk_display_wpm(), host_keyboard_led_state()); break;
        case SCREEN_CAT: draw_cat(adbk_display_wpm(), now); break;
        case SCREEN_APPLE: draw_apple(); break;
        default: break;
    }
}

static bool send_frame(void) {
    size_t pos = 0;
    // Command and row addresses are numeric LSB-first fields. The pixel bytes
    // are already arranged in transmission order (left pixel in bit 7).
    spi_frame[pos++] = (uint8_t)(0x80 | (vcom ? 0x40 : 0));
    for (uint8_t y = 0; y < H; ++y) {
        spi_frame[pos++] = reverse8((uint8_t)(y + 1));
        for (uint8_t x = 0; x < BYTES_PER_ROW; ++x) {
            // This panel is mounted upside down. Rotate the complete image
            // 180 degrees: reverse rows, byte order, and bits within each byte.
            // Complement black=1 drawing pixels to the panel's white=1 format.
            spi_frame[pos++] = reverse8((uint8_t)~fb[H - 1 - y][BYTES_PER_ROW - 1 - x]);
        }
        spi_frame[pos++] = 0;
    }
    spi_frame[pos++] = 0;

    if (!spi_start(NO_PIN, false, 0, 128)) return false;
    writePinHigh(CS);
    wait_us(3);
    spi_status_t result = spi_transmit(spi_frame, (uint16_t)pos);
    wait_us(3);
    writePinLow(CS);
    spi_stop();
    if (result != SPI_STATUS_SUCCESS) return false;
    vcom = !vcom;
    return true;
}

void adbk_display_init(void) {
    setPinOutput(CS);
    writePinLow(CS);
    spi_init();
    mode = SCREEN_NORMAL;
    animation_frame = 0;
    has_typed = false;
    vcom = false;
    last_task = last_animation = timer_read32();
    reset_typing(last_task);
    shown_wpm = 0;
    shown_locks = host_keyboard_led_state().raw;
    draw_ui(last_task);
    dirty = !send_frame();
    last_frame = last_task;
}

void adbk_display_next_mode(void) {
    mode = (mode + 1) % SCREEN_COUNT;
    animation_frame = 0;
    last_animation = timer_read32();
    dirty = true;
}

void adbk_display_note_key(void) {
    uint32_t now = timer_read32();
    if (!has_typed || now - last_key >= TYPING_IDLE_MS) reset_typing(now);
    advance_typing(now);
    if (typing_buckets[typing_bucket] < UINT8_MAX) {
        ++typing_buckets[typing_bucket];
        ++typing_total;
    }
    last_key = now;
    has_typed = true;
    if (mode == SCREEN_CAT) dirty = true;
}

void adbk_display_task(void) {
    uint32_t now = timer_read32();
    if (now - last_task < TASK_MS) return;
    last_task = now;
    uint8_t wpm = adbk_display_wpm();
    uint8_t locks = host_keyboard_led_state().raw;
    if (mode != SCREEN_APPLE && (wpm != shown_wpm || locks != shown_locks)) dirty = true;
    shown_wpm = wpm;
    shown_locks = locks;
    if (mode != SCREEN_NORMAL) {
        uint16_t interval = mode == SCREEN_APPLE ? 150 : wpm > 20 ? (wpm >= 140 ? 100 : 450 - wpm * 5 / 2) : 300;
        if (now - last_animation >= interval) {
            last_animation = now;
            animation_frame = (animation_frame + 1) % 80; // multiple of all animation lengths
            dirty = true;
        }
    }
    if (!dirty && now - last_frame < REFRESH_MS) return;
    draw_ui(now);
    if (send_frame()) {
        dirty = false;
        last_frame = now;
    }
}
