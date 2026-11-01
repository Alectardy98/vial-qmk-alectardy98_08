/*
    ChibiOS - Copyright (C) 2006..2020 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
/* MY2K experimental compatibility board; no Discovery pin aliases. */
#pragma once
#define BOARD_MY2K_APM32F405_COMPAT
#define BOARD_NAME "MY2K Geehy APM32F405RGT6 experimental"
#define STM32F405xx /* audited register-layer selection, not silicon identity */
#define STM32_HSECLK 16000000U
#define STM32_LSECLK 0U
#define STM32_VDD 330U
/* Original ASTRO loader's selected OTG2 full-speed configuration disables
 * VBUS sensing (08001ff4: GCCFG[21]=1, [19:18]=0). Keep the conservative
 * development profile available; the release profile explicitly selects this
 * board adaptation together with DCTL-based connect/disconnect. */
#if MY2K_GEEHY_FS_USB_ENABLE
#define BOARD_OTG_NOVBUSSENS
#endif
#define VAL_GPIOA_MODER 0x28000000U
#define VAL_GPIOA_OTYPER 0U
#define VAL_GPIOA_OSPEEDR 0U
#define VAL_GPIOA_PUPDR 0U
#define VAL_GPIOA_ODR 0xFFFFU
#define VAL_GPIOA_AFRL 0U
#define VAL_GPIOA_AFRH 0U
#define VAL_GPIOB_MODER 0U
#define VAL_GPIOB_OTYPER 0U
#define VAL_GPIOB_OSPEEDR 0U
#define VAL_GPIOB_PUPDR 0U
#define VAL_GPIOB_ODR 0xFFFFU
#define VAL_GPIOB_AFRL 0U
#define VAL_GPIOB_AFRH 0U
#define VAL_GPIOC_MODER 0U
#define VAL_GPIOC_OTYPER 0U
#define VAL_GPIOC_OSPEEDR 0U
#define VAL_GPIOC_PUPDR 0U
#define VAL_GPIOC_ODR 0xFFFFU
#define VAL_GPIOC_AFRL 0U
#define VAL_GPIOC_AFRH 0U
#define VAL_GPIOD_MODER 0U
#define VAL_GPIOD_OTYPER 0U
#define VAL_GPIOD_OSPEEDR 0U
#define VAL_GPIOD_PUPDR 0U
#define VAL_GPIOD_ODR 0xFFFFU
#define VAL_GPIOD_AFRL 0U
#define VAL_GPIOD_AFRH 0U
#define VAL_GPIOE_MODER 0U
#define VAL_GPIOE_OTYPER 0U
#define VAL_GPIOE_OSPEEDR 0U
#define VAL_GPIOE_PUPDR 0U
#define VAL_GPIOE_ODR 0xFFFFU
#define VAL_GPIOE_AFRL 0U
#define VAL_GPIOE_AFRH 0U
#define VAL_GPIOF_MODER 0U
#define VAL_GPIOF_OTYPER 0U
#define VAL_GPIOF_OSPEEDR 0U
#define VAL_GPIOF_PUPDR 0U
#define VAL_GPIOF_ODR 0xFFFFU
#define VAL_GPIOF_AFRL 0U
#define VAL_GPIOF_AFRH 0U
#define VAL_GPIOG_MODER 0U
#define VAL_GPIOG_OTYPER 0U
#define VAL_GPIOG_OSPEEDR 0U
#define VAL_GPIOG_PUPDR 0U
#define VAL_GPIOG_ODR 0xFFFFU
#define VAL_GPIOG_AFRL 0U
#define VAL_GPIOG_AFRH 0U
#define VAL_GPIOH_MODER 0U
#define VAL_GPIOH_OTYPER 0U
#define VAL_GPIOH_OSPEEDR 0U
#define VAL_GPIOH_PUPDR 0U
#define VAL_GPIOH_ODR 0xFFFFU
#define VAL_GPIOH_AFRL 0U
#define VAL_GPIOH_AFRH 0U
#define VAL_GPIOI_MODER 0U
#define VAL_GPIOI_OTYPER 0U
#define VAL_GPIOI_OSPEEDR 0U
#define VAL_GPIOI_PUPDR 0U
#define VAL_GPIOI_ODR 0xFFFFU
#define VAL_GPIOI_AFRL 0U
#define VAL_GPIOI_AFRH 0U
#define VAL_GPIOJ_MODER 0U
#define VAL_GPIOJ_OTYPER 0U
#define VAL_GPIOJ_OSPEEDR 0U
#define VAL_GPIOJ_PUPDR 0U
#define VAL_GPIOJ_ODR 0xFFFFU
#define VAL_GPIOJ_AFRL 0U
#define VAL_GPIOJ_AFRH 0U
#define VAL_GPIOK_MODER 0U
#define VAL_GPIOK_OTYPER 0U
#define VAL_GPIOK_OSPEEDR 0U
#define VAL_GPIOK_PUPDR 0U
#define VAL_GPIOK_ODR 0xFFFFU
#define VAL_GPIOK_AFRL 0U
#define VAL_GPIOK_AFRH 0U
#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif
