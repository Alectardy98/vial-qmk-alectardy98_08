/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <stdbool.h>
/* Unique I/O worker only. Game page cache already supplies read locality;
 * do not amplify random512B misses into unrelated4KiB card reads. */
void aux_disk_game_reads(bool enabled);
