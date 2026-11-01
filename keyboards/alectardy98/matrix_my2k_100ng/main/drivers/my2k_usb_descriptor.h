/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MY2K_USB_DESCRIPTOR_H
#define MY2K_USB_DESCRIPTOR_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Append a read-only MSC class interface without changing any HID descriptor,
 * interface number or endpoint. Failure leaves output length zero. */
bool my2k_usb_append_msc(const uint8_t *base, size_t base_size, uint8_t max_endpoint, uint8_t *out, size_t capacity, size_t *length, uint8_t *interface_number, uint8_t *endpoint_number);
#endif
