// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#define HAL_USE_ADC TRUE
#include_next <halconf.h>

#undef HAL_USE_PWM
#define HAL_USE_PWM TRUE
#undef HAL_USE_GPT
#define HAL_USE_GPT TRUE
