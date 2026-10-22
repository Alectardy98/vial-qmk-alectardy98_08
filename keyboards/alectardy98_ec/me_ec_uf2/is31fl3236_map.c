#ifdef RGB_MATRIX_ENABLE
#include "drivers/led/issi/is31fl3236.h"
#include "rgb_matrix.h"


const is31fl3236_led_t PROGMEM g_is31fl3236_leds[IS31FL3236_LED_COUNT] = {
    { 0, OUT36, OUT35, OUT34 },  // was {0, OUT34, OUT35, OUT36}
    { 0, OUT33, OUT32, OUT31 },  // was {0, OUT31, OUT32, OUT33}
    { 0, OUT30, OUT29, OUT28 },
    { 0, OUT27, OUT26, OUT25 },
    { 0, OUT24, OUT23, OUT22 },
    { 0, OUT21, OUT20, OUT19 },
    { 0, OUT18, OUT17, OUT16 },
    { 0, OUT15, OUT14, OUT13 },
    { 0, OUT12, OUT11, OUT10 },
    { 0, OUT9,  OUT8,  OUT7  },
    { 0, OUT6,  OUT5,  OUT4  },
    { 0, OUT3,  OUT2,  OUT1  }
};

#define LED_COUNT 12

led_config_t g_led_config = {
    {
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3, 4 },
        { 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3, 4, 5, 6, 7, 8 },
        { 10, 11, 12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1 },
        { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3 },
        { 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3, 4 },
        { 5, 6, 7, 8, 9, 10, 11, 12, 1, 2 },
    },
    {
        { 1, 1 },    // LED 1
        { 1, 2 },    // LED 2
        { 1, 3 },    // LED 3
        { 1, 4 },    // LED 4
        { 1, 5 },    // LED 5
        { 1, 6 },    // LED 6
        { 1, 7 },    // LED 7
        { 1, 8 },    // LED 8
        { 1, 9 },    // LED 9
        { 1, 10 },   // LED 10
        { 1, 11 },   // LED 11
        { 1, 12 },   // LED 12
    },
    {
        [0 ... LED_COUNT-1] = LED_FLAG_KEYLIGHT
    },
};
#endif  // RGB_MATRIX_ENABLE
