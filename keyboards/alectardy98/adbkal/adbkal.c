#include "quantum.h"
#include "display.h"

void keyboard_post_init_kb(void) {
    adbk_display_init();
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    adbk_display_task();
    housekeeping_task_user();
}
