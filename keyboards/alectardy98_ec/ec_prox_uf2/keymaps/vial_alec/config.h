

#define WS2812_BYTE_ORDER WS2812_BYTE_ORDER_RBG



// Native overlays reserve LED 0 for the existing layer indicator; other LEDs
// remain controlled by RGBLIGHT. Indicator remains visible with underglow off.
#define RGBLIGHT_LAYERS
#define RGBLIGHT_LAYERS_OVERRIDE_RGB_OFF
