/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* This TU-only rename redirects the MSD driver's inline bulk helper to the
 * board wrapper. The wrapper calls the original HAL API with identical args.
 * No shared source or USB error/recovery behavior is changed. */
#define usbhSynchronousTransfer my2k_debug_transfer
#include "hal.h"
#undef usbhSynchronousTransfer
