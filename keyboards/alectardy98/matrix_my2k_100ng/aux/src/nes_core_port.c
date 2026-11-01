/* SPDX-License-Identifier: GPL-2.0-or-later */
/* MY2K Nofrendo platform boundary. Original CPU/PPU/APU and mapper source
 * notices remain in vendor/nofrendo; no ASTRO executable code is used. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <noftypes.h>
#include <nes.h>
#include <nesinput.h>
#include <nes6502.h>
#include <vid_drv.h>
#include <gui.h>
#include "my2k_nes_core.h"

enum { ARENA_BYTES = 32768, LINE_PITCH = 272 };
static union {
    uint64_t alignment;
    uint8_t  data[ARENA_BYTES];
} arena;
static unsigned used, fault;
static nes_t   *machine;
static uint8_t *image;
static size_t   image_bytes;
static uint8_t  cart_ram[8192], cart_vram[8192], scanline[LINE_PITCH];
static union {
    uint64_t alignment;
    uint8_t  bytes[sizeof(bitmap_t) + 240u * sizeof(uint8_t *)];
} bitmap_storage;
static uint16_t           palette[256], row_pixels[128];
static uint8_t            buttons, latched;
static unsigned           pad_bit;
static my2k_nes_line_sink sink;
static void (*yield_cpu)(void);
extern mapintf_t map0_intf, map1_intf, map2_intf, map3_intf, map4_intf, map7_intf, map40_intf, map66_intf;
mapintf_t       *mappers[] = {&map0_intf, &map1_intf,  &map2_intf,  &map3_intf, &map4_intf,
                              &map7_intf, &map40_intf, &map66_intf, NULL};
rgb_t            gui_pal[GUI_TOTALCOLORS];
extern void      my2k_nofrendo_step(bool draw);
extern void      my2k_nofrendo_timing_init(void);

void *_my_malloc(int size) {
    if (size <= 0 || (unsigned)size > ARENA_BYTES - used) {
        fault = 1;
        return NULL;
    }
    unsigned n = ((unsigned)size + 7u) & ~7u;
    if (n > ARENA_BYTES - used) {
        fault = 1;
        return NULL;
    }
    void *p = arena.data + used;
    used += n;
    memset(p, 0, n);
    return p;
}
/* Session arena: destruction clears pointers, then stop/load rewinds it. */
void _my_free(void **p) {
    if (p)
        *p = NULL;
}
char *_my_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char  *p = _my_malloc((int)n);
    if (p)
        memcpy(p, s, n);
    return p;
}
int log_printf(const char *format, ...) {
    (void)format;
    return 0;
}
int log_print(const char *s) {
    (void)s;
    return 0;
}
void gui_sendmsg(int color, char *format, ...) {
    (void)color;
    (void)format;
}
void gui_frame(bool draw) {
    (void)draw;
}
void gui_tick(int ticks) {
    (void)ticks;
}
void osd_getsoundinfo(sndinfo_t *s) {
    s->sample_rate = 8000;
    s->bps         = 16;
}
void osd_setsound(void (*p)(void *, int)) {
    (void)p;
}
void         osd_getinput(void) {}
void         vid_flush(void) {}
volatile int nofrendo_ticks;
bitmap_t    *vid_getbuffer(void) {
    return (bitmap_t *)(void *)bitmap_storage.bytes;
}
void vid_setpalette(rgb_t *p) {
    for (unsigned i = 0; i < 256; i++)
        palette[i] = (uint16_t)(((unsigned)p[i].r >> 3) << 11 | ((unsigned)p[i].g >> 2) << 5 | ((unsigned)p[i].b >> 3));
}
void input_strobe(void) {
    latched = buttons;
    if ((latched & 0x30) == 0x30)
        latched &= (uint8_t)~0x30;
    if ((latched & 0xc0) == 0xc0)
        latched &= (uint8_t)~0xc0;
    pad_bit = 0;
}
uint8 input_get(int type) {
    if (!(type & INP_JOYPAD0))
        return 0x40;
    unsigned value = pad_bit < 8 ? (latched >> pad_bit) & 1u : 1u;
    if (pad_bit < 8)
        pad_bit++;
    return (uint8)(0x40u | value);
}
void my2k_nes_scanline(int line, bool draw) {
    if (draw && line >= 0 && line < 240 && (line & 1)) {
        for (unsigned x = 0; x < 128; x++)
            row_pixels[x] = palette[scanline[8u + 2u * x] & 0x3fu];
        if (sink)
            sink((unsigned)line / 2u + 4u, row_pixels);
    }
    if (yield_cpu && (line & 3) == 0)
        yield_cpu();
}

/* Bounded PRG/CHR bank cache. Active mappings are pinned, so a bank switch
 * cannot invalidate another CPU/PPU pointer. No filesystem work per byte. */
static my2k_nes_rom_info   cart;
static my2k_nes_rom_reader reader;
static uint8_t            *workspace;
static uint8_t             saved_header[16];
static struct {
    uint32_t tag, stamp;
} pages[40];
static int8_t   prg_pins[5], chr_pins[8];
static unsigned cache_clock, cache_hits, cache_misses;
static uint8_t *cache_page(unsigned window, unsigned bank, int chr) {
    unsigned size = chr ? 1024u : 8192u, count = chr ? cart.chr_bytes / 1024u : cart.prg_bytes / 8192u;
    int8_t  *pins  = chr ? chr_pins : prg_pins;
    unsigned npins = chr ? 8u : 5u, first = chr ? 8u : 0u, last = chr ? 40u : 8u;
    if (window >= npins || !count) {
        fault = 11;
        return NULL;
    }
    bank %= count;
    if (!reader)
        return image + (chr ? cart.chr_offset : cart.prg_offset) + bank * size;
    int      choice = -1;
    unsigned oldest = ~0u;
    for (unsigned i = first; i < last; i++) {
        if (pages[i].stamp && pages[i].tag == bank) {
            choice = (int)i;
            cache_hits++;
            break;
        }
        int pinned = 0;
        for (unsigned j = 0; j < npins; j++)
            if (j != window && pins[j] == (int)i)
                pinned = 1;
        if (!pinned && pages[i].stamp < oldest) {
            oldest = pages[i].stamp;
            choice = (int)i;
        }
    }
    if (choice < 0) {
        fault = 11;
        return NULL;
    }
    uint8_t *dst = workspace + (choice < 8 ? (unsigned)choice * 8192u : 65536u + ((unsigned)choice - 8u) * 1024u);
    if (!pages[choice].stamp || pages[choice].tag != bank) {
        cache_misses++;
        uint32_t off = (chr ? cart.chr_offset : cart.prg_offset) + bank * size;
        if (!reader(off, dst, size)) {
            pages[choice].stamp = 0;
            fault               = 12;
            return NULL;
        }
        pages[choice].tag = bank;
    }
    /* Resetting this short session's LRU stamps on wrap is safe with pinning. */
    if (++cache_clock == 0) {
        for (unsigned i = 0; i < 40; i++)
            if (pages[i].stamp)
                pages[i].stamp = 1;
        cache_clock = 2;
    }
    pages[choice].stamp = cache_clock;
    pins[window]        = (int8_t)choice;
    return dst;
}
uint8_t *my2k_nes_prg_page(unsigned w, unsigned b) {
    return cache_page(w, b, 0);
}
uint8_t *my2k_nes_chr_page(unsigned w, unsigned b) {
    return cache_page(w, b, 1);
}
unsigned my2k_nes_cache_misses(void) {
    return cache_misses;
}
unsigned my2k_nes_cache_hits(void) {
    return cache_hits;
}
uint8_t *my2k_nes_battery_ram(void) {
    return machine && (cart.flags & 2u) ? cart_ram : NULL;
}
static int valid_image(void) {
    if (!image || image_bytes < 16)
        return 2;
    return my2k_nes_rom_parse(image, (uint32_t)image_bytes, &cart);
}
rominfo_t *rom_load(const char *name) {
    (void)name;
    if (valid_image())
        return NULL;
    rominfo_t *r = _my_malloc(sizeof(*r));
    if (!r)
        return NULL;
    r->rom           = reader ? NULL : image + cart.prg_offset;
    r->rom_banks     = (int)(cart.prg_bytes / 16384u);
    r->vrom_banks    = (int)(cart.chr_bytes / 8192u);
    r->vrom          = reader ? NULL : (cart.chr_bytes ? image + cart.chr_offset : NULL);
    r->sram          = cart_ram;
    r->sram_banks    = 1;
    r->vram          = cart.chr_bytes ? NULL : cart_vram;
    r->vram_banks    = cart.chr_bytes ? 0 : 1;
    r->mapper_number = cart.mapper;
    r->mirror        = cart.flags & 1 ? MIRROR_VERT : MIRROR_HORIZ;
    if (cart.flags & 2)
        r->flags |= ROM_FLAG_BATTERY;
    if (cart.flags & 4) {
        r->flags |= ROM_FLAG_TRAINER;
        if (reader) {
            if (!reader(16, cart_ram + 0x1000, 512)) {
                fault = 12;
                return NULL;
            }
        } else
            memcpy(cart_ram + 0x1000, image + 16, 512);
    }
    if (cart.flags & 8)
        r->flags |= ROM_FLAG_FOURSCREEN;
    return r;
}
void rom_free(rominfo_t **r) {
    if (r)
        *r = NULL;
}
void my2k_nes_core_callbacks(my2k_nes_line_sink s, void (*y)(void)) {
    sink      = s;
    yield_cpu = y;
}
static int start_machine(void) {
    int result = valid_image();
    if (result) {
        fault = result;
        return result;
    }
    memset(pages, 0, sizeof pages);
    memset(prg_pins, -1, sizeof prg_pins);
    memset(chr_pins, -1, sizeof chr_pins);
    cache_clock = cache_hits = cache_misses = 0;
    bitmap_t *b                             = vid_getbuffer();
    memset(&bitmap_storage, 0, sizeof bitmap_storage);
    memset(scanline, 0, sizeof scanline);
    b->width    = 256;
    b->height   = 240;
    b->pitch    = LINE_PITCH;
    b->hardware = true;
    b->data     = scanline;
    for (unsigned i = 0; i < 240; i++)
        b->line[i] = scanline + 8;
    memset(cart_ram, 0, sizeof cart_ram);
    memset(cart_vram, 0, sizeof cart_vram);
    buttons = latched = 0;
    pad_bit           = 0;
    machine           = nes_create();
    if (!machine) {
        fault = 7;
        return 7;
    }
    if (nes_insertcart("MY2K RAM ROM", machine)) {
        machine = NULL;
        fault   = 8;
        return 8;
    }
    my2k_nofrendo_timing_init();
    return fault ? fault : 0;
}
int my2k_nes_core_load(uint8_t *rom, size_t n) {
    my2k_nes_core_stop();
    if (n > MY2K_NES_ROM_MAX)
        return 5;
    image       = rom;
    image_bytes = n;
    return start_machine();
}
int my2k_nes_core_load_stream(const uint8_t h[16], uint32_t n, uint8_t *buffer, size_t capacity,
                              my2k_nes_rom_reader read) {
    my2k_nes_core_stop();
    if (!h || !buffer || capacity < 96u * 1024u || !read)
        return 2;
    memcpy(saved_header, h, 16);
    image       = saved_header;
    image_bytes = n;
    workspace   = buffer;
    reader      = read;
    return start_machine();
}
void my2k_nes_core_step(int draw) {
    if (machine && !fault) {
        my2k_nofrendo_step(draw != 0);
        nes6502_context cpu;
        nes6502_getcontext(&cpu);
        if (cpu.jammed)
            fault = 14;
    }
}
void my2k_nes_core_stop(void) {
    if (machine)
        nes_destroy(&machine);
    machine = NULL;
    used = fault = 0;
    image        = NULL;
    image_bytes  = 0;
    reader       = NULL;
    workspace    = NULL;
}
void my2k_nes_core_buttons(uint8_t b) {
    buttons = b;
}
unsigned my2k_nes_core_heap_used(void) {
    return used;
}
unsigned my2k_nes_core_fault(void) {
    return fault;
}
