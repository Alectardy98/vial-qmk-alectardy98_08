/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_qmk_rgb.h"
#include "my2k_rgb_i2c.h"
#include <string.h>
#if MY2K_DEV17_PROFILE
#    include "main_rgb_observer.h"
#endif
#if MY2K_DEV4_PROFILE
#    include "my2k_rgb_refresh.h"
static uint8_t last_frame[MY2K_RGB_COUNT * 3], previous_test, previous_color;
static bool    have_frame, visual_blank, previous_blank;
void           my2k_qmk_rgb_visual_blank(bool blank) {
    visual_blank = blank;
}
#endif
#if MY2K_DEV2_PROFILE
#    include "main_rgb_bus.h"
#endif
static bool                 batch_error;
static my2k_rgb_diagnostics diagnostic;
#if MY2K_DEV3_PROFILE
static my2k_rgb_controller_diagnostics controller[6];
static uint8_t                         batch_faults, selected_page[6], test_group, test_color;
static uint32_t                        test_deadline;
#    if !MY2K_DEV4_PROFILE
static uint32_t last_readback;
#    endif
static int driver_index(uint8_t address) {
    for (unsigned i = 0; i < 6; i++)
        if (my2k_rgb_drivers[i].address_byte == address)
            return (int)i;
    return -1;
}
my2k_rgb_controller_diagnostics my2k_qmk_rgb_controller(unsigned i) {
    return i < 6 ? controller[i] : (my2k_rgb_controller_diagnostics){0};
}
bool my2k_qmk_rgb_test_start(uint8_t group, uint8_t color, uint16_t duration, uint32_t now) {
    if (!group || (group & ~7u) || color > 3 || duration < 100 || duration > 3000)
        return false;
    test_group    = group;
    test_color    = color;
    test_deadline = now + duration;
    return true;
}
void my2k_qmk_rgb_test_cancel(void) {
    test_group = 0;
}
uint8_t my2k_qmk_rgb_test_group(void) {
    return test_group;
}
#else
my2k_rgb_controller_diagnostics my2k_qmk_rgb_controller(unsigned i) {
    (void)i;
    return (my2k_rgb_controller_diagnostics){0};
}
bool my2k_qmk_rgb_test_start(uint8_t g, uint8_t c, uint16_t t, uint32_t n) {
    (void)g;
    (void)c;
    (void)t;
    (void)n;
    return false;
}
void    my2k_qmk_rgb_test_cancel(void) {}
uint8_t my2k_qmk_rgb_test_group(void) {
    return 0;
}
#endif
#if MY2K_DEV2_PROFILE && !MY2K_DEV3_PROFILE
static uint32_t test_started;
#endif
my2k_rgb_diagnostics my2k_qmk_rgb_diagnostics(void) {
    return diagnostic;
}
#if MY2K_DEV_PROFILE
static uint8_t  present_mask, family_mask;
static uint32_t errors, last_retry;
#else
static bool     initialized, faulted, retry_wait;
static uint32_t last_retry, retry_delay = 100;
#endif
i2c_status_t my2k_rgb_i2c_write_register(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t size,
                                         uint16_t timeout) {
#if MY2K_DEV3_PROFILE
    int index = driver_index(address);
    if (index < 0 || (batch_faults & (1u << index)))
        return I2C_STATUS_ERROR;
    uint8_t replacement;
    /*3236 Rev.H requires UPDATE=00, not QMK's01. SL=11 selects Imax/4. */
    if (address == 0x78 && size == 1 && (reg == 0x25 || (reg >= 0x26 && reg <= 0x49))) {
        replacement = reg == 0x25 ? 0 : (uint8_t)((data[0] & 1) ? 7 : 0);
        data        = &replacement;
    }
    controller[index].writes++;
    controller[index].last_register = reg;
#else
    if (batch_error)
        return I2C_STATUS_ERROR;
#endif
    diagnostic.writes++;
    diagnostic.last_address  = address;
    diagnostic.last_register = reg;
#if MY2K_DEV2_PROFILE
    i2c_status_t result = my2k_rgb_bus_write(address, reg, data, size, timeout);
#else
    i2c_status_t result = i2c_write_register(address, reg, data, size, timeout);
#endif
    diagnostic.last_status = result;
#if MY2K_DEV3_PROFILE
    my2k_rgb_controller_diagnostics *d = &controller[index];
    d->last_status                     = result;
    if (result == I2C_STATUS_SUCCESS) {
        d->acknowledged++;
        if (my2k_rgb_drivers[index].type == 3 && reg == 0xfd && size == 1)
            selected_page[index] = data[0];
        uint8_t type = my2k_rgb_drivers[index].type;
        if ((type == 8 && reg >= 1 && (unsigned)reg + size <= 0x90) ||
            (type == 3 && selected_page[index] < 8 && reg >= 0x24 && (unsigned)reg + size <= 0xb4) ||
            (type == 5 && reg >= 1 && (unsigned)reg + size <= 0x25))
            d->pwm_updates++;
        if (type == 5 && size == 1) {
            if (reg == 0)
                d->post[0] = data[0];
            if (reg == 0x4a)
                d->post[1] = data[0];
            if (reg == 0x25)
                d->post[2] = data[0];
            if (reg == 0x44)
                d->post[3] = data[0];
        }
    } else {
        if (result == I2C_STATUS_TIMEOUT)
            d->timeouts++;
        else
            d->nacks++;
        batch_faults |= 1u << index;
    }
#endif
    if (result == I2C_STATUS_SUCCESS) {
        diagnostic.acknowledged++;
#if MY2K_DEV_PROFILE
        for (unsigned i = 0; i < 6; i++)
            if (address == my2k_rgb_drivers[i].address_byte)
                present_mask |= 1u << i;
#endif
    }
    if (result != I2C_STATUS_SUCCESS) {
        batch_error = true;
#if MY2K_DEV_PROFILE
        errors++;
#else
        initialized = false;
        faulted     = true;
#endif
    }
    return result;
}
#define FAMILY_API(part)                                                             \
    void my2k_qmk_rgb_##part##_init(void);                                           \
    void my2k_qmk_rgb_##part##_set(unsigned index, uint8_t r, uint8_t g, uint8_t b); \
    void my2k_qmk_rgb_##part##_flush(void)
FAMILY_API(3729);
FAMILY_API(3731);
FAMILY_API(3236);
#if MY2K_DEV_PROFILE
/* Families are isolated: one missing chip must not suppress the other types.
 * Existing family adapters initialize their known members together. */
uint8_t my2k_qmk_rgb_present(void) {
    return present_mask;
}
uint8_t my2k_qmk_rgb_families(void) {
    return family_mask;
}
uint32_t my2k_qmk_rgb_errors(void) {
    return errors;
}
static void initialize_family(unsigned bit) {
    batch_error = false;
#    if MY2K_DEV3_PROFILE
    batch_faults = 0;
#    endif
    if (bit == 1)
        my2k_qmk_rgb_3729_init();
    else if (bit == 2)
        my2k_qmk_rgb_3731_init();
    else
        my2k_qmk_rgb_3236_init();
#    if MY2K_DEV3_PROFILE
    uint8_t mask = bit == 1 ? 3 : bit == 2 ? 28 : 32;
    if (mask & ~batch_faults)
        family_mask |= bit;
    for (unsigned i = 0; i < 6; i++)
        if (mask & (1u << i))
            controller[i].initialized = !(batch_faults & (1u << i));
#    else
    if (!batch_error)
        family_mask |= bit;
#    endif
}
#    if MY2K_DEV3_PROFILE
static void readback(unsigned i, bool before) {
    my2k_rgb_controller_diagnostics *d       = &controller[i];
    uint8_t                         *values  = before ? d->pre : d->post;
    uint8_t                          address = my2k_rgb_drivers[i].address_byte, type = my2k_rgb_drivers[i].type;
    if (type == 5) {
        d->read_status = 1;
        return;
    } /* write-only: post fields are write shadows */
    uint8_t      bytes[13];
    i2c_status_t status;
    if (type == 3) {
        uint8_t bank = 0x0b;
        batch_faults = 0;
        status       = my2k_rgb_i2c_write_register(address, 0xfd, &bank, 1, 5);
#        if MY2K_DEV19_PROFILE
        /* FN04 is undefined; no burst across it or volatile FN07. Frame/PWM
         * reads while operating are unsupported (IS31FL3731 DS p7, AN Q6/7). */
        const uint8_t regs[4] = {0, 10, 9, 1};
        for (unsigned j = 0; j < 4 && !status; j++)
            status = my2k_rgb_bus_read(address, regs[j], values + j, 1, 5);
#        else
        if (!status)
            status = my2k_rgb_bus_read(address, 0, bytes, 13, 5);
        if (!status) {
            values[0] = bytes[0];
            values[1] = bytes[10];
            values[2] = bytes[9];
            values[3] = bytes[1];
        }
#        endif
        bank         = 0;
        batch_faults = 0;
        (void)my2k_rgb_i2c_write_register(address, 0xfd, &bank, 1, 5);
    } else {
        status = my2k_rgb_bus_read(address, 0xa0, bytes, 2, 5);
        if (!status) {
            values[0] = bytes[0];
            values[1] = bytes[1];
            status    = my2k_rgb_bus_read(address, 0x90, bytes, 1, 5);
            values[2] = bytes[0];
        }
    }
    if (!before && !status
#        if MY2K_DEV19_PROFILE
        && type != 3
#        endif
    ) {
        for (unsigned led = 0; led < 128; led++)
            if (my2k_rgb_channels[led].driver == i) {
                const my2k_rgb_channel *c      = &my2k_rgb_channels[led];
                uint8_t                 reg[3] = {c->r, c->g, c->b};
                for (unsigned color = 0; color < 3; color++) {
                    i2c_status_t next = my2k_rgb_bus_read(address, (uint8_t)((type == 3 ? 0x24 : 1) + reg[color]),
                                                          &d->sample[color], 1, 5);
                    if (next)
                        status = next;
                }
                break;
            }
    }
    d->read_status = status;
}
#    endif
bool my2k_qmk_rgb_init(bool authorized) {
    if (!authorized)
        return false;
    present_mask = family_mask = 0;
#    if MY2K_DEV2_PROFILE
    my2k_rgb_bus_init();
#        if MY2K_DEV3_PROFILE
    for (unsigned i = 0; i < 6; i++)
        readback(i, true);
#        endif
    /* A register-read ping is invalid for write-only3236, and is not a reliable
     * prerequisite for the other families. Real init write ACKs establish presence. */
    initialize_family(1);
    initialize_family(2);
    initialize_family(4);
    diagnostic.startup_test = 1;
#        if !MY2K_DEV3_PROFILE
    test_started = 0;
#        endif
#        if MY2K_DEV3_PROFILE
    diagnostic.startup_test = 0;
    for (unsigned i = 0; i < 6; i++)
        readback(i, false);
#        endif
#    else
    i2c_init();
    for (unsigned i = 0; i < 6; i++) {
        if (i2c_ping_address(my2k_rgb_drivers[i].address_byte, 5) == I2C_STATUS_SUCCESS)
            present_mask |= 1u << i;
        else
            errors++;
    }
    if ((present_mask & 3) == 3)
        initialize_family(1);
    if ((present_mask & 28) == 28)
        initialize_family(2);
    if (present_mask & 32)
        initialize_family(4);
#    endif
#    if MY2K_DEV4_PROFILE
    my2k_refresh_init();
    have_frame = false;
#    endif
    return family_mask != 0;
}
static uint8_t limited(uint8_t v) {
#    if MY2K_STOCK_PARITY_PROFILE
    /* Original3236/3731/3729 setters preserve all8PWMbits. Current controls and
     * channel scales stay at the recovered stock values; no extra PWM ceiling. */
    return v;
#    elif MY2K_DEV3_PROFILE
    return (uint8_t)(((uint16_t)v * 16 + 127) / 255); /* linear brightness, not saturation above16 */
#    else
    return v > 16 ? 16 : v;
#    endif
}
bool my2k_qmk_rgb_flush(const my2k_rgb_state *state) {
    if (!state || !family_mask)
        return false;
    diagnostic.nonzero_channels = 0;
#    if MY2K_DEV3_PROFILE
    for (unsigned i = 0; i < 6; i++)
        controller[i].nonzero_channels = 0;
#    endif
    for (unsigned i = 0; i < MY2K_RGB_COUNT; i++) {
        const my2k_rgb_channel *ch = &my2k_rgb_channels[i];
        my2k_rgb_color          c =
#    if MY2K_DEV4_PROFILE
            visual_blank ? (my2k_rgb_color){0, 0, 0} :
#    endif
                         state->rendered[i];
        c.r = limited(c.r);
        c.g = limited(c.g);
        c.b = limited(c.b);
#    if MY2K_DEV2_PROFILE
        if (diagnostic.startup_test)
            c = (my2k_rgb_color){16, 0, 0};
#    endif
#    if MY2K_DEV3_PROFILE
        if (test_group
#        if MY2K_DEV4_PROFILE
            && !visual_blank
#        endif
        ) {
            bool selected = ((test_group & 4) && i < 64) || ((test_group & 1) && i >= 64 && i < 125) ||
                            ((test_group & 2) && (i >= 125 || i == 64 || i == 65 || i == 112));
            c             = (my2k_rgb_color){0, 0, 0};
            if (selected) {
                if (test_color == 1)
                    c.r = 8;
                if (test_color == 2)
                    c.g = 8;
                if (test_color == 3)
                    c.b = 8;
            }
        }
        controller[ch->driver].nonzero_channels += (c.r != 0) + (c.g != 0) + (c.b != 0);
#    endif
        diagnostic.nonzero_channels += (c.r != 0) + (c.g != 0) + (c.b != 0);
#    if MY2K_DEV4_PROFILE
        my2k_refresh_set(ch->driver, ch->r, c.r);
        my2k_refresh_set(ch->driver, ch->g, c.g);
        my2k_refresh_set(ch->driver, ch->b, c.b);
#    else
        uint8_t type = my2k_rgb_drivers[ch->driver].type;
        if (type == 8 && (family_mask & 1))
            my2k_qmk_rgb_3729_set(ch->family_index, c.r, c.g, c.b);
        if (type == 3 && (family_mask & 2))
            my2k_qmk_rgb_3731_set(ch->family_index, c.r, c.g, c.b);
        if (type == 5 && (family_mask & 4))
            my2k_qmk_rgb_3236_set(ch->family_index, c.r, c.g, c.b);
#    endif
    }
#    if MY2K_DEV4_PROFILE
    my2k_refresh_staged();
    diagnostic.frames++;
    return true; /* Staging only; physical service is bounded. */
#    else
    for (unsigned bit = 1; bit <= 4; bit <<= 1)
        if (family_mask & bit) {
            batch_error = false;
#        if MY2K_DEV3_PROFILE
            batch_faults = 0;
#        endif
            if (bit == 1)
                my2k_qmk_rgb_3729_flush();
            else if (bit == 2)
                my2k_qmk_rgb_3731_flush();
            else
                my2k_qmk_rgb_3236_flush();
#        if MY2K_DEV3_PROFILE
            uint8_t mask = bit == 1 ? 3 : bit == 2 ? 28 : 32;
            if (!(mask & ~batch_faults))
                family_mask &= ~bit;
#        else
            if (batch_error)
                family_mask &= ~bit;
#        endif
        }
    if (family_mask)
        diagnostic.frames++;
    return family_mask != 0;
#    endif
}
#    if MY2K_DEV4_PROFILE
static bool refresh_write(uint8_t chip, uint8_t reg, const uint8_t *data, uint8_t size) {
#        if MY2K_DEV17_PROFILE
    /* Failed diagnostic page cleanup must not let PWM corrupt function data. */
    if (!my2k_rgb_observer_bank_safe(chip))
        return false;
#        endif
    batch_faults = 0;
    return my2k_rgb_i2c_write_register(my2k_rgb_drivers[chip].address_byte, reg, data, size, 5) == I2C_STATUS_SUCCESS;
}
#    endif
bool my2k_qmk_rgb_service(const my2k_rgb_state *state, uint32_t now, bool authorized) {
    if (!authorized || !state)
        return false;
#    if MY2K_DEV3_PROFILE
    if (test_group && (int32_t)(now - test_deadline) >= 0)
        test_group = 0;
#    endif
    if (!family_mask) {
        if ((uint32_t)(now - last_retry) < 2000)
            return false;
        last_retry = now;
        if (!my2k_qmk_rgb_init(true))
            return false;
    }
#    if MY2K_DEV2_PROFILE
#        if !MY2K_DEV3_PROFILE
    if (!test_started)
        test_started = now ? now : 1;
    else if ((uint32_t)(now - test_started) >= 3000)
        diagnostic.startup_test = 0;
#        endif
#    endif
#    if MY2K_DEV3_PROFILE
#        if !MY2K_DEV4_PROFILE
    if ((uint32_t)(now - last_readback) >= 1000) {
        last_readback = now;
        for (unsigned i = 0; i < 6; i++)
            readback(i, false);
    }
#        endif
#    endif
#    if MY2K_DEV4_PROFILE
    /* Keep the renderer at50Hz and service ONE changed transaction per pass.
     * Cached pre/post registers remain boot snapshots: no periodic read storm. */
    if (!have_frame || previous_blank != visual_blank || memcmp(last_frame, state->rendered, sizeof last_frame) ||
        previous_test != test_group || previous_color != test_color) {
        previous_blank = visual_blank;
        memcpy(last_frame, state->rendered, sizeof last_frame);
        previous_test  = test_group;
        previous_color = test_color;
        have_frame     = true;
        (void)my2k_qmk_rgb_flush(state);
    }
    uint8_t allowed = 0;
    for (unsigned i = 0; i < 6; i++)
        if (controller[i].initialized)
            allowed |= 1u << i;
    bool wrote = my2k_refresh_step(allowed, now, refresh_write);
    return wrote;
#    else
    return my2k_qmk_rgb_flush(state);
#    endif
}
#else
uint8_t my2k_qmk_rgb_present(void) {
    return 0;
}
uint8_t my2k_qmk_rgb_families(void) {
    return initialized ? 7 : 0;
}
uint32_t my2k_qmk_rgb_errors(void) {
    return faulted ? 1 : 0;
}
bool my2k_qmk_rgb_init(bool authorized) {
    initialized = false;
    if (!authorized)
        return false;
    batch_error = false;
    i2c_init();
    for (unsigned i = 0; i < 6; i++)
        if (i2c_ping_address(my2k_rgb_drivers[i].address_byte, 5) != I2C_STATUS_SUCCESS) {
            faulted = true;
            return false;
        }
    my2k_qmk_rgb_3729_init();
    if (batch_error)
        return false;
    my2k_qmk_rgb_3731_init();
    if (batch_error)
        return false;
    my2k_qmk_rgb_3236_init();
    if (batch_error)
        return false;
    initialized = true;
    faulted     = false;
    return true;
}
bool my2k_qmk_rgb_flush(const my2k_rgb_state *state) {
    if (!initialized || !state)
        return false;
    batch_error = false;
    for (unsigned i = 0; i < MY2K_RGB_COUNT; i++) {
        const my2k_rgb_channel *ch = &my2k_rgb_channels[i];
        my2k_rgb_color          c  = state->rendered[i];
        switch (my2k_rgb_drivers[ch->driver].type) {
            case 8:
                my2k_qmk_rgb_3729_set(ch->family_index, c.r, c.g, c.b);
                break;
            case 3:
                my2k_qmk_rgb_3731_set(ch->family_index, c.r, c.g, c.b);
                break;
            case 5:
                my2k_qmk_rgb_3236_set(ch->family_index, c.r, c.g, c.b);
                break;
            default:
                return false;
        }
    }
    my2k_qmk_rgb_3729_flush();
    if (batch_error)
        return false;
    my2k_qmk_rgb_3731_flush();
    if (batch_error)
        return false;
    my2k_qmk_rgb_3236_flush();
    if (batch_error)
        return false;
    return true;
}

bool my2k_qmk_rgb_service(const my2k_rgb_state *state, uint32_t now, bool authorized) {
    if (!authorized || !state)
        return false;
    if (faulted && !retry_wait) {
        retry_wait = true;
        last_retry = now;
        return false;
    }
    if (retry_wait && (uint32_t)(now - last_retry) < retry_delay)
        return false;
    if (!initialized) {
        if (!my2k_qmk_rgb_init(true)) {
            retry_wait = true;
            last_retry = now;
            if (retry_delay < 2000) {
                retry_delay *= 2;
                if (retry_delay > 2000)
                    retry_delay = 2000;
            }
            return false;
        }
    }
    if (!my2k_qmk_rgb_flush(state)) {
        retry_wait = true;
        last_retry = now;
        return false;
    }
    retry_wait  = false;
    retry_delay = 100;
    return true;
}
#endif
