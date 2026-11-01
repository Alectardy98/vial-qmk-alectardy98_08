/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Offline original graphics exercise using the real asset decoder and panel
 * command emitter. Its modeled SPI wire times are lower bounds, NOT MCU FPS. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "aux_panels.h"
#include "my2k_asset.h"
static FILE *asset_file;
static bool dc[2];
static uint64_t wire_bytes,reads,read_bytes,serviced_chunks;
static bool configure(void *ctx){(void)ctx;return true;}
static bool line(void *ctx,aux_panel_id panel,unsigned signal,bool high){(void)ctx;if(signal==AUX_PANEL_DC)dc[panel]=high;return true;}
static bool send_bytes(void *ctx,aux_panel_id panel,const uint8_t *bytes,size_t size){(void)ctx;assert(panel==AUX_PANEL_COLOR||panel==AUX_PANEL_MONO);assert(bytes||!size);if(panel==AUX_PANEL_COLOR)wire_bytes+=size;return true;}
static bool delay_ms(void *ctx,uint32_t ms){(void)ctx;(void)ms;return true;}
static bool read_asset(void *ctx,uint32_t off,void *dst,size_t size){(void)ctx;reads++;read_bytes+=size;return fseek(asset_file,(long)off,SEEK_SET)==0&&fread(dst,1,size,asset_file)==size;}
static void full_frame(aux_panels *panels,my2k_asset *asset,unsigned frame){
 uint16_t row[128];
 for(unsigned y=0;y<128;y++){
  assert(my2k_asset_pixels(asset,(uint16_t)frame,y*128,128,row));
  assert(aux_panel_color_pixels(panels,128,128,y*128,row,128));
  serviced_chunks++; /* Same bounded foreground-service opportunity as runtime. */
 }
}
static void rectangle(aux_panels *p,unsigned x,unsigned y,bool sprite){
 uint16_t row[16];
 for(unsigned j=0;j<16;j++){
  for(unsigned i=0;i<16;i++)row[i]=sprite?(((i^j)&1u)?0xf800:0xffe0):0;
  assert(aux_panel_color_pixels(p,128,128,(y+j)*128+x,row,16));serviced_chunks++;
 }
}
int main(int argc,char **argv){
 assert(argc==2);asset_file=fopen(argv[1],"rb");assert(asset_file);
 assert(fseek(asset_file,0,SEEK_END)==0);long size=ftell(asset_file);assert(size>0&&size<=UINT32_MAX);
 my2k_reader reader={0,(uint32_t)size,read_asset};my2k_asset asset;assert(my2k_asset_open(&reader,&asset));
 assert(asset.width==128&&asset.height==128&&asset.frames==4);
 aux_panel_bus bus={0,configure,line,send_bytes,delay_ms};aux_panels panels;assert(aux_panels_init(&panels,&bus));
 uint64_t full[4];wire_bytes=reads=read_bytes=serviced_chunks=0;
 for(unsigned i=0;i<4;i++){uint64_t before=wire_bytes;full_frame(&panels,&asset,i);full[i]=wire_bytes-before;assert(full[i]==34176u);}
 uint64_t partial_start=wire_bytes;
 for(unsigned i=0;i<60;i++){
  rectangle(&panels,i,16,false);rectangle(&panels,i+1,16,true);
 }
 uint64_t partial=(wire_bytes-partial_start)/60u;assert(partial==1376u);
 printf("{\n  \"offline_only\": true,\n  \"physical_FPS_measured\": false,\n  \"original_asset_frames\": 4,\n  \"asset_read_calls\": %llu,\n  \"asset_read_bytes\": %llu,\n  \"full_frame_SPI_bytes\": %llu,\n  \"sprite_erase_draw_SPI_bytes\": %llu,\n  \"foreground_service_opportunities\": %llu,\n  \"source_nominal_SPI_Hz\": 54000000,\n  \"ideal_full_frame_wire_us\": %.3f,\n  \"ideal_sprite_pair_wire_us\": %.3f,\n  \"full_frame_60FPS_required_Mbit_s\": %.6f,\n  \"limitations\": \"Capture bus has no MCU PIO, SD, DMA, IRQ, cache or panel-refresh timing. Source-nominal SPI clock and fitted-controller limits require physical/document verification.\"\n}\n",
 (unsigned long long)reads,(unsigned long long)read_bytes,(unsigned long long)full[0],(unsigned long long)partial,(unsigned long long)serviced_chunks,
 (double)full[0]*8.0/54.0,(double)partial*8.0/54.0,(double)full[0]*8.0*60.0/1000000.0);
 fclose(asset_file);return 0;
}
