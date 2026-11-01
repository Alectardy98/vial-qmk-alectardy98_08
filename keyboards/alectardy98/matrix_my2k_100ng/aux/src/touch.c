/* SPDX-License-Identifier: GPL-2.0-or-later */
/* AUX touch input over I2C2. Polls sample registers without changing the
 * sensor's saved configuration. Recovered wiring: PB10/PB11 AF4, open drain.
 * Peripheral timing uses the recovered TIMINGR value 0x6000030d. */
#include "aux_platform.h"
#include "aux_work.h"
#define R(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define I2C 0x40005800u
#define RCC 0x40023800u
#define GPIOB 0x40020400u
#define ERR ((1u << 4) | (1u << 8) | (1u << 9) | (1u << 10) | (1u << 12))
static bool wait_flag(uint32_t mask) {
    uint32_t start = aux_millis();
    unsigned polls = 0;
    do {
        aux_work_checkpoint();
        uint32_t s = R(I2C + 0x18);
        if (s & ERR)
            return false;
        if (s & mask)
            return true;
    } while (++polls < 200000u && (uint32_t)(aux_millis() - start) < 10u);
    return false;
}
void aux_touch_init(void) {
    R(RCC + 0x30) |= 2u;
    R(RCC + 0x40) |= 1u << 22;
    (void)R(RCC + 0x40);
    R(GPIOB) = (R(GPIOB) & ~(15u << 20)) | (10u << 20);
    R(GPIOB + 4) |= 3u << 10;
    R(GPIOB + 8) |= 15u << 20;
    R(GPIOB + 12) &= ~(15u << 20);
    R(GPIOB + 0x24) = (R(GPIOB + 0x24) & ~(255u << 8)) | (0x44u << 8);
    R(I2C)          = 0;
    R(I2C + 0x10)   = 0x6000030du;
    R(I2C)          = 1u;
}
bool aux_touch_read(uint8_t channels[8]) {
    if (!channels || (R(I2C + 0x18) & (1u << 15)))
        return false;
    R(I2C + 0x1c) = 0x3f38u;
    /* Write ONLY the readable-register address pointer, then repeated-start read.
     * No data is written to a sensor setting, CTRL_CMD, CRC, save or reset. */
    R(I2C + 4) = 0x6eu | (1u << 16) | (1u << 13);
    if (!wait_flag(1u << 1))
        goto fail;
    R(I2C + 0x28) = 0xbau;
    if (!wait_flag(1u << 6))
        goto fail;
    R(I2C + 4) = 0x6eu | (16u << 16) | (1u << 10) | (1u << 13) | (1u << 25);
    for (unsigned i = 0; i < 16; i++) {
        if (!wait_flag(1u << 2))
            goto fail;
        uint8_t value = (uint8_t)R(I2C + 0x24);
        if (!(i & 1u))
            channels[i / 2] = value;
    }
    if (!wait_flag(1u << 5))
        goto fail;
    R(I2C + 0x1c) = 1u << 5;
    return true;
fail:
    R(I2C + 4) |= 1u << 14; /* STOP only; no GPIO bit-bang recovery or sensor reset. */
    R(I2C + 0x1c) = 0x3f38u;
    return false;
}
