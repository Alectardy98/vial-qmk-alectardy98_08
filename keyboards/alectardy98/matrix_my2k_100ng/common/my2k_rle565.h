/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* PackBits over opaque two-byte words. Bit7=repeat; low7+1 words.
 * Literals carry2*count bytes; repeats carry exactly2 bytes. */
bool my2k_rle565_decode(const uint8_t *source,size_t bytes,uint8_t *out,size_t length);
