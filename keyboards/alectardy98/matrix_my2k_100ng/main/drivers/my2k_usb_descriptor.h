/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_USB_DESCRIPTOR_H
#define MY2K_USB_DESCRIPTOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Append an MSC class interface without changing existing HID descriptors,
 * interface numbers or endpoints. SCSI policy controls write permissions;
 * descriptor construction does not. Failure leaves output length zero. */
bool my2k_usb_append_msc(const uint8_t *base, size_t base_size, uint8_t max_endpoint, uint8_t *out, size_t capacity,
                         size_t *length, uint8_t *interface_number, uint8_t *endpoint_number);
#endif
