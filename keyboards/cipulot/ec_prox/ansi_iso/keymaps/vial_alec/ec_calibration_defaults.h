#pragma once
#include <stdint.h>
#include "ec_switch_matrix.h"  // for MATRIX_ROWS/COLS

#if (MATRIX_ROWS != 5) || (MATRIX_COLS != 15)
#    error "Calibration tables are sized for 5x15. Adjust tables or matrix size."
#endif

// Table declarations (definitions in ec_calibration_defaults.c)
extern const uint16_t EC_NOISE_FLOOR[5][15];
extern const uint16_t EC_BOTTOMING[5][15];
extern const uint16_t EC_APC_ACT[5][15];
extern const uint16_t EC_APC_REL[5][15];
extern const uint16_t EC_RT_DZ[5][15];

// Apply the constants above into runtime config structs (called from keyboard_init())
void ec_apply_calibration_defaults(void);
