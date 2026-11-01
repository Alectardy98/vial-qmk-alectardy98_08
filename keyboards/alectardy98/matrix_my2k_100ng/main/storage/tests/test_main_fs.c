/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_main_fs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SECTORS 8192u
static uint8_t  image[SECTORS * 512u];
static unsigned format;
static int      media_budget = -1;
static bool directory_cycle, volume_cycle;
static uint32_t gen    = 1, reads;
static bool     online = true, change_during_read;
static void     w16(uint8_t *p, unsigned v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void w32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t r32(uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool media(void *c, uint32_t *n, uint32_t *g) {
    (void)c;
    if (media_budget == 0) return false;
    if (media_budget > 0) media_budget--;
    *n = format == 32 ? 70000u : format == 12 ? 2048u : SECTORS;
    *g = gen;
    return online;
}
static bool read_sector(void *c, uint32_t lba, uint8_t out[512]) {
    (void)c;
    reads++;
    if (format == 32) {
        assert(lba < 70000);
        memset(out, 0, 512);
        if (lba == 0) {
            out[510] = 0x55;
            out[511] = 0xaa;
            out[450] = 0x0c;
            w32(out + 454, 1);
            w32(out + 458, 69999);
        } else if (lba == 1) {
            out[0] = 0xeb;
            out[1] = 0x58;
            out[2] = 0x90;
            w16(out + 11, 512);
            out[13] = 1;
            w16(out + 14, 32);
            out[16] = 1;
            out[21] = 0xf8;
            w32(out + 32, 69999);
            w32(out + 36, 600);
            w32(out + 44, 2);
            w16(out + 48, 1);
            out[66] = 0x29;
            memcpy(out + 82, "FAT32   ", 8);
            out[510] = 0x55;
            out[511] = 0xaa;
        } else if (lba == 33) {
            w32(out, 0x0ffffff8);
            w32(out + 4, 0x0fffffff);
            w32(out + 8, directory_cycle ? 2 : 0x0fffffff);
        } else if (lba == 633 && directory_cycle) {
            for (unsigned i = 0; i < 16; i++) {
                memcpy(out + 32 * i, "DIRECTRY   ", 11);
                out[32 * i + 11] = volume_cycle ? 0x08 : 0x10;
            }
        }
    } else {
        assert(lba < SECTORS);
        memcpy(out, image + lba * 512, 512);
    }
    if (change_during_read) {
        gen++;
        change_during_read = false;
    }
    return true;
}
static void fixture12(void) {
    memset(image, 0, sizeof image);
    uint8_t *b = image;
    b[0]       = 0xeb;
    b[1]       = 0x3c;
    b[2]       = 0x90;
    w16(b + 11, 512);
    b[13] = 1;
    w16(b + 14, 1);
    b[16] = 1;
    w16(b + 17, 512);
    w16(b + 19, 2048);
    b[21] = 0xf8;
    w16(b + 22, 6);
    b[38] = 0x29;
    memcpy(b + 54, "FAT12   ", 8);
    b[510]     = 0x55;
    b[511]     = 0xaa;
    image[512] = 0xf8;
    image[513] = image[514] = 0xff;
    image[512 + 511]        = 0xf0;
    image[512 + 512]        = 0xff; /* cluster341 crosses sectors */
}
static void fixture(void) {
    memset(image, 0, sizeof image);
    image[510]    = 0x55;
    image[511]    = 0xaa;
    uint8_t *part = image + 446;
    part[4]       = 6;
    w32(part + 8, 1);
    w32(part + 12, 8191);
    uint8_t *b = image + 512;
    b[0]       = 0xeb;
    b[1]       = 0x3c;
    b[2]       = 0x90;
    memcpy(b + 3, "MY2KTEST", 8);
    w16(b + 11, 512);
    b[13] = 1;
    w16(b + 14, 1);
    b[16] = 1;
    w16(b + 17, 512);
    w16(b + 19, 8191);
    b[21] = 0xf8;
    w16(b + 22, 32);
    w16(b + 24, 32);
    w16(b + 26, 64);
    w32(b + 28, 1);
    b[38] = 0x29;
    memcpy(b + 54, "FAT16   ", 8);
    b[510]       = 0x55;
    b[511]       = 0xaa;
    uint8_t *fat = image + 2 * 512;
    w16(fat, 0xfff8);
    w16(fat + 2, 0xffff);
    w16(fat + 4, 3);
    w16(fat + 6, 0xffff);
    uint8_t    *root    = image + 34 * 512;
    const char *names[] = {"HELLO   TXT", "SECOND  BIN", "THIRD   CRS", "FOURTH  ABW", "FIFTH   NES"};
    for (unsigned i = 0; i < 5; i++) {
        memcpy(root + i * 32, names[i], 11);
        root[i * 32 + 11] = 0x20;
    }
    w16(root + 26, 2);
    w32(root + 28, 700);
    memcpy(root + 5 * 32, "DIRECTORY  ", 11);
    root[5 * 32 + 11] = 0x10;
    memcpy(root + 6 * 32, "VOLUME     ", 11);
    root[6 * 32 + 11] = 0x08;
    for (unsigned i = 0; i < 700; i++)
        image[66 * 512 + i] = (uint8_t)(i ^ 0x5a);
}
static void packet(uint8_t *r, uint8_t command) {
    memset(r, 0, 32);
    r[0] = 0xfd;
    r[1] = command;
}
static void process(uint8_t *r, bool ok) {
    assert(my2k_main_fs_process(r, 32));
    assert(r[2] == (ok ? 0xaa : 0x55));
}
static uint8_t open_name(const char *name, uint8_t slot, bool ok) {
    uint8_t r[32];
    packet(r, 37);
    r[2] = slot;
    r[3] = 1;
    memcpy(r + 4, name, strlen(name) + 1);
    process(r, ok);
    return r[3];
}
int main(void) {
    fixture();
    my2k_main_fs_backend backend = {NULL, media, read_sector};
    my2k_main_fs_init(&backend);
    uint8_t r[32];
    packet(r, 35);
    process(r, true);
    assert(r[3] == 5 && r32(r + 4) == 8124u * 512 && r32(r + 8) == 8126u * 512);
    packet(r, 36);
    process(r, true);
    assert(!strcmp((char *)r + 3, "HELLO.TXT") && r32(r + 16) == 700);
    packet(r, 36);
    r[2] = 5;
    process(r, false);
    uint8_t handle = open_name("HELLO.TXT", 255, true);
    assert(handle == 0 && open_name("HELLO.TXT", 255, true) == handle);
    for (unsigned offset = 0; offset < 700; offset += 24) {
        packet(r, 39);
        r[2] = handle;
        r[3] = 24;
        w32(r + 4, offset);
        process(r, true);
        unsigned expected = 700 - offset;
        if (expected > 24) expected = 24;
        assert(r[3] == expected);
        for (unsigned i = 0; i < expected; i++)
            assert(r[8 + i] == (uint8_t)((offset + i) ^ 0x5a));
    }
    packet(r, 39);
    r[2] = handle;
    r[3] = 25;
    process(r, false);
    packet(r, 39);
    r[2] = handle;
    r[3] = 1;
    w32(r + 4, 701);
    process(r, false);
    packet(r, 39);
    r[2] = handle;
    r[3] = 24;
    w32(r + 4, 700);
    process(r, true);
    assert(!r[3]);
    assert(open_name("SECOND.BIN", 255, true) == 1);
    assert(open_name("THIRD.CRS", 255, true) == 2);
    assert(open_name("FOURTH.ABW", 255, true) == 3);
    open_name("FIFTH.NES", 255, false);
    open_name("../X", 255, false);
    open_name("A/B.TXT", 255, false);
    open_name("HELLO.TXT", 1, false);
    packet(r, 40);
    r[2] = handle;
    process(r, true);
    packet(r, 40);
    r[2] = handle;
    process(r, true);
    assert(open_name("FIFTH.NES", 255, true) == 0);
    uint32_t before = reads;
    packet(r, 37);
    r[3] = 0;
    memcpy(r + 4, "HELLO.TXT", 10);
    process(r, false);
    packet(r, 38);
    process(r, false);
    packet(r, 41);
    process(r, false);
    assert(reads == before);
    online = false;
    packet(r, 35);
    process(r, false);
    online = true;
    gen++;
    packet(r, 39);
    r[2] = 1;
    r[3] = 1;
    process(r, false);
    open_name("HELLO.TXT", 255, true);
    change_during_read = true;
    packet(r, 35);
    process(r, false);
    packet(r, 35);
    process(r, true);
    uint8_t other[32] = {0xfd, 0};
    uint8_t copy[32];
    memcpy(copy, other, 32);
    assert(!my2k_main_fs_process(other, 32) && !memcmp(copy, other, 32));
    assert(!my2k_main_fs_process(r, 31));
    format = 12;
    gen++;
    fixture12();
    packet(r, 35);
    process(r, true);
    assert(r[3] == 0 && r32(r + 4) == 2008u * 512 && r32(r + 8) == 2009u * 512);
    format = 32;
    gen++;
    packet(r, 35);
    process(r, true);
    assert(r[3] == 0 && r32(r + 4) == 69366u * 512 && r32(r + 8) == 69367u * 512);
    media_budget = 20;
    packet(r, 35);
    process(r, false);
    media_budget    = -1;
    directory_cycle = true;
    gen++;
    uint32_t prior = reads;
    packet(r, 35);
    process(r, false);
    assert(reads - prior < 1000);
    volume_cycle = true;
    gen++;
    prior = reads;
    packet(r, 35);
    process(r, false);
    assert(reads - prior < 1000);
    puts("PASS: actual readonly FatFs FAT12/16/32, allocation/cross-sector FAT12, GUI packets, handles, cancellation and bounded cyclic directory");
    return 0;
}
