/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdio.h>
static inline int my2k_quiet_printf(const char *format, ...) {
    (void)format;
    return 0;
}
static inline int my2k_quiet_puts(const char *text) {
    (void)text;
    return 0;
}
#undef printf
#undef puts
#define printf my2k_quiet_printf
#define puts my2k_quiet_puts

/* The target has no stderr console. Keep I_Error's bounded vsnprintf+longjmp
 * intact; suppress only engine console logging, preserving argument evaluation. */
static inline int my2k_quiet_fprintf(FILE *stream, const char *format, ...) {
    (void)stream;
    (void)format;
    return 0;
}
#undef fprintf
#define fprintf my2k_quiet_fprintf
