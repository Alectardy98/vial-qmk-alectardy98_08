// SPDX-License-Identifier: GPL-2.0-or-later
#include "diag.h"
#include "usb_device_state.h"
#include "print.h"
/* Private guard/error state; ordinary ChibiOS BSS initialization is sufficient. */
volatile diag_record_t efl_diag_record;
void diag_mark(uint32_t phase) { (void)phase; }
void eeprom_driver_init(void) { s2_storage_init(); }
/* Report actionable storage failures once, without periodic diagnostic traffic. */
void housekeeping_task_user(void) {
 static uint32_t reported;
 uint32_t error=efl_diag_record.error;
 if (!error || error==reported || usb_device_state_get_configure_state()!=USB_DEVICE_STATE_CONFIGURED) return;
 uprintf("EEPROM persistence disabled: error=%lu flash=%lu\n",
         (unsigned long)error,(unsigned long)efl_diag_record.last_flash_status);
 reported=error;
}

/* A RAM EEPROM readback alone must never claim a failed physical write persisted. */
bool ec_storage_ready(void) { return s2_armed && efl_diag_record.backend_ok && !efl_diag_record.error; }
