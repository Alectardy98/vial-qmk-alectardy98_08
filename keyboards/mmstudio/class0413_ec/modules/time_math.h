// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#if !defined(__ASSEMBLER__) && !defined(__cplusplus)
#include <ch.h>
#include <stdint.h>
// Algebraic specializations only; preserve the configured timer and exact
// ChibiOS upward rounding, cast widths and 64-bit fallback semantics.
#if CH_CFG_ST_FREQUENCY != 100000 || CH_CFG_INTERVALS_SIZE != 32 || CH_CFG_TIME_TYPES_SIZE != 32
#error "Revalidate time conversion identities for changed ChibiOS time types"
#endif
_Static_assert(sizeof(int)==4,"32-bit int required");
#define EC_TIME_FAST_OR(value,fast,original) __extension__ ({time_conv_t ec_time_value_=(time_conv_t)(value); ec_time_value_<=UINT32_MAX ? (fast) : (original);})
#undef TIME_MS2I
static inline sysinterval_t ec_ms_signed(int32_t value){
    // Exact original uint64 wrap for negative int32 operands too.
    return (uint32_t)value*100U+(value<0?(uint32_t)(UINT64_MAX/1000U+1U):0U);
}
#define TIME_MS2I(value) __builtin_choose_expr((__builtin_types_compatible_p(__typeof__(value),int32_t)||__builtin_types_compatible_p(__typeof__(value),int)),ec_ms_signed(value),((sysinterval_t)EC_TIME_FAST_OR(value,(uint32_t)ec_time_value_*100U,(ec_time_value_*100000U+999U)/1000U)))
#undef TIME_US2I
#define TIME_US2I(value) ((sysinterval_t)EC_TIME_FAST_OR(value,(uint32_t)ec_time_value_/10U+((uint32_t)ec_time_value_%10U!=0),(ec_time_value_*100000U+999999U)/1000000U))
#undef TIME_I2S
#define TIME_I2S(value) ((time_secs_t)EC_TIME_FAST_OR(value,(uint32_t)ec_time_value_/100000U+((uint32_t)ec_time_value_%100000U!=0),(ec_time_value_+99999U)/100000U))
#undef TIME_I2MS
#define TIME_I2MS(value) ((time_msecs_t)EC_TIME_FAST_OR(value,(uint32_t)ec_time_value_/100U+((uint32_t)ec_time_value_%100U!=0),(ec_time_value_*1000U+99999U)/100000U))
#undef TIME_I2US
#define TIME_I2US(value) ((time_usecs_t)EC_TIME_FAST_OR(value,(uint32_t)ec_time_value_*10U,(ec_time_value_*1000000U+99999U)/100000U))
// For a 32-bit sysinterval_t, ceil(ticks/100) always fits time_msecs_t;
// the standard chTimeI2MS overflow assertion is therefore always satisfied.
#define chTimeI2MS(value) TIME_I2MS(value)
#endif
