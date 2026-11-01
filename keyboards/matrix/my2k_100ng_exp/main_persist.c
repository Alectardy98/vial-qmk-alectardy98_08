/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_persist.h"
#include <string.h>
#if MY2K_LOCK_LIGHTS_PROFILE
#include "main_lock_store.h"
#endif
#ifndef MY2K_PERSIST_HOST
#if !MY2K_DEV_PERSISTENCE
#error "Settings flash adapter requires explicit persistence profile"
#endif
#include "hal.h"
#endif
/* Reuse only the manufacturer's settings sector4. No application, loader,
 * staging, option-byte or OTP address is accepted by this adapter. */
#define BASE UINT32_C(0x08010000)
#define END  UINT32_C(0x08020000)
#define SLOT (MY2K_EEPROM_BYTES + 20u)
static my2k_journal log_store;
static _Alignas(4) uint8_t snapshot[MY2K_EEPROM_BYTES], staged[SLOT];
static uint32_t destination, cursor, commits, erases, error;
static bool staging, pending, destination_set, format_needed;

#ifdef MY2K_PERSIST_HOST
extern bool my2k_test_flash_read(uint32_t, void *, size_t);
extern bool my2k_test_flash_word(uint32_t, uint32_t);
extern bool my2k_test_flash_erase4(void);
static bool flash_read(uint32_t a, void *p, size_t n) { return my2k_test_flash_read(a,p,n); }
static bool flash_word(uint32_t a, uint32_t v) { return my2k_test_flash_word(a,v); }
static bool flash_erase4(void) { return my2k_test_flash_erase4(); }
#else
#define REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define FMC UINT32_C(0x40023c00)
static bool flash_read(uint32_t a, void *p, size_t n) { memcpy(p,(const void *)(uintptr_t)a,n); return true; }
static bool wait_ready(void) {
    /* CPU fetches from flash may stall until completion; do not rely on SysTick
     * while interrupts are masked. Bounded polling also handles stuck status. */
    for (unsigned i=0;i<10000000u;i++) if (!(REG(FMC+12)&0x10000u)) return true;
    return false;
}
__attribute__((noipa,externally_visible)) bool my2k_settings_flash_operation(uint32_t address, uint32_t value, bool erase) {
    if ((!erase && (address<BASE || address>END-4 || (address&3))) || (erase && address!=BASE)) return false;
    uint32_t mask=__get_PRIMASK(); __disable_irq(); __DSB(); __ISB();
    uint32_t access=REG(FMC); bool ok=wait_ready();
    /* Geehy RM V2.4 FMC fields, with interrupt/cache erratum mitigations.
     * This function executes from application flash, not SRAM (§4.3 trigger).
     * Never touch FMC_OPTKEY/FMC_OPTCTRL or reserved CTRL bit13. */
    if (ok) {
        REG(FMC)=access&~0x700u;
        REG(FMC)=(access&~0x700u)|0x1800u;
        REG(FMC)=access&~0x700u;
        if (REG(FMC+16)&0x80000000u) { REG(FMC+4)=0x45670123u; REG(FMC+4)=0xcdef89abu; }
        ok=!(REG(FMC+16)&0x80000000u);
        if (ok) {
            REG(FMC+12)=0xf3u; /* W1C documented completion/error bits only. */
            if (erase) { REG(FMC+16)=0x222u; REG(FMC+16)=0x10222u; } /* SNUM4, SERS, 32bit */
            else { REG(FMC+16)=0x201u; REG(address)=value; }
            __DSB(); bool ready=wait_ready();ok=ready && !(REG(FMC+12)&0xf2u);
            /* CTRL is writable only while BUSY=0. A stuck controller latches
             * the save fault; never issue another CTRL write while it is busy. */
            if (ready) REG(FMC+16)=0x80000000u;
        }
        REG(FMC)=(access&~0x700u)|0x1800u;
        REG(FMC)=access&~0x700u;
        REG(FMC)=access;
    }
    __DSB(); __ISB(); __set_PRIMASK(mask);
    return ok;
}
static bool flash_word(uint32_t a,uint32_t v) { return my2k_settings_flash_operation(a,v,false); }
static bool flash_erase4(void) { return my2k_settings_flash_operation(BASE,0,true); }
#endif
static bool read_log(void *ctx,uint32_t o,void *p,size_t n) {
    (void)ctx;
    if (o>END-BASE || n>END-BASE-o) return false;
    /* Unknown manufacturer/older data is never interpreted as QMK settings. */
    if (format_needed) { memset(p,255,n); return true; }
    return flash_read(BASE+o,p,n);
}
static bool stage_log(void *ctx,uint32_t o,const void *p,size_t n) {
    (void)ctx;
    if (!staging || !p || (o&3) || (n&3) || o>=END-BASE || n>END-BASE-o) return false;
    if (!destination_set) { destination=o; destination_set=true; }
    if (o<destination || o-destination>SLOT || n>SLOT-(o-destination)) return false;
    memcpy(staged+o-destination,p,n); return true;
}
bool my2k_eeprom_storage(my2k_journal *j,uint32_t *units,my2k_journal_erase_unit *erase,void **ctx) {
    pending=staging=destination_set=format_needed=false; commits=erases=error=0;
    if (!j || !units || !erase || !ctx) return false;
    *units=0; *erase=NULL; *ctx=NULL;
    if (!my2k_journal_init(&log_store,(my2k_journal_backend){NULL,END-BASE,read_log,stage_log},MY2K_EEPROM_BYTES)) return false;
    size_t length=0;
    my2k_journal_result result=my2k_journal_load(&log_store,staged,MY2K_EEPROM_BYTES,&length);
    if (result==MY2K_JOURNAL_IO) { error=5; return false; }
    if (result!=MY2K_JOURNAL_OK || length!=MY2K_EEPROM_BYTES) {
        /* First use/recovery: erase only if the sector contains foreign or
         * corrupt records. Erasure is deferred until the idle save task. */
        for (uint32_t offset=0;offset<END-BASE;offset+=4) {
            uint32_t word;
            if (!flash_read(BASE+offset,&word,4)) { error=5; return false; }
            if (word!=UINT32_MAX) { format_needed=true; break; }
        }
    }
    *j=log_store; return true;
}
bool my2k_persist_begin(const uint8_t *image) {
    if (!image || pending || error) return false;
    /* EEPROM updates and the worker run on the QMK foreground task. Snapshot
     * makes later writes independent of GUI edits received during this save. */
    memcpy(snapshot,image,sizeof snapshot);
    if (format_needed) {
        if (!flash_erase4()) { error=1; return false; }
        erases++; format_needed=false;
#if MY2K_LOCK_LIGHTS_PROFILE
        my2k_lock_store_erased();
#endif
    }
    /* Coalesced edits that return to the committed image cost no flash wear. */
    size_t length=0;
    my2k_journal_result loaded=my2k_journal_load(&log_store,staged,MY2K_EEPROM_BYTES,&length);
    if (loaded==MY2K_JOURNAL_IO) { error=5; return false; }
    if (loaded==MY2K_JOURNAL_OK && length==MY2K_EEPROM_BYTES && !memcmp(snapshot,staged,length)) return true;
    memset(staged,255,sizeof staged);
    destination_set=false; staging=true;
    my2k_journal_result result=my2k_journal_save(&log_store,snapshot,sizeof snapshot);
    if (result==MY2K_JOURNAL_FULL) {
        /* One-sector compaction: interruption here can discard settings, never
         * loader/application. Caller waits for keyboard idle before starting. */
        if (!flash_erase4()) { error=1; staging=false; return false; }
        erases++; destination_set=false;
#if MY2K_LOCK_LIGHTS_PROFILE
        my2k_lock_store_erased();
#endif
        result=my2k_journal_save(&log_store,snapshot,sizeof snapshot);
    }
    staging=false;
    if (result!=MY2K_JOURNAL_OK || !destination_set || destination>SLOT*7u-SLOT) { error=2; return false; }
    cursor=0; pending=true; return true;
}
int my2k_persist_poll(void) {
    if (error) return -1;
    if (!pending) return 0;
    /* Four words per foreground pass. Commit word is naturally last. */
    for (unsigned budget=0;budget<4 && cursor<SLOT;budget++,cursor+=4) {
        uint32_t value; memcpy(&value,staged+cursor,4);
        uint32_t address=BASE+destination+cursor;
        if (address<BASE || address>END-4 || !flash_word(address,value)) { pending=false; error=3; return -1; }
        uint32_t readback;
        if (!flash_read(address,&readback,4) || readback!=value) { pending=false; error=4; return -1; }
    }
    if (cursor==SLOT) { pending=false; commits++; return 1; }
    return 0;
}
bool my2k_persist_busy(void) { return pending; }
uint32_t my2k_persist_commits(void) { return commits; }
uint32_t my2k_persist_erases(void) { return erases; }
uint32_t my2k_persist_error(void) { return error; }

#if MY2K_LOCK_LIGHTS_PROFILE
void my2k_persist_extension_fault(void){if(!error)error=6;}
#endif
