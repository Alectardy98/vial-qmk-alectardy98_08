// ADBK nice!view LS011B7DH03 landscape (160x68), write-only Sharp Memory LCD.
// The panel uses ACTIVE-HIGH chip select, unlike conventional SPI devices.
// Hardware SPI0 on GP2/GP3; GP1 SCS is controlled separately (active high).
#include "quantum.h"
#include "gpio.h"
#include "spi_master.h"
#include "timer.h"
#include "display.h"
#include <string.h>

#define W 160
#define H 68
#define BYTES_PER_ROW (W / 8)
#define CS ADBK_DISPLAY_CS_PIN
#define FRAME_LEN (1 + H * (1 + BYTES_PER_ROW + 1) + 1)
static uint8_t spi_frame[FRAME_LEN];
#define PERIOD_MS 250

static uint8_t fb[H][BYTES_PER_ROW];
static bool vcom;
static uint32_t last_frame;
static uint8_t history[56];
static uint8_t history_pos;

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

static void circle(int cx, int cy, int r) {
    int x=r, y=0, err=0;
    while (x>=y) {
        pixel(cx+x,cy+y,true);pixel(cx+y,cy+x,true);pixel(cx-y,cy+x,true);pixel(cx-x,cy+y,true);
        pixel(cx-x,cy-y,true);pixel(cx-y,cy-x,true);pixel(cx+y,cy-x,true);pixel(cx+x,cy-y,true);
        ++y; if (err<=0) err += 2*y+1; else {--x;err+=2*(y-x)+1;}
    }
}

static void draw_ui(void) {
    memset(fb,0,sizeof(fb));
    const uint8_t layer = get_highest_layer(layer_state | default_layer_state);
    const bool caps = host_keyboard_led_state().caps_lock;
    const uint8_t wpm = get_current_wpm();

    str(3,3,"ADBK",2);
    str(87,3,"USB",2);
    line(0,17,159,17);
    str(3,22,"WPM",1);
    number(3,31,wpm,3);
    str(3,52,"CAPS",1);
    str(27,52,caps ? "ON" : "OFF",1);
    line(66,19,66,66);
    str(75,22,"LAYER",1);
    for (int i=0;i<3;++i) {
        int cx=91+i*24;
        circle(cx,44,10);
        number(cx-3,41,(uint16_t)i,1);
        if (layer == i) { line(cx-6,57,cx+6,57); line(cx-6,58,cx+6,58); }
    }
    // Thin WPM history sparkline across the bottom.
    for (int i=1;i<56;++i) {
        int a=history[(history_pos+i-1)%56];
        int b=history[(history_pos+i)%56];
        line(103+i,66-a/20,103+i+1,66-b/20);
    }
}

static void send_frame(void) {
    // Build the complete Sharp Memory LCD write transaction before asserting SCS.
    // SPI mode 0, MSB-first. The on-wire Sharp command is LSB-first;
    // 0x80 is WRITE and 0x40 is VCOM after bit reversal.
    size_t pos = 0;
    spi_frame[pos++] = (uint8_t)(0x80 | (vcom ? 0x40 : 0));
    for (uint8_t y = 0; y < H; ++y) {
        spi_frame[pos++] = reverse8((uint8_t)(y + 1));
        for (uint8_t x = 0; x < BYTES_PER_ROW; ++x) {
            // Sharp transmits the leftmost pixel first (LSB-first on the wire).
            // QMK SPI uses MSB-first, so reverse each framebuffer byte.
            spi_frame[pos++] = reverse8(fb[y][x]);
        }
        spi_frame[pos++] = 0;
    }
    spi_frame[pos++] = 0;

    // QMK normally asserts slave-select LOW. Use NO_PIN so it does not touch
    // the Sharp panel's active-HIGH SCS, which we drive as ordinary GPIO.
    if (!spi_start(NO_PIN, false, 0, 128)) return;
    writePinHigh(CS);
    wait_us(3);
    spi_status_t result = spi_transmit(spi_frame, (uint16_t)pos);
    wait_us(3);
    writePinLow(CS);
    spi_stop();
    if (result == SPI_STATUS_SUCCESS) vcom = !vcom;
}

void adbk_display_init(void) {
    setPinOutput(CS);
    writePinLow(CS);
    spi_init();
    memset(history, 0, sizeof(history));
    last_frame = timer_read32();
    draw_ui();
    send_frame();
}

void adbk_display_task(void) {
    if (timer_elapsed32(last_frame) < PERIOD_MS) return;
    last_frame = timer_read32();
    history[history_pos] = get_current_wpm();
    history_pos = (uint8_t)((history_pos+1)%56);
    draw_ui();
    send_frame();
}
