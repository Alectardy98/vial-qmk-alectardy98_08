/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "raw_hid.h"
#if MY2K_ACTIVE_DEV_PROFILE
#include "main_debug.h"
#endif
#include "timer.h"
#include "my2k_usbh_chibios.h"
#include <stdint.h>
#if MY2K_EEPROM32_PROFILE
#include "main_eeprom.h"
#include "main_factory_reset.h"
#if MY2K_ANIMATION_PROFILE
#include "main_animation.h"
#include "main_keycodes.h"
#include "dynamic_keymap.h"
#endif
#include "main_storage_mode.h"
#include "main_oled_config.h"
#include "vial.h"
#endif
#if MY2K_DEV_PROFILE
#include "main_diag.h"
#endif
void            my2k_qmk_raw_hid_receive(uint8_t *, uint8_t);
static bool     file_pending;
static uint8_t  file_request[32];
static uint32_t file_started;
void            my2k_hid_file_task(uint32_t now) {
#if MY2K_ACTIVE_DEV_PROFILE
    my2k_debug_task(now);
#endif
    if (!file_pending) return;
    uint8_t reply[32];
    if (my2k_usbh_file_take_reply(reply)) {
        file_pending = false;
        raw_hid_send(reply, 32);
    } else if ((uint32_t)(now - file_started) > 3000) {
        my2k_usbh_file_cancel_reply();
        file_pending = false;
        for (unsigned i = 0; i < 32; i++)
            reply[i] = 0;
        reply[0] = 0xfd;
        reply[1] = file_request[1];
        reply[2] = 0x55;
        raw_hid_send(reply, 32);
    }
}
#if MY2K_DEV_PROFILE
/* Keep the complete packet entry callable for ELF-level regression tests. */
__attribute__((noinline,noclone,externally_visible))
#endif
void raw_hid_receive(uint8_t *data, uint8_t length) {
    if (!data || length != 32) return;
#if MY2K_EEPROM32_PROFILE
    /* Do not acknowledge settings/file/boot commands while a physical factory
     * reset is committing. Only the read-only EEPROM status pages are allowed. */
    if(my2k_eeprom32_reset_pending()){
        if(!my2k_eeprom32_hid(data,length))data[0]=0xff;
        raw_hid_send(data,32);return;
    }
    if(data[0]==253&&data[1]==236){
        bool allowed=vial_unlocked&&!file_pending&&!my2k_asset_export_active()&&
            !my2k_asset_transition_pending()&&!my2k_oled_main_busy()&&my2k_usbh_asset_idle();
        (void)my2k_factory_reset_hid(data,length,allowed);
        raw_hid_send(data,32);return;
    }
#endif
#if MY2K_ACTIVE_DEV_PROFILE
    if(my2k_debug_hid(data,length,vial_unlocked)){raw_hid_send(data,32);return;}
#endif
#if MY2K_DEV_PROFILE
#if MY2K_ANIMATION_PROFILE
    if(my2k_animation_hid(data,length,vial_unlocked)){raw_hid_send(data,32);return;}
    if(data[0]==253&&data[1]==238){
        if(data[2]==10){
            bool ok=false;
            if(data[3]==1&&vial_unlocked&&!file_pending&&my2k_usbh_asset_idle()&&!my2k_oled_main_busy()){
                if(my2k_asset_export_active())ok=true;
                else for(unsigned layer=0;layer<DYNAMIC_KEYMAP_LAYER_COUNT&&!ok;layer++)
                    for(unsigned row=0;row<MATRIX_ROWS&&!ok;row++)for(unsigned col=0;col<MATRIX_COLS&&!ok;col++)
                        if(dynamic_keymap_get_keycode(layer,row,col)==MY2K_STORAGE_TOGGLE)ok=my2k_asset_mode_request_key(row,col,true,timer_read32());
            }
            uint8_t token=data[4];for(unsigned i=0;i<32;i++)data[i]=0;
            data[0]=253;data[1]=238;data[2]=170;data[3]=1;data[4]=10;data[5]=token;data[6]=ok?2:4;
            raw_hid_send(data,32);return;
        }
        if(data[3]!=1||!data[4]||data[2]>8||(data[2]>=3&&!vial_unlocked)){
            uint8_t op=data[2],token=data[4];for(unsigned i=0;i<32;i++)data[i]=0;
            data[0]=253;data[1]=238;data[2]=170;data[3]=1;data[4]=op;data[5]=token;data[6]=3;
            raw_hid_send(data,32);return;
        }
    }

#endif
    if (my2k_diag_reply(data)) { raw_hid_send(data,32); return; }
    bool is_set=data[0]==5;
    if(is_set)my2k_diag_set_attempt(data);
#endif
    bool is_file = data[0] == 0xfd && (data[1] == 238 || data[1] == 35 || data[1] == 36 || data[1] == 37 || data[1] == 39 || data[1] == 40);
#if MY2K_STOCK_PARITY_PROFILE
    is_file|=data[0]==0xfd && (data[1]==38 || data[1]==41);
#endif
    if (!is_file && file_pending) {
        my2k_usbh_file_cancel_reply();
        file_pending = false;
    }
    if ((data[0] == 0x0e || data[0] == 0x0f || data[0] == 0x12 || data[0] == 0x13) && data[3] > 28) {
        data[0] = 0xff;
        raw_hid_send(data, 32);
        return;
    }
    /* In the retained Vial NVM implementation a locked odd-offset size0 SET
     * evaluates data[size-1] and underflows its scan bound. A zero-length request
     * is instead a true no-op, regardless of alignment. Do not modify global QMK. */
    if (data[0] == 0x13 && data[3] == 0) {
        raw_hid_send(data, 32);
        return;
    }
    if (is_file) {
        if (my2k_usbh_file_request(data)) {
            bool different = !file_pending;
            for (unsigned i = 0; i < 32; i++)
                if (file_request[i] != data[i]) different = true;
            if (different) file_started = timer_read32();
            for (unsigned i = 0; i < 32; i++)
                file_request[i] = data[i];
            file_pending = true;
            return;
        }
    }
    if(data[0]==253&&data[1]==238){
        uint8_t op=data[2],token=data[4];for(unsigned i=0;i<32;i++)data[i]=0;
        data[0]=253;data[1]=238;data[2]=170;data[3]=1;data[4]=op;data[5]=token;data[6]=4;
        raw_hid_send(data,32);return;
    }
    if (file_pending) {
        my2k_usbh_file_cancel_reply();
        file_pending = false;
    }
    my2k_qmk_raw_hid_receive(data, length);
#if MY2K_DEV_PROFILE
    if(is_set)my2k_diag_set_complete();
#endif
}
