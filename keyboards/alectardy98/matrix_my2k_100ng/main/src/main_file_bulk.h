/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "hal.h"
#include "usbh/dev/msd.h"
bool my2k_file_bulk_write(USBHMassStorageLUNDriver *,uint8_t *,unsigned,unsigned,uint8_t,uint8_t,uint8_t,uint32_t);
void my2k_file_bulk_cbw(usbh_ep_t *,void *,uint32_t);

bool my2k_file_bulk_read(USBHMassStorageLUNDriver *lun,uint8_t *data,unsigned n,uint8_t slot,uint8_t token,uint32_t at);

bool my2k_file_direct_read(USBHMassStorageLUNDriver *,uint8_t *,unsigned,uint8_t,uint8_t,uint32_t);
