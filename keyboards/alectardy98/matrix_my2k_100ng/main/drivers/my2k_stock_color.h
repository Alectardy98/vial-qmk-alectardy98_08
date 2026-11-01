/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "my2k_rgb.h"
/* The stock protocol uses byte HSV, with 43 hue units per sector, integer
 * truncation and a CIE lightness curve applied to V before RGB conversion.
 * RGB->HSV deliberately does NOT invert that curve. Thus RGB SET followed by
 * RGB GET is not generally an identity; HSV GET/SET preserves canonical state.
 * rgb_to_hsv returns H/S/V in the existing triple's r/g/b members. */
my2k_rgb_color my2k_stock_hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v);
my2k_rgb_color my2k_stock_rgb_to_hsv(my2k_rgb_color rgb);
