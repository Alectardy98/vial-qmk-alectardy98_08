/* SPDX-License-Identifier: GPL-2.0-or-later */
#if MY2K_DEV12_PROFILE
#    include "main_host_events.h"
#endif
#if MY2K_DEV11_PROFILE
#    include "main_scheduler.h"
#endif
#if MY2K_DEV10_PROFILE
#    include "main_host_trace.h"
#    include "timer.h"
#endif
#include "ch.h"
#include "hal.h"
#include "my2k_block.h"
#include "my2k_usbh_chibios.h"
#include "my2k_usb_msc.h"
#include "main_file_bulk.h"
#if MY2K_DEV9_PROFILE
#    include "main_host_mode.h"
#endif
#include "my2k_main_fs.h"
#if MY2K_DEV7_PROFILE
#    include "main_storage_mode.h"
#endif
#include "usbh/dev/msd.h"
#include <string.h>
#if MY2K_RELEASE1_PROFILE
#    include "main_oled_config.h"
#endif
#if !HAL_USE_USBH || !HAL_USBH_USE_MSD || !STM32_USBH_USE_OTG1 || STM32_USBH_USE_OTG2
#    error "MY2K internal host needs Contrib USBH/MSD on OTG1(FS), not PC device OTG2"
#endif
static bool started;
#if MY2K_STOCK_PARITY_PROFILE
static bool start_attempted;
#endif
static bool    disconnected;
static mutex_t lock;
#if MY2K_DEV15_PROFILE
#    include "my2k_storage_caps.h"
#    include "main_perf.h"
static binary_semaphore_t work_ready;
static my2k_storage_caps  negotiated;
static uint32_t           queued_cycles, read_requests, read_bytes, read_failures;
#endif
static my2k_block_bridge bridge;
static enum { IDLE, PENDING, RUNNING, DONE } state;
static struct {
    uint32_t lba;
    uint16_t offset, size;
    int32_t  result;
    _Alignas(4) uint8_t bytes[MY2K_BRIDGE_MAX_TRANSFER];
} request;
#if MY2K_RELEASE1_PROFILE
static volatile bool files_changed;
void                 my2k_usbh_files_changed(void) {
    files_changed = true;
}
static uint8_t request_kind; /* 0=read,1=write512,2=sync. One existing queue owner. */
#endif
static bool     animation_upload;
static bool     file_fast;
static bool     bulk_peer, bulk_session, bulk_read_peer, bulk_rle_peer, bulk_direct_peer;
static uint8_t  bulk_credits;
static uint32_t bulk_next;
static uint8_t *fast_buffer;
static uint16_t fast_size;
#if MY2K_ANIMATION_PROFILE
static bool     animation_peer;
static uint32_t animation_epoch;
#endif
static bool media_ready(void *ctx);
static enum { FILE_IDLE, FILE_PENDING, FILE_RUNNING, FILE_DONE } file_state;
static struct {
    uint8_t request[32], response[32];
    bool    deliver;
} file_job;
#if MY2K_DEV6_PROFILE
static volatile uint32_t connect_attempts, connect_returns, connect_failures;
static volatile uint8_t  worker_phase, last_connect_result = 255;
#    define HOST_PHASE(value) (worker_phase = (value))
#else
#    define HOST_PHASE(value) ((void)0)
#endif
static bool file_media(void *ctx, uint32_t *sectors, uint32_t *generation) {
    (void)ctx;
    chMtxLock(&lock);
    *sectors    = bridge.blocks;
    *generation = bridge.generation;
    bool ready  = bridge.attached && media_ready(&MSBLKD[0]) && file_state == FILE_RUNNING && file_job.deliver;
    chMtxUnlock(&lock);
    return ready;
}
static bool file_read_sector(void *ctx, uint32_t lba, uint8_t bytes[512]) {
    (void)ctx;
    HOST_PHASE(MY2K_HOST_FILE_READ);
    bool ok = my2k_block_read10(&bridge, lba, 0, bytes, 512) == 512;
    HOST_PHASE(MY2K_HOST_FILE);
    return ok;
}
#if MY2K_STOCK_PARITY_PROFILE
static bool file_write_sector(void *ctx, uint32_t lba, const uint8_t bytes[512]) {
    (void)ctx;
    uint32_t count, epoch;
    if (!bytes || !usbhmsdLUNWriteProfile(&MSBLKD[0]) || !file_media(NULL, &count, &epoch) || lba >= count)
        return false;
    HOST_PHASE(MY2K_HOST_FILE_WRITE);
    bool written = usbhmsdLUNWrite(&MSBLKD[0], lba, bytes, 1) == HAL_SUCCESS;
    HOST_PHASE(MY2K_HOST_FILE);
    if (!written)
        return false;
    uint32_t current_count, current_epoch;
    return file_media(NULL, &current_count, &current_epoch) && current_epoch == epoch && current_count == count;
}
static bool file_sync(void *ctx) {
    (void)ctx;
    uint32_t count, epoch;
    if (!usbhmsdLUNWriteProfile(&MSBLKD[0]) || !file_media(NULL, &count, &epoch))
        return false;
    HOST_PHASE(MY2K_HOST_FILE_SYNC);
    bool ok = usbhmsdLUNSync(&MSBLKD[0]) == HAL_SUCCESS;
    HOST_PHASE(MY2K_HOST_FILE);
    return ok;
}
#endif
static THD_WORKING_AREA(host_stack, 2048);
static THD_WORKING_AREA(storage_stack, 4096);
static bool media_ready(void *ctx) {
#if MY2K_STOCK_PARITY_PROFILE
    if (usbh_lld_fault(&USBHD1))
        return false;
#endif
    return blkGetDriverState((USBHMassStorageLUNDriver *)ctx) == BLK_READY;
}
static bool media_read(void *ctx, uint32_t lba, uint8_t *dst, uint32_t count) {
#if MY2K_DEV15_PROFILE
    uint32_t began = my2k_perf_clock();
    bool     ok    = usbhmsdLUNRead(ctx, lba, dst, count) == HAL_SUCCESS;
    my2k_perf_record(MY2K_PERF_STORAGE_USB, began);
    return ok;
#else
    return usbhmsdLUNRead(ctx, lba, dst, count) == HAL_SUCCESS;
#endif
}
static THD_FUNCTION(host_task, arg) {
    (void)arg;
    chRegSetThreadName("my2k-usbh");
    for (;;) {
#if MY2K_DEV10_PROFILE
        my2k_host_trace_loop(true);
#endif
#if MY2K_DEV12_PROFILE
        my2k_host_events_pass(true);
#endif
        usbhMainLoop(&USBHD1);
#if MY2K_DEV12_PROFILE
        my2k_host_events_pass(false);
#endif
#if MY2K_DEV10_PROFILE
        my2k_host_trace_loop(false);
#endif
        chThdSleepMilliseconds(1);
    }
}
#if MY2K_ANIMATION_PROFILE
/* AFS1 uses EP0 on the already identity-gated AUX. One worker owns both
 * control and MSC transfers. No FatFs/raw sector writes are enabled in Main. */
__attribute__((noinline, noclone)) static void animation_file_process(uint8_t report[32]) {
    USBH_DEFINE_BUFFER(uint8_t q[32]);
    memcpy(q, report, 32);
    memset(report, 0, 32);
    report[0] = 253;
    report[1] = 238;
    report[2] = 170;
    report[3] = 1;
    report[4] = q[2];
    report[5] = q[4];
    report[6] = 10;
    if (animation_epoch != bridge.generation) {
        animation_peer  = false;
        animation_epoch = bridge.generation;
    }
    if (negotiated.version != 24 || !media_ready(&MSBLKD[0]) || (q[2] && !animation_peer))
        return;
    USBH_DEFINE_BUFFER(usbh_control_request_t out) = {0x40, 0x51, 0, 0, 32};
    USBH_DEFINE_BUFFER(usbh_control_request_t in)  = {0xc0, 0x52, 0, 0, 32};
    uint32_t       actual                          = 0;
    usbh_device_t *device                          = &USBHD1.rootport.device;
    if (q[2] >= 3)
        files_changed = true;
    if (usbhControlRequestExtended(device, &out, q, &actual, TIME_MS2I(500)) != USBH_URBSTATUS_OK || actual != 32) {
        if (q[2] >= 3 && q[2] <= 6)
            animation_upload = true;
        return;
    }
    systime_t began = chVTGetSystemTimeX();
    do {
        if (usbhControlRequestExtended(device, &in, report, &actual, TIME_MS2I(500)) != USBH_URBSTATUS_OK ||
            actual != 32)
            break;
        if (report[0] != 253 || report[1] != 238 || report[2] != 170 || report[3] != 1 || report[4] != q[2] ||
            report[5] != q[4])
            break;
        if (report[6] != 1) {
            if (report[6] == 2) {
                if (q[2] == 0) {
                    animation_peer   = !memcmp(report + 12, "AFS1", 4) && report[16] == 10 && report[17] == 20;
                    animation_upload = report[19] != 0;
                }
                if (q[2] == 3)
                    animation_upload = true;
                if (q[2] == 5 || q[2] == 6)
                    animation_upload = false;
            }
            /* Invalidate legacy Main FatFs after any possible mutation. */
            if (q[2] >= 3)
                files_changed = true;
            return;
        }
        chThdSleepMilliseconds(1);
    } while (chVTTimeElapsedSinceX(began) < TIME_MS2I(1800));
    /* Ambiguous mutation is never retried and keeps the transition interlock. */
    if (q[2] >= 3 && q[2] <= 6)
        animation_upload = true;
    memset(report, 0, 32);
    report[0] = 253;
    report[1] = 238;
    report[2] = 170;
    report[3] = 1;
    report[4] = q[2];
    report[5] = q[4];
    report[6] = 10;
}
#endif
/* AFB1: large control transfers on the exact identity-gated peer. This
 * unique worker owns the borrowed buffer until external delivery retires it. */
static uint32_t afb_word(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void afb_reply(uint8_t op, uint8_t token, uint8_t status, uint32_t offset) {
    memset(fast_buffer, 0, 32);
    fast_buffer[0] = 253;
    fast_buffer[1] = 238;
    fast_buffer[2] = 170;
    fast_buffer[3] = 2;
    fast_buffer[4] = op;
    fast_buffer[5] = token;
    fast_buffer[6] = status;
    for (unsigned i = 0; i < 4; i++) {
        fast_buffer[8 + i] = (uint8_t)(offset >> (8 * i));
    }
    fast_size = 32;
}
static uint8_t bulk_wait(uint32_t offset, bool drain, uint8_t slot) {
    if (!drain && bulk_credits)
        return offset == bulk_next ? 2 : 3;
    USBH_DEFINE_BUFFER(uint8_t status[32]);
    USBH_DEFINE_BUFFER(usbh_control_request_t get) = {0xc0, 0x55, 0, 0, 32};
    systime_t began                                = chVTGetSystemTimeX();
    uint32_t  got;
    do {
        if (usbhControlRequestExtended(&USBHD1.rootport.device, &get, status, &got, TIME_MS2I(500)) !=
                USBH_URBSTATUS_OK ||
            got != 32 || memcmp(status, "BSQ1", 4) || !status[4] || status[6] > 2 || status[7] > 2 ||
            status[16] != slot)
            return 10;
        if (status[5])
            return status[5];
        if (!drain && afb_word(status + 12) != offset)
            return 3;
        if (drain ? !status[6] : status[7]) {
            bulk_credits = status[7];
            return 2;
        }
        chThdSleepMilliseconds(1);
    } while (chVTTimeElapsedSinceX(began) < TIME_MS2I(5000));
    return 10;
}
__attribute__((noinline, noclone)) static void animation_fast_process(void) {
    uint8_t  op = fast_buffer[2], token = fast_buffer[4];
    uint32_t actual                                 = 0;
    uint32_t relay_started                          = timer_read32();
    USBH_DEFINE_BUFFER(usbh_control_request_t out)  = {0x40, 0x53, 0, 0, fast_size};
    USBH_DEFINE_BUFFER(usbh_control_request_t in)   = {0xc0, 0x54, 0, 0, 4096};
    USBH_DEFINE_BUFFER(usbh_control_request_t poll) = {0xc0, 0x56, 0, 0, 32};
    uint8_t        read_slot                        = fast_buffer[5];
    uint32_t       read_at                          = afb_word(fast_buffer + 8);
    unsigned       read_n                           = (unsigned)fast_buffer[30] | (unsigned)fast_buffer[31] << 8;
    usbh_device_t *device                           = &USBHD1.rootport.device;
    if (negotiated.version != 24 || !media_ready(&MSBLKD[0]))
        goto uncertain;
    if (fast_size < 32 || fast_buffer[0] != 253 || fast_buffer[1] != 238 || fast_buffer[3] != 2 || !token) {
        afb_reply(op, token, 3, 0);
        return;
    }
    if (bulk_direct_peer && op == 2) {
        if (fast_size != 32 || fast_buffer[6] || fast_buffer[7] || !read_n || read_n > 4096 || read_slot > 10) {
            afb_reply(op, token, 3, read_at);
            return;
        }
        if (!my2k_file_direct_read(&MSBLKD[0], fast_buffer + 32, read_n, read_slot, token, read_at))
            goto uncertain;
        afb_reply(op, token, 2, read_at);
        fast_buffer[30]  = (uint8_t)read_n;
        fast_buffer[31]  = (uint8_t)(read_n >> 8);
        fast_size        = (uint16_t)(32u + read_n);
        uint32_t elapsed = timer_read32() - relay_started;
        for (unsigned i = 0; i < 4; i++)
            fast_buffer[24 + i] = (uint8_t)(elapsed >> (8 * i));
        return;
    }
    if (bulk_session && op == 4) {
        unsigned n    = (unsigned)fast_buffer[30] | (unsigned)fast_buffer[31] << 8;
        uint8_t  slot = fast_buffer[5], codec = fast_buffer[7];
        uint32_t at      = afb_word(fast_buffer + 8);
        unsigned logical = codec ? ((unsigned)fast_buffer[28] | (unsigned)fast_buffer[29] << 8) : n;
        if (!n || n > 4096 || fast_size != 32u + n || fast_buffer[6] || codec > 1 || !logical || logical > 4096 ||
            n > logical || (codec && !bulk_rle_peer) || (!codec && (fast_buffer[28] || fast_buffer[29]))) {
            afb_reply(op, token, 3, 0);
            return;
        }
        uint8_t okay = bulk_wait(at, false, slot);
        if (okay != 2) {
            afb_reply(op, token, okay, 0);
            return;
        }
        if (!my2k_file_bulk_write(&MSBLKD[0], fast_buffer + 32, n, logical, codec, slot, token, at))
            goto uncertain;
        bulk_credits--;
        bulk_next = at + logical;
        afb_reply(op, token, 2, at + logical);
        fast_buffer[7]   = 2;
        uint32_t elapsed = timer_read32() - relay_started;
        for (unsigned i = 0; i < 4; i++) {
            fast_buffer[24 + i] = (uint8_t)(elapsed >> (8 * i));
        }
        files_changed = true;
        return; /* accepted, completion checked before commit */
    }
    if ((bulk_peer && op == 3) || (bulk_session && (op == 5 || op == 6))) {
        USBH_DEFINE_BUFFER(uint8_t q[32]);
        memcpy(q, fast_buffer, 32);
        if (op != 3) {
            uint8_t okay = bulk_wait(0, true, q[5]);
            if (okay != 2 && !(op == 6 && okay != 10)) {
                afb_reply(op, token, okay, 0);
                return;
            }
        }
        q[3]  = 1;
        q[7]  = op == 3 ? 4 : 0;
        q[27] = op == 3 ? 0xb2 : 0;
        animation_file_process(q);
        memcpy(fast_buffer, q, 32);
        fast_buffer[3]  = 2;
        fast_buffer[30] = fast_buffer[31] = 0;
        fast_size                         = 32;
        if (q[6] == 2) {
            if (op == 3) {
                bulk_session = true;
                bulk_credits = 2;
                bulk_next    = 0;
            } else {
                bulk_session = false;
                bulk_credits = 0;
            }
        }
        return;
    }
    if (fast_size > 4096) {
        afb_reply(op, token, 3, 0);
        return;
    }
    if (op >= 3)
        files_changed = true;
    if (usbhControlRequestExtended(device, &out, fast_buffer, &actual, TIME_MS2I(1000)) != USBH_URBSTATUS_OK ||
        actual != fast_size)
        goto uncertain;
    systime_t began = chVTGetSystemTimeX();
    do {
        if (usbhControlRequestExtended(device, (op == 2 && bulk_read_peer) ? &poll : &in, fast_buffer, &actual,
                                       TIME_MS2I(1000)) != USBH_URBSTATUS_OK ||
            actual < 32 || actual > 4096)
            goto uncertain;
        if (op == 2 && bulk_read_peer && fast_buffer[6] != 1) {
            if (fast_buffer[6] == 2) {
                if (!my2k_file_bulk_read(&MSBLKD[0], fast_buffer, read_n, read_slot, token, read_at))
                    goto uncertain;
                actual = 32u + read_n;
            } else if (usbhControlRequestExtended(device, &in, fast_buffer, &actual, TIME_MS2I(1000)) !=
                           USBH_URBSTATUS_OK ||
                       actual != 32)
                goto uncertain;
        }
        if (fast_buffer[0] != 253 || fast_buffer[1] != 238 || fast_buffer[2] != 170 || fast_buffer[3] != 2 ||
            fast_buffer[4] != op || fast_buffer[5] != token)
            goto uncertain;
        if (fast_buffer[6] != 1) {
            unsigned n = (unsigned)fast_buffer[30] | (unsigned)fast_buffer[31] << 8;
            if (n > 4064 || actual != 32 + n)
                goto uncertain;
            fast_size = (uint16_t)actual;
            if (!op && fast_buffer[6] == 2) {
                bulk_peer        = !memcmp(fast_buffer + 12, "AFB1", 4) && (fast_buffer[22] & 4);
                bulk_read_peer   = bulk_peer && (fast_buffer[22] & 8);
                bulk_rle_peer    = bulk_peer && (fast_buffer[22] & 16);
                bulk_direct_peer = bulk_peer && (fast_buffer[22] & 128);
                if (bulk_direct_peer && my2k_usb_animation_bulk_available())
                    fast_buffer[23] |= 1;
                if (bulk_peer) {
                    memcpy(fast_buffer + 12,
                           my2k_usb_animation_bulk_available() ? (bulk_rle_peer ? "AFB7" : "AFB6") : "AFB2", 4);
                    fast_buffer[20] = 0;
                    fast_buffer[21] = 16;
                }
            }
            if (op) {
                uint32_t elapsed = timer_read32() - relay_started;
                for (unsigned i = 0; i < 4; i++)
                    fast_buffer[24 + i] = (uint8_t)(elapsed >> (i * 8));
            }
            if (fast_buffer[6] == 2) {
                if (op == 3)
                    animation_upload = true;
                if (op == 5 || op == 6)
                    animation_upload = false;
            }
            return;
        }
        chThdSleepMilliseconds(1);
    } while (chVTTimeElapsedSinceX(began) < TIME_MS2I(15000));
uncertain:
    if (op >= 3 && op <= 6)
        animation_upload = true;
    memset(fast_buffer, 0, 32);
    fast_buffer[0] = 253;
    fast_buffer[1] = 238;
    fast_buffer[2] = 170;
    fast_buffer[3] = 2;
    fast_buffer[4] = op;
    fast_buffer[5] = token;
    fast_buffer[6] = 10;
    fast_size      = 32;
}
__attribute__((noinline, noclone, externally_visible)) void my2k_usbh_fast_wakeI(void) {
    if (started)
        chBSemSignalI(&work_ready);
}
static THD_FUNCTION(storage_task, arg) {
    (void)arg;
    chRegSetThreadName("my2k-media");
    USBHMassStorageLUNDriver  *lun        = &MSBLKD[0];
    const my2k_main_fs_backend filesystem = {.media       = file_media,
                                             .read_sector = file_read_sector
#if MY2K_STOCK_PARITY_PROFILE
                                             ,
                                             .write_sector = file_write_sector,
                                             .sync         = file_sync
#endif
    };
    my2k_main_fs_init(&filesystem);
    for (;;) {
        my2k_usb_animation_service();
        HOST_PHASE(MY2K_HOST_IDLE);
        if (
#if MY2K_STOCK_PARITY_PROFILE
            !usbh_lld_fault(&USBHD1) &&
#endif
            blkGetDriverState(lun) == BLK_ACTIVE) {
            HOST_PHASE(MY2K_HOST_CONNECT);
#if MY2K_DEV6_PROFILE
            connect_attempts++;
            bool result         = usbhmsdLUNConnect(lun);
            last_connect_result = (uint8_t)result;
            connect_returns++;
            if (result != HAL_SUCCESS)
                connect_failures++;
#else
            (void)usbhmsdLUNConnect(lun);
#endif
            HOST_PHASE(MY2K_HOST_IDLE);
        }
        chMtxLock(&lock);
        if (!media_ready(lun)) {
            if (bridge.attached) {
                my2k_block_detach(&bridge);
                my2k_main_fs_reset();
                disconnected = true;
#if MY2K_DEV15_PROFILE
                negotiated = (my2k_storage_caps){0, 0};
#endif
            }
            if (state == PENDING) {
                request.result = -1;
                state          = DONE;
            }
        } else if (!bridge.attached) {
            chMtxUnlock(&lock);
            BlockDeviceInfo info;
            HOST_PHASE(MY2K_HOST_INFO);
            bool valid = usbhmsdLUNGetInfo(lun, &info) == HAL_SUCCESS;
#if MY2K_DEV15_PROFILE
            my2k_usbh_identity id;
            my2k_usbh_get_identity(&id);
            negotiated =
                my2k_storage_negotiate(id.vendor_id, id.product_id, id.device_valid, id.inquiry_valid, id.inquiry);
            valid = valid && negotiated.version;
#    if MY2K_RELEASE1_PROFILE
            /* Keep shared HAL untouched. Its existing cached write gate is
             * armed only for our exact R001 identity, never a legacy AUX. */
            if (valid && negotiated.version == 24)
                lun->my2k_write_profile = true;
#    endif
#endif
            HOST_PHASE(MY2K_HOST_IDLE);
            chMtxLock(&lock);
            if (valid && media_ready(lun)) {
                my2k_block_ops ops = {lun, media_ready, media_read};
                (void)my2k_block_attach(&bridge, &ops, info.blk_num, (uint16_t)info.blk_size);
                my2k_main_fs_reset();
            }
        }
        if (state == PENDING && bridge.attached) {
            state = RUNNING;
#if MY2K_DEV15_PROFILE
            my2k_perf_record(MY2K_PERF_STORAGE_QUEUE, queued_cycles);
#endif
            uint32_t lba    = request.lba;
            uint16_t offset = request.offset, size = request.size;
            chMtxUnlock(&lock);
            HOST_PHASE(MY2K_HOST_BLOCK_READ);
            int32_t result;
#if MY2K_RELEASE1_PROFILE
            if (request_kind) {
                bool allowed = negotiated.version == 24 && my2k_asset_export_active() && my2k_oled_export_owned() &&
                               usbhmsdLUNWriteProfile(lun);
                if (request_kind == 2 && negotiated.version != 24)
                    result = 0; /* Read-only peers have no writes to flush. */
                else if (!allowed)
                    result = -1;
                else if (request_kind == 1)
                    result = usbhmsdLUNWrite(lun, lba, request.bytes, 1) == HAL_SUCCESS ? 512 : -1;
                else
                    result = usbhmsdLUNSync(lun) == HAL_SUCCESS ? 0 : -1;
            } else
#endif
                result = my2k_block_read10(&bridge, lba, offset, request.bytes, size);
            HOST_PHASE(MY2K_HOST_IDLE);
            chMtxLock(&lock);
            request.result = result;
#if MY2K_DEV15_PROFILE
            read_requests++;
            if (result > 0)
                read_bytes += (uint32_t)result;
            else
                read_failures++;
#endif
            state = DONE;
        }
        if (file_state == FILE_PENDING) {
            uint8_t report[32];
            memcpy(report, file_job.request, 32);
            file_state = FILE_RUNNING;
            chMtxUnlock(&lock);
            HOST_PHASE(MY2K_HOST_FILE);
#if MY2K_ANIMATION_PROFILE
            if (file_fast)
                animation_fast_process();
            else if (report[0] == 253 && report[1] == 238)
                animation_file_process(report);
            else
#endif
                if (!my2k_main_fs_process(report, sizeof report))
                report[2] = 0x55;
#if MY2K_STOCK_PARITY_PROFILE
            /* A cancelled file operation may have completed a physical sector.
             * Retire its write lease after the worker leaves FatFs, never by
             * pretending that an in-flight transaction was undone. */
            chMtxLock(&lock);
            bool cancelled = !file_job.deliver;
            chMtxUnlock(&lock);
            if (cancelled) {
                HOST_PHASE(MY2K_HOST_CANCEL_SYNC);
                if (media_ready(lun))
                    (void)usbhmsdLUNSync(lun);
                my2k_main_fs_reset();
            }
#endif
            chMtxLock(&lock);
            if (file_job.deliver) {
                if (!file_fast)
                    memcpy(file_job.response, report, 32);
                file_state = FILE_DONE;
            } else {
                file_state = FILE_IDLE;
                file_fast  = false;
            }
        }
#if MY2K_RELEASE1_PROFILE
        if (files_changed && file_state != FILE_RUNNING) {
            files_changed = false;
            my2k_main_fs_reset();
        }
#endif
        chMtxUnlock(&lock);
        my2k_usb_animation_service();
        HOST_PHASE(MY2K_HOST_IDLE);
#if MY2K_DEV15_PROFILE
        (void)chBSemWaitTimeout(&work_ready, TIME_MS2I(2));
#else
        chThdSleepMilliseconds(2);
#endif
    }
}
bool my2k_usbh_start(void) {
    if (started)
        return true;
#if MY2K_STOCK_PARITY_PROFILE
    if (start_attempted)
        return false;
    start_attempted = true;
#endif
    chMtxObjectInit(&lock);
#if MY2K_DEV15_PROFILE
    chBSemObjectInit(&work_ready, true);
#endif
    my2k_block_init(&bridge);
    state = IDLE;
    /* Recovered internal FS core pin function. Supply/reset pins deliberately not
     * assigned: those board-level dependencies must be validated independently. */
    palSetPadMode(GPIOA, 11, PAL_MODE_ALTERNATE(10));
    palSetPadMode(GPIOA, 12, PAL_MODE_ALTERNATE(10));
    usbhStart(&USBHD1);
#if MY2K_STOCK_PARITY_PROFILE
    if (usbh_lld_fault(&USBHD1))
        return false;
#endif
#if MY2K_DEV9_PROFILE
    my2k_dev9_host_discovery();
#endif
#if MY2K_DEV10_PROFILE
    my2k_host_trace_service(timer_read32());
#endif
#if MY2K_DEV11_PROFILE
    thread_t *host = chThdCreateStatic(host_stack, sizeof(host_stack), NORMALPRIO, host_task, NULL);
    my2k_scheduler_workers(host, NULL);
    if (!host)
        return false;
    thread_t *media = chThdCreateStatic(storage_stack, sizeof(storage_stack), NORMALPRIO + 1, storage_task, NULL);
    my2k_scheduler_workers(host, media);
    if (!media)
        return false;
#else
    if (!chThdCreateStatic(host_stack, sizeof(host_stack), NORMALPRIO, host_task, NULL))
        return false;
    if (!chThdCreateStatic(storage_stack, sizeof(storage_stack), NORMALPRIO + 1, storage_task, NULL))
        return false;
#endif
    started = true;
    return true;
}
bool my2k_usbh_capacity(uint32_t *blocks, uint16_t *size) {
    if (!blocks || !size)
        return false;
    *blocks = 0;
    *size   = 0;
    if (!started)
        return false;
    chMtxLock(&lock);
    bool ok = bridge.attached && bridge.blocks && media_ready(&MSBLKD[0]);
    if (ok) {
        *blocks = bridge.blocks;
        *size   = MY2K_BLOCK_SIZE;
    }
    chMtxUnlock(&lock);
    return ok;
}
bool my2k_usbh_submit_read(uint32_t lba, uint16_t offset, uint16_t size) {
    if (!started || !size || size > MY2K_BRIDGE_MAX_TRANSFER || offset >= 512)
        return false;
#if MY2K_DEV15_PROFILE
    if (my2k_asset_transition_pending())
        return false;
#endif
    chMtxLock(&lock);
    bool ok = state == IDLE && bridge.attached;
    if (ok) {
#if MY2K_RELEASE1_PROFILE
        request_kind = 0;
#endif
        request.lba    = lba;
        request.offset = offset;
        request.size   = size;
        state          = PENDING;
#if MY2K_DEV15_PROFILE
        queued_cycles = my2k_perf_clock();
        chBSemSignal(&work_ready);
#endif
    }
    chMtxUnlock(&lock);
    return ok;
}
bool my2k_usbh_take_read(void *dst, size_t cap, int32_t *result) {
    if (!started || !result)
        return false;
    chMtxLock(&lock);
    bool ok = state == DONE;
    if (ok && request.result > 0 && (!dst || cap < (size_t)request.result))
        ok = false;
    if (ok) {
        *result = request.result;
        if (request.result > 0)
            memcpy(dst, request.bytes, (size_t)request.result);
        state = IDLE;
    }
    chMtxUnlock(&lock);
    return ok;
}

bool my2k_usbh_take_disconnect(void) {
    if (!started)
        return false;
    chMtxLock(&lock);
    bool value   = disconnected;
    disconnected = false;
    chMtxUnlock(&lock);
    return value;
}

/* Bind the independently tested SCSI service to the real asynchronous USB host
 * worker. One owner must use this queue; no USB endpoint/BOT binding is implied. */
static bool scsi_capacity(void *context, uint32_t *blocks, uint16_t *size, uint32_t *generation) {
    (void)context;
    if (!started || !blocks || !size || !generation)
        return false;
    chMtxLock(&lock);
    bool ok = bridge.attached && bridge.blocks && media_ready(&MSBLKD[0]);
#if MY2K_RELEASE1_PROFILE
    if (negotiated.version == 24 && !my2k_oled_export_owned())
        ok = false;
#endif
    *blocks     = ok ? bridge.blocks : 0;
    *size       = ok ? MY2K_BLOCK_SIZE : 0;
    *generation = bridge.generation;
    chMtxUnlock(&lock);
    return ok;
}
static bool scsi_submit(void *context, uint32_t lba, uint16_t bytes) {
    (void)context;
    return my2k_usbh_submit_read(lba, 0, bytes);
}
#if MY2K_DEV15_PROFILE
static uint16_t scsi_limit(void *context) {
    (void)context;
    chMtxLock(&lock);
    uint16_t n = negotiated.read_bytes;
    chMtxUnlock(&lock);
    return n;
}
#endif
static bool scsi_take(void *context, void *dst, size_t size, int32_t *result) {
    (void)context;
    return my2k_usbh_take_read(dst, size, result);
}
#if MY2K_RELEASE1_PROFILE
static bool scsi_writable(void *ctx) {
    (void)ctx;
    return started && my2k_asset_export_active() && !my2k_asset_transition_pending() && my2k_oled_export_owned() &&
           negotiated.version == 24 && usbhmsdLUNWriteProfile(&MSBLKD[0]);
}
static bool scsi_queue_mutation(unsigned kind, uint32_t lba, const uint8_t *data) {
    if ((kind == 1 && !scsi_writable(NULL)) || (kind == 2 && (!started || !my2k_asset_export_active())))
        return false;
    chMtxLock(&lock);
    bool okay = state == IDLE && bridge.attached && (kind == 2 || lba < bridge.blocks);
    if (okay) {
        request_kind   = kind;
        request.lba    = lba;
        request.offset = 0;
        request.size   = kind == 1 ? 512 : 0;
        if (data)
            memcpy(request.bytes, data, 512);
        queued_cycles = my2k_perf_clock();
        state         = PENDING;
        chBSemSignal(&work_ready);
    }
    chMtxUnlock(&lock);
    return okay;
}
static bool scsi_submit_write(void *ctx, uint32_t lba, const uint8_t data[512]) {
    (void)ctx;
    return scsi_queue_mutation(1, lba, data);
}
static bool scsi_submit_sync(void *ctx) {
    (void)ctx;
    return scsi_queue_mutation(2, 0, NULL);
}
#endif
void my2k_usbh_scsi_init(my2k_scsi *service) {
    const my2k_scsi_ops ops = {.capacity = scsi_capacity,
                               .submit   = scsi_submit,
                               .take     = scsi_take
#if MY2K_RELEASE1_PROFILE
                               ,
                               .writable     = scsi_writable,
                               .submit_write = scsi_submit_write,
                               .submit_sync  = scsi_submit_sync
#endif
#if MY2K_DEV15_PROFILE
                               ,
                               .read_limit = scsi_limit
#endif
    };
    my2k_scsi_init(service, &ops);
}

bool my2k_usbh_file_request(const uint8_t report[32]) {
    if (!started || !report || my2k_usb_animation_transfer_busy())
        return false;
#if MY2K_DEV7_PROFILE
    if (!my2k_asset_file_access_allowed())
        return false;
#    if MY2K_ANIMATION_PROFILE
    if (my2k_oled_main_busy())
        return false;
#    endif
    if (animation_upload && report[1] != 238)
        return false;
    /* Dev7 isolates the unresolved AUX read instability: never mutate
     * external assets, even with an older write-capable paired AUX. */
    if (report[1] == 38 || report[1] == 41 || (report[1] == 37 && report[3] == 0))
        return false;
#endif
    chMtxLock(&lock);
    bool ok = false;
    if (file_fast) {
        chMtxUnlock(&lock);
        return false;
    }
    if ((file_state == FILE_PENDING || file_state == FILE_RUNNING || file_state == FILE_DONE) && file_job.deliver &&
        !memcmp(file_job.request, report, 32))
        ok = true;
    else if (file_state == FILE_RUNNING) {
        file_job.deliver = false;
    }
#if MY2K_DEV6_PROFILE
    else if (!bridge.attached || !media_ready(&MSBLKD[0])) {
        /* A connect/abort worker can be sleeping while AUX is unavailable.
         * Refuse NEW work immediately; coalesced in-flight work above keeps
         * its existing bounded HID deadline even during transient LUN I/O. */
        ok = false;
    }
#endif
    else {
        memcpy(file_job.request, report, 32);
        file_job.deliver = true;
        file_state       = FILE_PENDING;
        ok               = true;
    }
    chMtxUnlock(&lock);
    if (ok)
        chBSemSignal(&work_ready);
    return ok;
}
bool my2k_usbh_file_take_reply(uint8_t report[32]) {
    if (!started || !report)
        return false;
    chMtxLock(&lock);
    bool ready = !file_fast && file_state == FILE_DONE && file_job.deliver;
    if (ready) {
        memcpy(report, file_job.response, 32);
        file_state       = FILE_IDLE;
        file_job.deliver = false;
    }
    chMtxUnlock(&lock);
    return ready;
}
void my2k_usbh_file_cancel_reply(void) {
    if (!started)
        return;
    chMtxLock(&lock);
    if (!file_fast) {
        file_job.deliver = false;
        if (file_state == FILE_PENDING || file_state == FILE_DONE)
            file_state = FILE_IDLE;
    }
    chMtxUnlock(&lock);
}

bool my2k_usbh_fast_request(uint8_t *buffer, uint16_t size) {
    if (!started || !buffer || size < 32 || size > 4128 || !my2k_asset_file_access_allowed() || my2k_oled_main_busy())
        return false;
    chMtxLock(&lock);
    bool ok = file_state == FILE_IDLE && state == IDLE && bridge.attached && media_ready(&MSBLKD[0]);
    if (ok) {
        fast_buffer      = buffer;
        fast_size        = size;
        file_fast        = true;
        file_job.deliver = true;
        file_state       = FILE_PENDING;
        memcpy(file_job.request, buffer, 32);
        chBSemSignal(&work_ready);
    }
    chMtxUnlock(&lock);
    return ok;
}
bool my2k_usbh_fast_done(uint16_t *size) {
    if (!started)
        return false;
    chMtxLock(&lock);
    bool ready = file_fast && file_state == FILE_DONE;
    if (ready)
        *size = fast_size;
    chMtxUnlock(&lock);
    return ready;
}
bool my2k_usbh_fast_finish(bool cancel) {
    if (!started)
        return true;
    chMtxLock(&lock);
    if (file_fast) {
        if (cancel)
            file_job.deliver = false;
        if (file_state != FILE_RUNNING) {
            file_state       = FILE_IDLE;
            file_job.deliver = false;
            file_fast        = false;
        }
    }
    bool released = !file_fast;
    chMtxUnlock(&lock);
    return released;
}
uint32_t my2k_usbh_fault(void) {
#if MY2K_STOCK_PARITY_PROFILE
    return usbh_lld_fault(&USBHD1);
#else
    return 0;
#endif
}

bool my2k_usbh_write_profile_ready(void) {
#if MY2K_STOCK_PARITY_PROFILE
    if (!started)
        return false;
    chMtxLock(&lock);
    bool ready = bridge.attached && !usbh_lld_fault(&USBHD1) && usbhmsdLUNWriteProfile(&MSBLKD[0]);
    chMtxUnlock(&lock);
    return ready;
#else
    return false;
#endif
}

void my2k_usbh_get_diagnostics(my2k_usbh_diagnostics *out) {
    if (!out)
        return;
    memset(out, 0, sizeof *out);
#if MY2K_DEV6_PROFILE
    /* Brief scheduler lock, no mutex/semaphore wait and no peripheral access.
     * Snapshot works even when the storage thread is suspended in USB I/O. */
    chSysLock();
    out->flags        = (started ? 1u : 0u) | (bridge.attached ? 2u : 0u) | (MSBLKD[0].my2k_write_profile ? 4u : 0u) |
                        (file_job.deliver ? 8u : 0u) | (start_attempted ? 16u : 0u);
    out->host_state   = (uint8_t)USBHD1.status;
    out->lun_state    = (uint8_t)blkGetDriverState(&MSBLKD[0]);
    out->worker_phase = worker_phase;
    out->blocks       = bridge.blocks;
    out->generation   = bridge.generation;
    out->connect_attempts    = connect_attempts;
    out->connect_returns     = connect_returns;
    out->connect_failures    = connect_failures;
    out->read_state          = (uint8_t)state;
    out->file_state          = (uint8_t)file_state;
    out->last_connect_result = last_connect_result;
    out->file_command        = file_job.request[1];
    chSysUnlock();
#endif
}

bool my2k_usbh_asset_idle(void) {
    if (!started)
        return true;
    chMtxLock(&lock);
    bool idle = state == IDLE && file_state == FILE_IDLE && !animation_upload && !my2k_usb_animation_transfer_busy();
#if MY2K_DEV7_PROFILE
    if (idle)
        idle = !my2k_main_fs_has_open_writers();
#endif
    chMtxUnlock(&lock);
    return idle;
}
void my2k_usbh_get_identity(my2k_usbh_identity *out) {
    if (!out)
        return;
    memset(out, 0, sizeof *out);
#if MY2K_DEV7_PROFILE
    usbhmsd_cached_identity_t cached;
    chSysLock();
    usbhmsdLUNCachedIdentity(&MSBLKD[0], &cached);
    chSysUnlock();
    memcpy(out->inquiry, cached.inquiry, sizeof out->inquiry);
    out->vendor_id     = cached.vendor_id;
    out->product_id    = cached.product_id;
    out->device_valid  = cached.device_valid;
    out->inquiry_valid = cached.inquiry_valid;
#endif
}

#if MY2K_DEV15_PROFILE
bool my2k_usbh_performance_page(uint8_t index, uint8_t p[32]) {
    if (index)
        return false;
    chSysLock();
    memset(p + 4, 0, 28);
    p[4]                   = 1;
    p[5]                   = negotiated.version;
    p[6]                   = (uint8_t)negotiated.read_bytes;
    p[7]                   = (uint8_t)(negotiated.read_bytes >> 8);
    const uint32_t words[] = {read_requests, read_bytes, read_failures, bridge.generation, bridge.blocks};
    for (unsigned i = 0; i < 5; i++)
        for (unsigned b = 0; b < 4; b++)
            p[8 + 4 * i + b] = (uint8_t)(words[i] >> (b * 8));
    p[28] = bridge.attached;
#    if MY2K_RELEASE1_PROFILE
    p[29] = !(negotiated.version == 24 && my2k_asset_export_active() && my2k_oled_export_owned());
#    else
    p[29] = 1;
#    endif
    chSysUnlock();
    return true;
}
#endif

bool my2k_usbh_animation_busy(void) {
    if (my2k_usb_animation_transfer_busy())
        return true;
    if (!started)
        return false;
    chMtxLock(&lock);
    bool busy = animation_upload || (file_state != FILE_IDLE && file_job.request[1] == 238);
    chMtxUnlock(&lock);
    return busy;
}
