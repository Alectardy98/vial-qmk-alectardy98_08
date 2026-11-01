/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include "../release/config.h"
#undef MY2K_RGB_RUNTIME_ENABLED
#define MY2K_RGB_RUNTIME_ENABLED 1
#undef PRODUCT
#include "dev_build.h"
#define PRODUCT "MY-2K " MY2K_DEV_BUILD
