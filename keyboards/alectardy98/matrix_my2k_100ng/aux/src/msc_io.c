/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_msc.h"
#if MY2K_RELEASE1_PROFILE
#    include "aux_oled_store.h"
static bool export_owned;
bool        aux_msc_export_active(void) {
    return export_owned;
}
void aux_msc_set_export(bool on) {
    export_owned = on;
}
#endif
#include "aux_platform.h"
#include "aux_msc_limits.h"
#if MY2K_ANIMATION_PROFILE
#    include "aux_animation_files.h"
static bool    request_file;
static uint8_t file_command[16];
#endif
#include <string.h>
/* The borrowed TinyUSB buffer is touched ONLY by foreground completion. Slow
 * reads use a private worker-owned buffer, which survives yields and reset.
 * Generation cancellation permits a fresh request while an old SD command
 * drains, without copying old data into a new CBW's endpoint buffer. */
enum { IO_IDLE, IO_QUEUED, IO_RUNNING, IO_READY };
static unsigned phase;
static uint32_t generation = 1;
static struct {
    uint8_t  lun;
    uint32_t lba, offset, size;
    void    *destination;
} request;
static uint8_t completed[AUX_MSC_READ_MAX_BYTES];
#if MY2K_ANIMATION_PROFILE
enum { BUFFER_FREE, BUFFER_CONTROL, BUFFER_GAME };
static uint8_t control_borrowed;
uint8_t       *aux_msc_control_acquire(void) {
    if (control_borrowed || !aux_msc_io_idle() || aux_msc_export_active())
        return NULL;
    control_borrowed = BUFFER_CONTROL;
    return completed;
}
void aux_msc_control_release(void) {
    if (control_borrowed == BUFFER_CONTROL)
        control_borrowed = BUFFER_FREE;
}
#endif
#if MY2K_DEV15_PROFILE
/* The unique worker may yield to USB while a reset queues a newer command.
 * Keep its read destination private until the epoch check below. A static
 * buffer avoids putting a4096-byte object on either foreground or worker
 * stack; only this single worker ever writes it. */
static uint8_t read_staging[AUX_MSC_READ_MAX_BYTES];
#endif
#if MY2K_ANIMATION_PROFILE && MY2K_DEV15_PROFILE
uint8_t *aux_msc_control_second(void) {
    return control_borrowed == BUFFER_CONTROL ? read_staging : NULL;
}
#endif
#if MY2K_DOOM_PROFILE && MY2K_ANIMATION_PROFILE && MY2K_DEV15_PROFILE
_Static_assert(AUX_MSC_READ_MAX_BYTES >= 4096, "Game cache loan needs the existing4KiB buffers");
bool aux_msc_game_buffers(uint8_t **first, uint8_t **second) {
    if (!first || !second || control_borrowed || !aux_msc_io_idle() || aux_msc_export_active() ||
        aux_msc_media_writing())
        return false;
    control_borrowed = BUFFER_GAME;
    *first           = completed;
    *second          = read_staging;
    return true;
}
void aux_msc_game_release(void) {
    if (control_borrowed == BUFFER_GAME)
        control_borrowed = BUFFER_FREE;
}
#endif
static int32_t result;
#if MY2K_STOCK_PARITY_PROFILE
static bool     request_write, worker_active, write_lease, write_dirty, write_failed;
static uint8_t  write_source[512];
static uint32_t media_generation = 1;
uint32_t        aux_msc_media_generation(void) {
    return media_generation;
}
bool aux_msc_media_writing(void) {
    return write_lease
#    if MY2K_RELEASE1_PROFILE
           || export_owned
#    endif
        ;
}
bool aux_msc_io_idle(void) {
    return phase == IO_IDLE && !worker_active;
}
static void retire_lease(void) {
    if (write_dirty)
        media_generation++;
    write_lease = write_dirty = write_failed = false;
}
bool aux_msc_media_sync(void) {
    if (worker_active || phase != IO_IDLE)
        return false;
    bool okay = !write_failed;
    retire_lease();
    return okay;
}
#endif
uint32_t aux_msc_generation(void) {
    return generation;
}
void aux_msc_cancel(void) {
    generation++;
    phase               = IO_IDLE;
    request.destination = NULL;
#if MY2K_STOCK_PARITY_PROFILE
    if (!worker_active)
        retire_lease();
#endif
}
int32_t aux_msc_queue(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t size) {
    uint32_t sectors = aux_sd_sectors();
    if (
#if MY2K_ANIMATION_PROFILE
        control_borrowed ||
#endif
        phase != IO_IDLE || lun || !buffer || !size || size > sizeof completed || offset >= 512u || lba >= sectors ||
        size > (uint64_t)(sectors - lba) * 512u - offset)
        return -1;
    request.lun         = lun;
    request.lba         = lba;
    request.offset      = offset;
    request.destination = buffer;
    request.size        = size;
#if MY2K_ANIMATION_PROFILE
    request_file = false;
#endif
#if MY2K_STOCK_PARITY_PROFILE
    request_write = false;
#endif
    phase = IO_QUEUED;
    return -2; /* TUD_MSC_RET_ASYNC; never busy-requeue tud_task. */
}
#if MY2K_STOCK_PARITY_PROFILE
int32_t aux_msc_queue_write(uint8_t lun, uint32_t lba, uint32_t offset, const void *buffer, uint32_t size) {
#    if MY2K_DEV7_PROFILE && !MY2K_RELEASE1_PROFILE
    (void)lun;
    (void)lba;
    (void)offset;
    (void)buffer;
    (void)size;
    return -1; /* No write lease/buffer/SD operation is armed by Dev7. */
#    else
#        if MY2K_RELEASE1_PROFILE
    if (!export_owned || aux_oled_store_writing())
        return -1;
#        endif
    uint32_t sectors = aux_sd_sectors();
    if (
#        if MY2K_ANIMATION_PROFILE
        control_borrowed ||
#        endif
        phase != IO_IDLE || lun || !buffer || !size || size > sizeof write_source || offset >= 512 || lba >= sectors ||
        size > (uint64_t)(sectors - lba) * 512u - offset)
        return -1;
    memcpy(write_source, buffer, size);
    request.lun         = lun;
    request.lba         = lba;
    request.offset      = offset;
    request.size        = size;
    request.destination = NULL;
    if (!write_lease) {
        write_lease = true;
        write_dirty = write_failed = false;
    }
    request_write = true;
#        if MY2K_ANIMATION_PROFILE
    request_file = false;
#        endif
    phase = IO_QUEUED;
    return -2;
#    endif
}
#endif
#if MY2K_ANIMATION_PROFILE
int32_t aux_msc_queue_file(const uint8_t cdb[16], void *buffer, uint16_t size) {
    unsigned n = (unsigned)cdb[12] | (unsigned)cdb[13] << 8;
    if (control_borrowed || phase != IO_IDLE || !aux_msc_io_idle() || !buffer || aux_msc_export_active() ||
        aux_msc_media_writing() || cdb[0] != 0xc3 || memcmp(cdb + 1, "AFD", 3) || cdb[4] != 1 || cdb[5] > 10 ||
        !cdb[6] || cdb[7] || cdb[14] || cdb[15] || !n || n > 4096 || size != ((n + 511u) & ~511u))
        return -1;
    memcpy(file_command, cdb, 16);
    request.lun = 0;
    request.lba = request.offset = 0;
    request.size                 = size;
    request.destination          = buffer;
    request_write                = false;
    request_file                 = true;
    phase                        = IO_QUEUED;
    return -2;
}
#endif
void aux_msc_worker_service(void) {
#if MY2K_RELEASE1_PROFILE
    if (aux_oled_store_writing())
        return;
#endif
    if (phase != IO_QUEUED)
        return;
    const uint32_t epoch = generation, lba = request.lba, offset = request.offset, size = request.size;
    const uint8_t  lun = request.lun;
#if MY2K_DEV15_PROFILE
    uint8_t *staging = read_staging;
#else
    uint8_t staging[512];
#endif
    phase = IO_RUNNING;
#if MY2K_STOCK_PARITY_PROFILE
    const bool writing = request_write;
    worker_active      = true;
    if (writing)
        memcpy(staging, write_source, size);
#    if MY2K_ANIMATION_PROFILE
    uint8_t cdb[16];
    memcpy(cdb, file_command, 16);
    const bool file_read = request_file;
    int32_t    n         = file_read ? aux_animation_direct_read(cdb, staging, (uint16_t)size) :
#    else
    int32_t n =
#    endif
                writing ? aux_msc_write(lun, lba, offset, staging, size)
                        : aux_msc_read(lun, lba, offset, staging, size);
    worker_active        = false;
    if (writing) {
        write_dirty = true;
        if (n != (int32_t)size)
            write_failed = true;
    }
    if (epoch != generation) {
        if (!(phase == IO_QUEUED && request_write))
            retire_lease();
        return;
    }
    if (n > 0 && !writing)
        memcpy(completed, staging, (size_t)n);
#else
    int32_t n = aux_msc_read(lun, lba, offset, staging, size);
    if (epoch != generation)
        return;
    if (n > 0)
        memcpy(completed, staging, (size_t)n);
#endif
    result = n;
    phase  = IO_READY;
}
bool aux_msc_take_completion(int32_t *bytes) {
    if (phase != IO_READY || !bytes)
        return false;
    if (result > 0
#if MY2K_STOCK_PARITY_PROFILE
        && !request_write
#endif
    )
        memcpy(request.destination, completed, (size_t)result);
    *bytes              = result;
    request.destination = NULL;
    phase               = IO_IDLE;
    return true;
}
