/* SPDX-License-Identifier: GPL-2.0-or-later */
#if MY2K_DEV22_PROFILE
#include "aux_perf22.h"
#endif
#include "aux_diagnostics.h"
#include "my2k_wire.h"
#include "aux_storage.h"
#include "aux_msc.h"
#include "aux_platform.h"
#if MY2K_DEV21_PROFILE
#include "aux_perf21.h"
#endif
extern aux_asset_status aux_live_assets;
static void le16(uint8_t *out,uint32_t n){out[0]=(uint8_t)n;out[1]=(uint8_t)(n>>8);}
static void le32(uint8_t *out,uint32_t n){le16(out,n);le16(out+2,n>>16);}
bool aux_diagnostics_snapshot(uint8_t page,const aux_panels *p,uint8_t b[8]){
#if MY2K_DEV7_PROFILE
 if(!p||!b||page>=MY2K_DIAG_PAGES)return false;
#if MY2K_DEV22_PROFILE
 if(page>=10&&page<20)return aux_perf22_page(page,b);
 if(page>=20){
  /* Foreground snapshots worker-owned RAM while the cooperative worker is
   * suspended. No SPI, SD or new peripheral read is initiated by this GET. */
  static aux_mono_perf_stats snap;static uint8_t epoch;static uint32_t hit,miss,bulk,failed,sd_failed;
  if(page>25)return false;
  for(unsigned i=0;i<8;i++){b[i]=0;}
  b[0]=page;
  if(page==20){
   snap=aux_mono_perf;epoch++;
#if MY2K_SD_BULK8
   extern volatile uint32_t aux_disk_cache_hits,aux_disk_cache_misses,aux_sd_bulk_successes,aux_sd_bulk_failures;
   hit=aux_disk_cache_hits;miss=aux_disk_cache_misses;bulk=aux_sd_bulk_successes;failed=aux_sd_bulk_failures;
#endif
   sd_failed=aux_sd_read_failures;
   b[1]=0xad;b[2]=epoch;b[3]=(p->mono_ready?1u:0u)|(p->mono_enabled?2u:0u)|(p->mono_dirty?4u:0u)|(p->mono_overlay_enabled?8u:0u);
   le32(b+4,snap.flushes);return true;
  }
  b[7]=epoch;
  if(page==21){le32(b+1,snap.last_cycles);le16(b+5,snap.last_ms>65535u?65535u:snap.last_ms);}
  else if(page==22){le32(b+1,snap.max_cycles);le16(b+5,snap.max_ms>65535u?65535u:snap.max_ms);}
  else if(page==23){le32(b+1,snap.pages);le16(b+5,snap.failures>65535u?65535u:snap.failures);}
  else if(page==24){le16(b+1,hit>65535u?65535u:hit);le16(b+3,miss>65535u?65535u:miss);le16(b+5,failed>65535u?65535u:failed);}
  else if(page==25){le32(b+1,bulk);le16(b+5,sd_failed>65535u?65535u:sd_failed);}
  return true;
 }
#endif
#else
 if(!p||!b||page>3)return false;
#endif
 for(unsigned n=0;n<8;n++)b[n]=0;
 b[0]=page;
 switch(page){
 case 0:
  b[1]=6;b[2]=aux_storage_live.stage;b[3]=(uint8_t)aux_sd_init_stage;
  b[4]=(uint8_t)aux_fault_code;b[5]=(p->mono_ready?1u:0u)|(p->color_ready?2u:0u);
  b[6]=aux_live_assets.mounted;b[7]=!aux_msc_io_idle();break;
 case 1:
  le32(b+1,aux_storage_live.sectors);le16(b+5,aux_storage_live.attempts);b[7]=aux_live_assets.last_mount_result;break;
 case 2:
  b[1]=(uint8_t)aux_sd_last_command;le32(b+2,aux_sd_last_status);le16(b+6,aux_sd_init_attempts);break;
 case 3:
  le32(b+1,aux_live_assets.mono_completed);le16(b+5,aux_live_assets.color_completed);b[7]=aux_live_assets.last_mono_open_result;break;
#if MY2K_DEV7_PROFILE
 case 4:
  b[1]=(uint8_t)aux_sd_read_fail_stage;b[2]=(uint8_t)aux_sd_last_read_words;
  b[3]=(uint8_t)aux_sd_read_ready_queries;le32(b+4,aux_sd_last_read_status);break;
 case 5:
  le32(b+1,aux_sd_last_read_lba);b[5]=(uint8_t)aux_sd_read_attempts;
  b[6]=(uint8_t)aux_sd_read_successes;b[7]=(uint8_t)aux_sd_read_failures;break;
#endif
#if MY2K_DEV21_PROFILE
 case 6: b[1]=21;le32(b+2,aux_display_perf.frames);le16(b+6,aux_display_perf.last_frame_ms);break;
 case 7: le32(b+1,aux_display_perf.bytes);le16(b+5,aux_display_perf.max_frame_ms);b[7]=(uint8_t)aux_display_perf.failed_sends;break;
 case 8: le32(b+1,aux_display_perf.checkpoints);le16(b+5,aux_display_perf.failed_frames);b[7]=aux_display_perf.frame_active;break;
 case 9: for(unsigned i=0;i<5;i++)b[i+1]=aux_display_perf.histogram[i]>255?255:aux_display_perf.histogram[i];le16(b+6,aux_display_perf.total_frame_ms);break;
#endif
 }
 return true;
}
