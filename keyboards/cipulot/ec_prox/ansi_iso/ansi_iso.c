#include "quantum.h"

#ifdef RGB_MATRIX_ENABLE
led_config_t g_led_config = {
    {
        { NO_LED }
    },
    {
        { 0, 0 }
    },
    {
        LED_FLAG_ALL
    }
};
#endif
