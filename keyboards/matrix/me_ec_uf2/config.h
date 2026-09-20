
#pragma once

#define MATRIX_ROWS     6
#define MATRIX_COLS     16

// Map to the ChibiOS I²C driver you enabled (I2C1)
#define I2C_DRIVER           I2CD1
#define I2C1_SCL_PIN         B8
#define I2C1_SCL_PAL_MODE    4
#define I2C1_SDA_PIN         B9
#define I2C1_SDA_PAL_MODE    4
#define IS31FL3236_I2C_TIMEOUT      100   // ms before a failure
#define IS31FL3236_I2C_PERSISTENCE  0     // retry count

// Single chip, AD→GND ⇒ 7-bit addr 0x3C
#define IS31FL3236_I2C_ADDRESS_1    IS31FL3236_I2C_ADDRESS_GND
#define IS31FL3236_DRIVER_COUNT     1

// Data-in pin for WS2812
#define WS2812_DI_PIN        B7
#define WS2812_DRIVER = pwm
#define RGBLIGHT_LED_COUNT  4
#define RGBLIGHT_STARTUP_HUE    128
#define RGBLIGHT_STARTUP_SAT    255
#define RGBLIGHT_STARTUP_VAL    128
#define RGBLIGHT_LIMIT_VAL  255

// WS2812 Lighting Effect

#define RGBLIGHT_EFFECT_ALTERNATING
#define RGBLIGHT_EFFECT_BREATHING
#define RGBLIGHT_EFFECT_CHRISTMAS
#define RGBLIGHT_EFFECT_KNIGHT
#define RGBLIGHT_EFFECT_RAINBOW_MOOD
#define RGBLIGHT_EFFECT_RAINBOW_SWIRL
#define RGBLIGHT_EFFECT_RGB_TEST
#define RGBLIGHT_EFFECT_SNAKE
#define RGBLIGHT_EFFECT_STATIC_GRADIENT
#define RGBLIGHT_EFFECT_TWINKLE


#define RGB_MATRIX_DISABLE_SHARED_KEYCODES

#define DISCHARGE_WAIT_PRE  30
#define DISCHARGE_WAIT_POST 30
#define CHARGE_WAIT     2

#define ROW_6_PIN       B6
#define ROW_1_PIN       B5
#define ROW_2_PIN       B4
#define ROW_3_PIN       A15
#define ROW_4_PIN       B15
#define ROW_5_PIN       B1

#define LEFT_EN_PIN     B10
#define RIGHT_EN_PIN    B2
#define COL_A_PIN       B14
#define COL_B_PIN       B13
#define COL_C_PIN       B12

#define OPA_EN_PIN      A5
#define DISCHARGE_PIN   A6

#define USE_ADC1
#define ADC_INST        hadc1
#define KEY_IN_CHANNEL  ADC_CHANNEL_2
#define KEY_IN_PIN      GPIO_PIN_2
#define KEY_IN_PORT     GPIOA
#define SENSE_LOW       300
#define SENSE_HIGH      500

#define USE_PWM_TIM4
#define PWM_TIM_PERIOD      119
#define PWM_TIM             htim4
#define PWM_TIM_CHANNEL     TIM_CHANNEL_2

#define DEBOUNCE        10

#define USE_I2C1
#define DWT_DELAY

#define STM32_HSECLK                8000000U


//Used for the astro bootloader, im set to use dfu
 
#define VECT_TAB_OFFSET 0x20000
#define FEE_PAGE_SIZE 0x20000
#define FEE_PAGE_COUNT 1
#define FEE_MCU_FLASH_SIZE 512
#define FEE_PAGE_BASE_ADDRESS 0x8060000
#define FEE_DENSITY_BYTES 4096

