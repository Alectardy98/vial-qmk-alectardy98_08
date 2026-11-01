/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Speed-specific HID descriptors. High-speed MSC is intentionally unsupported;
 * dedicated storage mode retains the Dev15 full-speed64-byte bridge. */
bool my2k_input_usb_configuration(const uint8_t *base, size_t size, bool high, bool other, uint8_t keyboard_ep,
                                  uint8_t shared_ep, uint8_t *out, size_t capacity, size_t *length);
