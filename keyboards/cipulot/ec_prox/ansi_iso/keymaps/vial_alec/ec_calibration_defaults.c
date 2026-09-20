#include "ec_calibration_defaults.h"
#include "ec_switch_matrix.h"

// Provided by firmware (storage for runtime config and persisted EEPROM values)
extern eeprom_ec_config_t eeprom_ec_config;
extern ec_config_t        ec_config;

// ---------------------------
// Constant calibration tables
// ---------------------------
const uint16_t EC_NOISE_FLOOR[5][15] = {
    {   2,  62,  47,  39, 118, 128,  73,  69,  57,  79,  38,  89,  53,  53,  99 },
    { 123,  44,  61, 103,  61,  85, 140,  61,  57, 121,  80,  58,  67, 105,   0 },
    {  91,  43,  84,  88,  75, 188,  67,  82,  77, 127,  88, 117,   5,  92,   0 },
    {  81,  23,  98,  78, 113, 138, 171,  88,  58,  93,  90, 113,   0, 116, 129 },
    {   0,  86,  43,   0,   0,   0,  48,   0,   0,  45,  83,   0,   0,   0,   0 },
};

const uint16_t EC_BOTTOMING[5][15] = {
    { 581, 493, 445, 428, 502, 467, 496, 571, 592, 584, 521, 559, 549, 511, 550 },
    { 457, 415, 384, 476, 406, 350, 452, 482, 514, 552, 428, 508, 567, 363,1023 },
    { 312, 378, 416, 481, 386, 475, 314, 369, 318, 378, 338, 442,1023, 404,1023 },
    { 313,1023, 393, 380, 422, 479, 481, 437, 352, 375, 392, 385,1023, 357, 566 },
    {1023, 474, 438,1023,1023,1023, 420,1023,1023, 363, 417,1023,1023,1023,1023 },
};

const uint16_t EC_APC_ACT[5][15] = {
    { 313, 293, 260, 248, 324, 310, 300, 338, 344, 350, 297, 341, 319, 299, 341 },
    { 302, 243, 234, 303, 246, 227, 307, 287, 302, 352, 267, 299, 335, 243, 550 },
    { 209, 223, 262, 299, 242, 342, 199, 236, 206, 261, 222, 291, 552, 259, 550 },
    { 205, 560, 256, 240, 279, 321, 337, 275, 216, 244, 252, 259, 550, 245, 363 },
    { 550, 294, 255, 550, 550, 550, 248, 550, 550, 215, 262, 550, 550, 550, 550 },
};

const uint16_t EC_APC_REL[5][15] = {
    { 284, 272, 241, 229, 305, 293, 279, 314, 318, 325, 274, 318, 295, 276, 319 },
    { 286, 225, 218, 285, 229, 214, 292, 266, 280, 331, 250, 277, 311, 231, 500 },
    { 199, 206, 246, 280, 227, 328, 187, 222, 194, 249, 210, 275, 502, 244, 500 },
    { 194, 511, 242, 225, 264, 304, 322, 258, 201, 230, 237, 245, 500, 233, 342 },
    { 500, 275, 236, 500, 500, 500, 229, 500, 500, 200, 246, 500, 500, 500, 500 },
};

const uint16_t EC_RT_DZ[5][15] = {
    { 313, 293, 260, 248, 324, 310, 300, 338, 344, 350, 297, 341, 319, 299, 341 },
    { 302, 243, 234, 303, 246,  227, 307, 287, 302, 352, 267, 299, 335, 243, 550 },
    { 209, 223, 262, 299, 242,  342, 199, 236, 206, 261, 222, 291, 552, 259, 550 },
    { 205, 560, 256, 240, 279,  321, 337, 275, 216, 244, 252, 259, 550, 245, 363 },
    { 550, 294, 255, 550, 550,  550, 248, 550, 550, 215, 262, 550, 550, 550, 550 },
};

// -------------------------------------
// Apply constants into runtime structs
// -------------------------------------
void ec_apply_calibration_defaults(void) {
    for (uint8_t r = 0; r < MATRIX_ROWS; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            // noise_floor lives in ec_config
            ec_config.noise_floor[r][c] = EC_NOISE_FLOOR[r][c];

            // bottoming_reading lives in eeprom struct
            eeprom_ec_config.bottoming_reading[r][c] = EC_BOTTOMING[r][c];

            // rescaled APC thresholds live in ec_config
            ec_config.rescaled_mode_0_actuation_threshold[r][c] = EC_APC_ACT[r][c];
            ec_config.rescaled_mode_0_release_threshold[r][c]   = EC_APC_REL[r][c];

            // Rapid Trigger initial deadzone
            ec_config.rescaled_mode_1_initial_deadzone_offset[r][c] = EC_RT_DZ[r][c];
        }
    }

    // Keep the global base thresholds consistent with your logs
    ec_config.mode_0_actuation_threshold = 550;
    ec_config.mode_0_release_threshold   = 500;
}
