// SPDX-License-Identifier: GPL-3.0-or-later
// A record is canonical bytes; its logical EEPROM placement is a board contract.
#include "calibration.h"
#include "diag.h"
#include "dynamic_keymap.h"
#include "eeprom.h"
#include <string.h>
#ifdef EC_RECORD_SPLIT
_Static_assert(EECONFIG_KB_DATA_SIZE==219,"Preserve existing Vial base");
_Static_assert(EC_RECORD_TAIL_ADDR+sizeof(eeprom_ec_config_t)-EECONFIG_KB_DATA_SIZE==WEAR_LEVELING_LOGICAL_SIZE,"Tail bounds");
_Static_assert(DYNAMIC_KEYMAP_EEPROM_MAX_ADDR+1==EC_RECORD_TAIL_ADDR,"Macro/record separation");
// Match QMK's macro delimiter convention. Never overwrite a live old macro tail.
// A shortened/rewritten macro buffer must contain every terminator and QMK's
// end-of-buffer zero before the reserved EC region. No EEPROM repair writes.
bool ec_record_storage_ready(void) {
    if(!ec_storage_ready())return false;
    uint32_t n=dynamic_keymap_macro_get_buffer_size(),zeros=0;
    if(!n)return false;
    uint8_t v;
    dynamic_keymap_macro_get_buffer(n-1,1,&v);if(v)return false;
    for(uint32_t i=0;i<n && zeros<DYNAMIC_KEYMAP_MACRO_COUNT;i++){
        dynamic_keymap_macro_get_buffer(i,1,&v);if(!v)zeros++;
    }
    return zeros==DYNAMIC_KEYMAP_MACRO_COUNT;
}
uint32_t ec_record_read(void *data,uint32_t offset,uint32_t length) {
    if(offset>sizeof(eeprom_ec_config_t)||length>sizeof(eeprom_ec_config_t)-offset)return 0;
    uint32_t first=offset<EECONFIG_KB_DATA_SIZE?MIN(length,EECONFIG_KB_DATA_SIZE-offset):0;
    if(first && eeconfig_read_kb_datablock(data,offset,first)!=first)return 0;
    if(length>first)eeprom_read_block((uint8_t*)data+first,(const void*)(uintptr_t)(EC_RECORD_TAIL_ADDR+offset+first-EECONFIG_KB_DATA_SIZE),length-first);
    return length;
}
uint32_t ec_record_write(const void *data,uint32_t offset,uint32_t length) {
    if(!ec_record_storage_ready()||offset>sizeof(eeprom_ec_config_t)||length>sizeof(eeprom_ec_config_t)-offset)return 0;
    uint32_t first=offset<EECONFIG_KB_DATA_SIZE?MIN(length,EECONFIG_KB_DATA_SIZE-offset):0;
    if(first && eeconfig_update_kb_datablock(data,offset,first)!=first)return 0;
    if(length>first)eeprom_update_block((const uint8_t*)data+first,(void*)(uintptr_t)(EC_RECORD_TAIL_ADDR+offset+first-EECONFIG_KB_DATA_SIZE),length-first);
    return ec_storage_ready()?length:0;
}
#endif
