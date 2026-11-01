/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_storage.h"
#include "aux_assets.h"
#include "aux_msc.h"
#include "aux_platform.h"
volatile aux_storage_diagnostics aux_storage_live;
static uint32_t                  retry_delay;
static void                      retry_later(void) {
    aux_storage_live.next_retry = aux_millis() + retry_delay;
    retry_delay                 = retry_delay < 15000u ? retry_delay * 2u : 30000u;
}
void aux_storage_start(void) {
    retry_delay      = 1000u;
    aux_storage_live = (aux_storage_diagnostics){.next_retry = aux_millis()};
}
void aux_storage_service(uint32_t now) {
    aux_asset_status status;
    aux_assets_status(&status);
    aux_storage_live.sectors = aux_sd_sectors();
    if (status.mounted) {
        aux_storage_live.stage =
            status.color_present || status.mono_present ? AUX_STORAGE_READY : AUX_STORAGE_NO_ASSETS;
        return;
    }
    if ((int32_t)(now - aux_storage_live.next_retry) < 0)
        return;
    if (aux_msc_media_writing() || !aux_msc_io_idle()) {
        aux_storage_live.stage = AUX_STORAGE_DEFERRED;
        aux_storage_live.deferred++;
        return;
    }
    aux_storage_live.attempts++;
    /* sector_count stays zero throughout aux_sd_init, so foreground MSC requests
     * cannot borrow the card while the cooperative initializer yields. A mounted
     * FIL is never invalidated by this retry path. */
    if (!aux_sd_sectors()) {
        aux_storage_live.sd_attempts++;
        if (!aux_sd_init()) {
            aux_storage_live.stage = AUX_STORAGE_SD_FAILED;
            retry_later();
            return;
        }
    }
    aux_storage_live.sectors = aux_sd_sectors();
    aux_storage_live.mount_attempts++;
    (void)aux_assets_init();
    aux_assets_status(&status);
    if (!status.mounted)
        aux_storage_live.stage = AUX_STORAGE_MOUNT_FAILED;
    else
        aux_storage_live.stage =
            status.color_present || status.mono_present ? AUX_STORAGE_READY : AUX_STORAGE_NO_ASSETS;
    if (status.mounted) {
        retry_delay                 = 1000u;
        aux_storage_live.next_retry = aux_millis() + retry_delay;
    } else
        retry_later();
}
