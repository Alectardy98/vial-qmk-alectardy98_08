// SPDX-License-Identifier: GPL-3.0-or-later
// EC42 v2.9 profile. Explicit fields avoid embedding metadata in absent switches.
// Native words are little endian; wire words are big endian. Overrides are bytes.
static uint8_t va_overrides[CELLS];
static uint16_t va_revision,va_saved;
static uint8_t va_status;
static uint32_t va_changed;
static bool store_raw(const eeprom_ec_config_t *record);
static bool packed(const eeprom_ec_config_t *x){return cell(x,EC_CAL_MAGIC_INDEX)==EC_CAL_MAGIC && cell(x,EC_CAL_METHOD_INDEX)==0x42;}
static bool crc_valid(const eeprom_ec_config_t *x){return checksum(x)==((uint32_t)cell(x,EC_CAL_CRC_HI_INDEX)<<16|cell(x,EC_CAL_CRC_LO_INDEX));}
static bool unpack_record(const eeprom_ec_config_t *x,eeprom_ec_config_t *canon,uint8_t *overrides){
 if(!packed(x)||!crc_valid(x))return false;
 for(unsigned i=0;i<CELLS;i++)if(x->overrides[i]>99)return false;
 if(x->actuation_mode>1||!x->mode_0_actuation_threshold||x->mode_0_actuation_threshold>=1023||!x->mode_0_release_threshold||x->mode_0_release_threshold>=x->mode_0_actuation_threshold||!x->mode_1_actuation_offset||!x->mode_1_release_offset)return false;
 eeprom_ec_config_t decoded=*x;seal(&decoded,EC_CAL_PARTIAL);
 if(!ec_cal_record_valid(&decoded))return false;
 if(canon)*canon=decoded;
 if(overrides)memcpy(overrides,x->overrides,CELLS);
 return true;
}
static bool pack_record(const eeprom_ec_config_t *canon,eeprom_ec_config_t *x){
 if(!ec_cal_record_valid(canon))return false;
 *x=*canon;memcpy(x->overrides,va_overrides,CELLS);seal(x,0x42);return true;
}
void ec_cal_empty(void){
    memset(&eeprom_ec_config,0,sizeof eeprom_ec_config);
    memset(va_overrides,0,sizeof va_overrides);
    eeprom_ec_config.mode_0_actuation_threshold=DEFAULT_MODE_0_ACTUATION_LEVEL;
    eeprom_ec_config.mode_0_release_threshold=DEFAULT_MODE_0_RELEASE_LEVEL;
    eeprom_ec_config.mode_1_initial_deadzone_offset=DEFAULT_MODE_0_ACTUATION_LEVEL;
    eeprom_ec_config.mode_1_actuation_offset=DEFAULT_MODE_1_ACTUATION_OFFSET;
    eeprom_ec_config.mode_1_release_offset=DEFAULT_MODE_1_RELEASE_OFFSET;
    seal(&eeprom_ec_config,EC_CAL_PARTIAL);
    va_status=4;va_revision=va_saved=0;
}
void ec_va_load(void){
    va_status=0;va_revision=va_saved=0;memset(va_overrides,0,sizeof va_overrides);
    eeprom_ec_config_t decoded;
    if(unpack_record(&eeprom_ec_config,&decoded,va_overrides)&&ec_cal_record_valid(&decoded))eeprom_ec_config=decoded;
    else ec_cal_empty();
}
bool ec_cal_store(const eeprom_ec_config_t *record){
    eeprom_ec_config_t encoded;
    if(!pack_record(record,&encoded)||!store_raw(&encoded))return false;
    va_status=0;va_saved=va_revision;return true;
}
