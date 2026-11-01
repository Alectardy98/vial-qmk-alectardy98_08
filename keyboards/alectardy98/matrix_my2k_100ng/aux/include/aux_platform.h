/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef AUX_PLATFORM_H
#define AUX_PLATFORM_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Compiled for offline review only. No transport is opened by the build. */
bool aux_clock_init(void);
void aux_uart_init(void);
bool aux_uart_get(uint8_t *byte);
bool aux_uart_rx_pending(void);
/* Frame-atomic nonblocking enqueue, at most511 bytes; false leaves queue unchanged. */
bool aux_uart_send(const char *bytes,size_t size);
extern volatile uint32_t aux_uart_rx_overflows,aux_uart_tx_overflows;
uint32_t aux_millis(void);
/* Same SysTick word as aux_millis(), without a cross-TU call in hot polling
 * paths. Native models keep their existing clock injection boundary. */
static inline uint32_t aux_millis_fast(void){
#if defined(__arm__)
 extern volatile uint32_t aux_tick_ms;
 return aux_tick_ms;
#else
 return aux_millis();
#endif
}
/* False means the tick stopped; callers must abort the dependent sequence. */
bool aux_delay(uint32_t ms);
void aux_idle(void);
void aux_touch_init(void);
bool aux_touch_read(uint8_t channels[8]);
bool aux_sd_init(void);
bool aux_sd_read(uint32_t lba,uint8_t block[512]);
#if MY2K_SD_BULK8
bool aux_sd_read_blocks(uint32_t lba,uint8_t *blocks,unsigned count);
uint32_t aux_sd_content_epoch(void);
#endif
#if MY2K_STOCK_PARITY_PROFILE && (!MY2K_DEV7_PROFILE || MY2K_RELEASE1_PROFILE)
bool aux_sd_write(uint32_t lba,const uint8_t block[512]);
bool aux_sd_write_blocks(uint32_t lba,const uint8_t *blocks,unsigned count);
#endif
uint32_t aux_sd_sectors(void);
/* Pure CSD parsing shared with host tests; SD register words RESP1..RESP4. */
bool aux_csd_sectors(const uint32_t words[4],uint32_t *sectors);
extern volatile uint32_t aux_fault_code;
#endif
