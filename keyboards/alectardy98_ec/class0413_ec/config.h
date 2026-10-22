#pragma once

/* MM Studio Class0413 EC: proven Cipulot-based EC sensing and hardware mapping. */


/* Verified logical row order; push-pull outputs, low initially, rising excitation. */
#define MATRIX_ROW_PINS { B8, B7, B6, B5, B3 }

#define AMUX_COUNT 3
#define AMUX_MAX_COLS_COUNT 8

/* Verified active-low enables: HIGH disables, LOW enables. */
#define AMUX_EN_PINS { A4, A5, A7 }

/* Verified least-significant address bit first: S0=PB0, S1=PB1, S2=PB10. */
#define AMUX_SEL_PINS { B0, B1, B10 }

/* Logical matrix columns are concatenated in bank order: 0..7, 8..15, 16..20. */
#define AMUX_COL_CHANNELS_SIZES { 8, 8, 5 }
#define AMUX_0_COL_CHANNELS { 3, 0, 1, 2, 4, 6, 7, 5 }
#define AMUX_1_COL_CHANNELS { 3, 0, 1, 2, 4, 6, 7, 5 }
#define AMUX_2_COL_CHANNELS { 3, 0, 1, 2, 4 }
#define AMUX_COL_CHANNELS AMUX_0_COL_CHANNELS, AMUX_1_COL_CHANNELS, AMUX_2_COL_CHANNELS

/* Verified ADC1_IN2 on PA2; analog mode, no internal pull. */
#define ANALOG_PORT A2

/* Verified open-drain output. LOW sinks/discharges; HIGH releases to high-Z.
 * Cipulot's STM32 open-drain path must be used. Do not drive PA3 push-pull HIGH.
 */
#define DISCHARGE_PIN A3

/* Stock indicators: Num PA15, Caps PB15; both active-low. */
#define LED_NUM_LOCK_PIN A15
#define LED_CAPS_LOCK_PIN B15
#define LED_PIN_ON_STATE 0


/* Cipulot EC60 defaults, not original firmware thresholds/timing. */
#define DEFAULT_ACTUATION_MODE 0
#define DEFAULT_MODE_0_ACTUATION_LEVEL 550
#define DEFAULT_MODE_0_RELEASE_LEVEL 500
#define DEFAULT_MODE_1_INITIAL_DEADZONE_OFFSET DEFAULT_MODE_0_ACTUATION_LEVEL
#define DEFAULT_MODE_1_ACTUATION_OFFSET 70
#define DEFAULT_MODE_1_RELEASE_OFFSET 70
#define DEFAULT_EXTREMUM 1023
#define EXPECTED_NOISE_FLOOR 0
#define NOISE_FLOOR_THRESHOLD 50
#define BOTTOMING_CALIBRATION_THRESHOLD 100
#define DEFAULT_NOISE_FLOOR_SAMPLING_COUNT 30
#define DEFAULT_BOTTOMING_READING 1023
#define DEFAULT_CALIBRATION_STARTER true

#define DISCHARGE_TIME 10


#define EECONFIG_KB_DATA_SIZE 219
#define DYNAMIC_KEYMAP_LAYER_COUNT 4

/* Absent from every layout option in the recovered Vial definition. */
#define UNUSED_POSITIONS_LIST { {1, 16}, {2, 16}, {2, 20}, {3, 15}, {3, 20}, {4, 5}, {4, 6}, {4, 7}, {4, 9}, {4, 10}, {4, 11}, {4, 12}, {4, 16}, {4, 17} }

/* Route existing Cipulot custom VIA channel through this checkout's legacy core. */
#define VIA_CUSTOM_LIGHTING_ENABLE

/* Standard embedded-flash EEPROM, guarded to sector 2. */
#define BACKING_STORE_WRITE_SIZE 4

/* Measured 128 KiB geometry; one guarded 16 KiB EEPROM sector. */
#define WEAR_LEVELING_LOGICAL_SIZE 4096
#define WEAR_LEVELING_BACKING_SIZE (16U * 1024U)
#define WEAR_LEVELING_EFL_FIRST_SECTOR 2
#define WEAR_LEVELING_EFL_FLASH_SIZE (128U * 1024U)

/* Exclude sectors 3, 4 even from the backend sector-selection search. */
#define WEAR_LEVELING_EFL_OMIT_LAST_SECTOR_COUNT 2

#ifdef EC_NATIVE_ACTUATORS
#define SOLENOID_PIN B13
#define HAPTIC_ENABLE_PIN B14
#define HAPTIC_OFF_IN_LOW_POWER 1
#define SOLENOID_DEFAULT_DWELL 4
#define SOLENOID_MIN_DWELL 4
#define SOLENOID_MAX_DWELL 45
#define SOLENOID_DEFAULT_BUZZ 0
#define AUDIO_PIN A6
#define AUDIO_PIN_ALT B12
#define AUDIO_PIN_ALT_AS_NEGATIVE
#define AUDIO_PWM_DRIVER PWMD3
#define AUDIO_PWM_CHANNEL 1
#define AUDIO_STATE_TIMER GPTD4
#define AUDIO_DEFAULT_ON false
#define AUDIO_DEFAULT_CLICKY_ON false
/* This QMK checkout enables native Clicky with a C define, not a make option. */
#define AUDIO_CLICKY
#define AUDIO_INIT_DELAY
#define STARTUP_SONG SONG(NO_SOUND)
#define GOODBYE_SONG SONG(NO_SOUND)
#endif

/* Canonical EC42 record: preserve the old 219-byte KB reservation and every
 * Vial address. Only the record tail uses the final 113 logical EEPROM bytes. */
#define A98_RECORD_MAGIC 0xa105
#define A98_FIRMWARE_ID "A98-0413-EC42-2.9"
#define EC_RECORD_SPLIT
#define EC_RECORD_TAIL_ADDR 3983U
#define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR 3982
#define A98_STORAGE_START 0x8000U
#define A98_STORAGE_END 0xc000U

// Retain the proven noinline rescale helper in matrix.c; decisions are shared.
#define EC_BOARD_RESCALE
