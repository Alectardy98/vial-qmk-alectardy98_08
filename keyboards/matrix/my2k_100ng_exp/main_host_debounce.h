/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
/* Called only at the first ATTACHED debounce-return boundary. A true result
 * acknowledges that observed connection-change indication so the caller can wait ONE fresh window.
 * No MMIO write, port reset, task delay or synthetic connection is performed. */
bool my2k_host_debounce_rearm(void);
