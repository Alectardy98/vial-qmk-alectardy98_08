/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_DIAGNOSTICS_H
#define AUX_DIAGNOSTICS_H
#include "aux_panels.h"
/* Pure cached-RAM snapshot. This function never performs bus/filesystem I/O. */
bool aux_diagnostics_snapshot(uint8_t page, const aux_panels *, uint8_t bytes[8]);
#endif
