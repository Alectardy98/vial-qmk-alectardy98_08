/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "i2c_master.h"
void my2k_rgb_bus_init(void);
i2c_status_t my2k_rgb_bus_write(uint8_t,uint8_t,const uint8_t *,uint16_t,uint16_t);
i2c_status_t my2k_rgb_bus_read(uint8_t,uint8_t,uint8_t *,uint16_t,uint16_t);
uint8_t my2k_rgb_bus_lines(void); /* bit0 SCL, bit1 SDA; sampled at idle */
uint8_t my2k_rgb_bus_fault(void); /* 0 OK,1SCL timeout,2SDA stuck,3NACK,4bad request */
uint32_t my2k_rgb_bus_recoveries(void);
#if MY2K_DEV10_PROFILE
/* Passive cache of existing WRITE transactions; querying never touches I2C.
 * "fault" is the earliest failure, before STOP can overwrite the global fault;
 * status is the unchanged final return value. Index is zero-based within the
 * payload, or UINT16_MAX outside DATA. Timestamps are completion uptime in ms. */
enum {
    MY2K_RGB_PHASE_NONE,
    MY2K_RGB_PHASE_VALIDATE,
    MY2K_RGB_PHASE_START,
    MY2K_RGB_PHASE_ADDRESS,
    MY2K_RGB_PHASE_REGISTER,
    MY2K_RGB_PHASE_DATA,
    MY2K_RGB_PHASE_STOP
};
typedef struct {
    uint32_t attempts, failures, first_ms, last_ms;
    uint16_t first_data_index, last_data_index;
    uint8_t first_register, last_register, first_fault, last_fault;
    uint8_t first_phase, last_phase;
    int8_t first_status, last_status;
} my2k_rgb_bus_failure_diagnostics;
bool my2k_rgb_bus_failure(unsigned controller, my2k_rgb_bus_failure_diagnostics *out);
#endif

#if MY2K_DEV19_PROFILE
bool my2k_rgb_bus_hw_page(uint8_t page,uint8_t index,uint8_t out[32]);
#endif
