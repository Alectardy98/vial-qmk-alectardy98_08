/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_link.h"
#include "hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now, origin;
static unsigned starts, changes, sleeps;
SerialDriver    SD1;
uint32_t        chVTGetSystemTimeX(void) {
    return now;
}
void chThdSleepMilliseconds(unsigned n) {
    assert(n == 1);
    now += n;
    sleeps++;
    assert(sleeps <= 13);
}
void sdStart(SerialDriver *s, const SerialConfig *c) {
    assert(s == &SD1 && c->speed == 115200);
    starts++;
}
void palSetPadMode(unsigned p, unsigned pin, unsigned mode) {
    if (p == GPIOB && pin == 4) {
        assert((uint32_t)(now - origin) == (mode == PAL_MODE_INPUT ? 13 : 11));
        changes++;
    }
}
void palWritePad(unsigned p, unsigned pin, unsigned high) {
    if (p == GPIOA)
        assert(pin == 8 && high && (uint32_t)(now - origin) == 0);
    else {
        assert(p == GPIOB && pin == 4 && !high && (uint32_t)(now - origin) == 11);
        changes++;
    }
}
msg_t sdGetTimeout(SerialDriver *d, unsigned t) {
    (void)d;
    (void)t;
    return -1;
}
msg_t sdPutTimeout(SerialDriver *d, uint8_t b, unsigned t) {
    (void)d;
    (void)b;
    (void)t;
    return MSG_OK;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    origin = now = !strcmp(argv[1], "wrap") ? UINT32_MAX - 5u : 0;
    my2k_aux_prepare();
    assert(starts == 1 && changes == 3 && sleeps == 13 && my2k_aux_startup_ready());
    uint8_t r[32] = {0};
    my2k_aux_startup_status(r);
    uint32_t released = (uint32_t)r[16] | (uint32_t)r[17] << 8 | (uint32_t)r[18] << 16 | (uint32_t)r[19] << 24;
    assert(r[11] == 1 && released == now);
    unsigned before = changes;
    /* QMK rebases its clock after pre-init. No reset may be replayed. */
    my2k_aux_service(0);
    my2k_aux_service(1);
    my2k_aux_service(5000);
    my2k_aux_prepare();
    assert(changes == before && starts == 1 && sleeps == 13);
    puts("PASS early AUX reset order/timing/wrap, one-shot ownership and QMK timer rebase");
}
