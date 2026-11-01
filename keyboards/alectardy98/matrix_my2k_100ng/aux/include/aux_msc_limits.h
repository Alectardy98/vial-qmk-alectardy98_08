/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* D015 is a versioned read-only capability profile, not a new USB packet size.
 * Endpoint packets remain64bytes. The larger TinyUSB buffer bounds one
 * asynchronous callback and reduces foreground/worker round trips. */
#if MY2K_DEV15_PROFILE
#    if !MY2K_DEV7_PROFILE
#        error "D015 must retain the Dev7 read-only storage policy"
#    endif
#    define AUX_MSC_READ_MAX_BYTES 4096u
#    if MY2K_RELEASE1_PROFILE
#        define AUX_MSC_INQUIRY_REVISION "R001"
#    elif MY2K_DEV21_PROFILE
#        define AUX_MSC_INQUIRY_REVISION "D021"
#    else
#        define AUX_MSC_INQUIRY_REVISION "D015"
#    endif
#else
#    define AUX_MSC_READ_MAX_BYTES 512u
#endif
