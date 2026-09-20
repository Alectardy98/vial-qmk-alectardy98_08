#pragma once

/* Matrix size */
#define MATRIX_ROWS 5
#define MATRIX_COLS 16

/* Debounce: keep taps snappy but filter chatter */
#define DEBOUNCE 5

#define SOLENOID_PIN D2                 // your old solenoid trigger
// #define SOLENOID_PIN_ACTIVE_LOW      // uncomment only if your driver is active-low

// If you have a separate enable/boost pin for the solenoid driver:
#define HAPTIC_ENABLE_PIN D3
// #define HAPTIC_ENABLE_PIN_ACTIVE_LOW  // uncomment only if active-low

// Default dwell (click length); tweak to taste
#define SOLENOID_DEFAULT_DWELL 12
#define SOLENOID_MIN_DWELL 4
#define SOLENOID_MAX_DWELL 100
#define SOLENOID_DWELL_STEP_SIZE 1
