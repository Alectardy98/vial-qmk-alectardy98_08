#include QMK_KEYBOARD_H
#include "ec_switch_matrix.h"
#include "print.h"
#include "timer.h"

#define CALABRATION_TIME_MS 60000

extern ec_config_t ec_config;
extern eeprom_ec_config_t eeprom_ec_config;

void ec_save_bottoming_reading(void) { for (uint8_t row=0; row<MATRIX_ROWS; row++) { for (uint8_t col=0; col<MATRIX_COLS; col++) { if (ec_config.bottoming_calibration_starter[row][col] || ec_config.bottoming_reading[row][col] < (ec_config.noise_floor[row][col] + BOTTOMING_CALIBRATION_THRESHOLD)) eeprom_ec_config.bottoming_reading[row][col]=1023; else eeprom_ec_config.bottoming_reading[row][col]=ec_config.bottoming_reading[row][col]; } } for (uint8_t row=0; row<MATRIX_ROWS; row++) { for (uint8_t col=0; col<MATRIX_COLS; col++) { uint16_t n=ec_config.noise_floor[row][col], b=eeprom_ec_config.bottoming_reading[row][col]; ec_config.rescaled_mode_0_actuation_threshold[row][col]=rescale(ec_config.mode_0_actuation_threshold,0,1023,n,b); ec_config.rescaled_mode_0_release_threshold[row][col]=rescale(ec_config.mode_0_release_threshold,0,1023,n,b); ec_config.rescaled_mode_1_initial_deadzone_offset[row][col]=rescale(ec_config.mode_1_initial_deadzone_offset,0,1023,n,b); ec_config.rescaled_mode_1_actuation_offset[row][col]=rescale(ec_config.mode_1_actuation_offset,0,1023,n,b); ec_config.rescaled_mode_1_release_offset[row][col]=rescale(ec_config.mode_1_release_offset,0,1023,n,b); } } eeconfig_update_kb_datablock(&eeprom_ec_config, 0, EECONFIG_KB_DATA_SIZE); }

static uint32_t calibration_timer;
static bool calibration_finished = false;

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = {
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO},
        {KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO}
    }
};

static void print_array(const char *name, uint16_t data[MATRIX_ROWS][MATRIX_COLS]) {
    uprintf("static const uint16_t %s[%u][%u] = {\n",
            name, (unsigned)MATRIX_ROWS, (unsigned)MATRIX_COLS);

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        uprintf("    {");
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uprintf("%u", data[row][col]);
            if (col != MATRIX_COLS - 1) {
                uprintf(", ");
            }
        }
        uprintf("}%s\n", row == MATRIX_ROWS - 1 ? "" : ",");
    }
    uprintf("};\n\n");
}

static void print_calibration_dump(void) {
    uprintf("\n\n/* ================================================ */\n");
    uprintf("/* VIRGO EC CALABRATION DATA - COPY EVERYTHING     */\n");
    uprintf("/* ================================================ */\n\n");

    print_array("EC_NOISE_FLOOR", ec_config.noise_floor);
    uint16_t bottoming[MATRIX_ROWS][MATRIX_COLS]; for (uint8_t r=0;r<MATRIX_ROWS;r++) for (uint8_t c=0;c<MATRIX_COLS;c++) bottoming[r][c]=eeprom_ec_config.bottoming_reading[r][c]; print_array("EC_BOTTOMING", bottoming);
    print_array("EC_APC_ACT", ec_config.rescaled_mode_0_actuation_threshold);
    print_array("EC_APC_REL", ec_config.rescaled_mode_0_release_threshold);
    print_array("EC_RT_DZ", ec_config.rescaled_mode_1_initial_deadzone_offset);

    uprintf("/* END VIRGO EC CALABRATION DATA */\n");
}

void keyboard_post_init_user(void) {
    /*
     * Normal keyboard initialization has already measured the noise floor.
     * Reset the temporary bottom-out capture state, then immediately enter
     * Cipulot's native bottoming calibration mode.
     */
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            ec_config.bottoming_calibration_starter[row][col] = true;
            ec_config.bottoming_reading[row][col] = 0;
        }
    }

    ec_config.bottoming_calibration = true;
    calibration_timer = timer_read32();

    uprintf("\n\n########################################\n");
    uprintf("# VIRGO EC CALABRATION STARTED         #\n");
    uprintf("########################################\n");
    uprintf("You have 60 seconds.\n");
    uprintf("Press EVERY physical key fully to bottom-out.\n");
    uprintf("Normal keypresses are intentionally disabled during calibration.\n");
    uprintf("At 60 seconds calibration will stop, save, and print C arrays.\n\n");
}

void housekeeping_task_user(void) {
    if (!calibration_finished &&
        timer_elapsed32(calibration_timer) >= CALABRATION_TIME_MS) {

        /*
         * Stop native calibration first. ec_save_bottoming_reading() then
         * validates the captured values, writes the bottoming readings to
         * EEPROM, and recalculates the rescaled thresholds.
         */
        ec_config.bottoming_calibration = false;
        ec_save_bottoming_reading();
        calibration_finished = true;

        uprintf("\n########################################\n");
        uprintf("# VIRGO EC CALABRATION COMPLETE        #\n");
        uprintf("########################################\n");
        uprintf("Calibration has been saved to EEPROM.\n");
        uprintf("Printing implementation data below...\n");

        print_calibration_dump();
    }
}
