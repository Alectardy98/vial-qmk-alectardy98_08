#include "ps2.h"
#include "led.h"

bool led_update_kb(led_t led_state) {
    if (led_update_user(led_state)) {
        ps2_host_set_led(led_state.caps_lock ? 0x01 : 0x00);
    }
    return false;
}
