/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t  version;
    uint16_t read_bytes;
} my2k_storage_caps;
/* Exact AUX identity matching and read-transfer limits. Version 7 permits
 * 512-byte reads; D015 and Release 1 negotiate 4KiB. Write authorization is
 * checked separately. Unknown/incomplete identities receive zero capability. */
my2k_storage_caps my2k_storage_negotiate(uint16_t vid, uint16_t pid, bool device_valid, bool inquiry_valid,
                                         const uint8_t inquiry[36]);
