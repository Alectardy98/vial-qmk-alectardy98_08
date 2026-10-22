// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// Preserve the original Prox's GPIO startup values instead of adopting the
// generic F411 Nucleo pin defaults. The two upstream board.c files are identical.
// Reuse the original Apache-2.0 board header, retaining its original notices.
#include "lib/chibios/os/hal/boards/ST_STM32F401C_DISCOVERY/board.h"
#undef STM32F401xC
#ifndef STM32F411xE
#define STM32F411xE
#endif
#undef STM32_HSE_BYPASS
