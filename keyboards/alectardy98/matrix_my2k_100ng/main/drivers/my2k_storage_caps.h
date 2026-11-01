/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint8_t version;uint16_t read_bytes;} my2k_storage_caps;
/* Exact read-only pairing. Version7 is safe512 fallback; D015 negotiates4KiB.
 * Unknown or incomplete identities deliberately receive zero capability. */
my2k_storage_caps my2k_storage_negotiate(uint16_t vid,uint16_t pid,bool device_valid,bool inquiry_valid,const uint8_t inquiry[36]);
