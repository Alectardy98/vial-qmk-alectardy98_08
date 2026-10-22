/* Copyright 2023 Cipulot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include_next <mcuconf.h>

#undef STM32_ADC_USE_ADC1
#define STM32_ADC_USE_ADC1 TRUE

/* Stock application PLLCFGR=0x07412A04, CFGR=0x38881000.
 * The existing generic F401XC board must retain that clock configuration.
 */
#if STM32_HSECLK != 8000000U || STM32_PLLM_VALUE != 4 || STM32_PLLN_VALUE != 168 || STM32_PLLP_VALUE != 4 || STM32_PLLQ_VALUE != 7
#    error "Clock profile differs from the verified stock application"
#endif

/* Guards only: clock initialization remains the generic ChibiOS implementation. */
#if STM32_PLLSRC != STM32_PLLSRC_HSE || STM32_SW != STM32_SW_PLL
#    error "This keyboard requires the stock HSE/PLL clock source"
#endif
/* HAL derived-clock macros are defined after mcuconf.h; calculate from inputs. */
#if ((STM32_HSECLK / STM32_PLLM_VALUE) * STM32_PLLN_VALUE / STM32_PLLP_VALUE) != 84000000U || ((STM32_HSECLK / STM32_PLLM_VALUE) * STM32_PLLN_VALUE / STM32_PLLQ_VALUE) != 48000000U
#    error "Expected 84 MHz system clock and 48 MHz USB clock"
#endif



#undef STM32_PWM_USE_TIM3
#define STM32_PWM_USE_TIM3 TRUE
#undef STM32_GPT_USE_TIM4
#define STM32_GPT_USE_TIM4 TRUE
#if STM32_ST_USE_TIMER != 2 || STM32_PWM_USE_TIM2 || STM32_GPT_USE_TIM2 || STM32_GPT_USE_TIM3 || STM32_PWM_USE_TIM4
#error "Native audio conflicts with the system timer"
#endif
