#include QMK_KEYBOARD_H

#define DATA_PIN B2
#define CLK_PIN  B1
#define LATCH_PIN B3

static uint8_t led_index = 0;
static uint16_t last_tick = 0;

static void shift595_write(uint32_t pattern) {
    for (int i = 23; i >= 0; i--) {
        writePinLow(CLK_PIN);
        if (pattern & (1UL << i)) {
            writePinHigh(DATA_PIN);
        } else {
            writePinLow(DATA_PIN);
        }
        writePinHigh(CLK_PIN);
    }
    writePinLow(LATCH_PIN);
    writePinHigh(LATCH_PIN);
}

void keyboard_pre_init_kb(void) {
    setPinOutput(DATA_PIN);
    setPinOutput(CLK_PIN);
    setPinOutput(LATCH_PIN);
    writePinLow(DATA_PIN);
    writePinLow(CLK_PIN);
    writePinLow(LATCH_PIN);
    shift595_write(0);
}

static void run_led_chaser(void) {
    uint16_t now = timer_read();
    if (timer_elapsed(last_tick) > 1000) { // 1000 ms per LED
        last_tick = now;
        uint32_t pattern = (1UL << led_index);
        shift595_write(pattern);
        led_index = (led_index + 1) % 24;
    }
}

void housekeeping_task_kb(void) {
    run_led_chaser();
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(KC_A)
};
