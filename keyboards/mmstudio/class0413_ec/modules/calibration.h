// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ec_switch_matrix.h"
// Allocation/addresses are unchanged. Method 3 packs validity/enabled flags into measured cells;
// method/CRC metadata remains in firmware-impossible matrix slots.
#define EC_CAL_MAGIC_INDEX 37
#define EC_CAL_METHOD_INDEX 58
#define EC_CAL_CRC_LO_INDEX 62
#define EC_CAL_CRC_HI_INDEX 78
#define EC_CAL_MAGIC 0xEC03
#define EC_CAL_MANUAL 1
#define EC_CAL_AUTO 2
enum { CAL_IDLE, CAL_MANUAL, CAL_RESERVED_2, CAL_REVIEW, CAL_READY, CAL_APPLIED };
extern uint8_t ec_cal_state;
extern uint16_t ec_cal_session;
uint8_t ec_cal_method(void);
void ec_cal_abort(void);
void ec_cal_sample(uint8_t r,uint8_t c,uint16_t raw,bool logical);
bool ec_cal_runtime(void);
bool ec_cal_record_valid(const eeprom_ec_config_t *record);
bool ec_cal_protocol(uint8_t op,const uint8_t *p,uint8_t n,uint8_t *out,uint8_t *length,uint8_t *error);
uint8_t ec_cal_cell_flags(uint8_t r,uint8_t c);

bool ec_cal_import_valid(const eeprom_ec_config_t *record);
bool ec_cal_store(const eeprom_ec_config_t *record);
bool ec_cal_active_valid(void);

// Canonical EC wire bytes: packed little-endian fields are sent big-endian.
uint8_t ec_record_byte(const eeprom_ec_config_t *record,unsigned offset);
void ec_record_set_byte(eeprom_ec_config_t *record,unsigned offset,uint8_t value);

#define EC_CAL_PARTIAL 3
#define EC_CAL_QUALIFIED 0x4000U
#define EC_CAL_ENABLED 0x8000U
uint16_t ec_cal_bottom(uint8_t row,uint8_t col);
bool ec_cal_enabled(uint8_t row,uint8_t col);

#define EC_CAL_RETRY 0x2000U

void ec_cal_empty(void);
void ec_va_load(void);
void ec_va_task(void);
void ec_va_disconnected(void);
bool ec_va_protocol(uint8_t op,const uint8_t *p,uint8_t n,uint8_t *out,uint8_t *length,uint8_t *error);
void ec_va_thresholds(uint8_t row,uint8_t col);

bool ec_cal_restore_record(const eeprom_ec_config_t *record);

bool ec_va_dynamic(void);
bool ec_rt_key(uint8_t row,uint8_t col,uint16_t raw,bool current);
void ec_rt_boundaries(uint8_t row,uint8_t col,uint16_t *press,uint16_t *release);
