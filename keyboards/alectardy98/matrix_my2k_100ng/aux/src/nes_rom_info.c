/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "my2k_nes_rom.h"
#include <string.h>
static uint32_t ram(unsigned n){return n?64u<<n:0;}
static uint32_t rom_size(unsigned lo,unsigned hi,unsigned unit){
 if(hi!=15)return ((hi<<8)|lo)*unit;
 unsigned e=lo>>2,m=2u*(lo&3u)+1u;
 if(e>=24)return 0xffffffffu;
 return (1u<<e)*m;
}
int my2k_nes_rom_parse(const uint8_t h[16],uint32_t size,my2k_nes_rom_info *out){
 if(!h||!out||size<16||memcmp(h,"NES\032",4))return 2;
 /* Recognize only the exact legacy tool signature; never guess arbitrary
  * damaged mapper bits, and never rewrite the user's ROM file. */
 uint8_t normalized[16];
 if(!memcmp(h+7,"DiskDude!",9)){memcpy(normalized,h,16);memset(normalized+7,0,9);h=normalized;}
 my2k_nes_rom_info r={0};r.nes2=(h[7]&12u)==8u;r.flags=h[6]&15u;
 if((h[7]&3u)||(!r.nes2&&(h[7]&12u)))return 3; /* Console variants unsupported. */
 r.mapper=(h[6]>>4)|(h[7]&240u);
 if(r.nes2){
  r.mapper|=(uint16_t)(h[8]&15u)<<8;r.submapper=h[8]>>4;
  r.prg_bytes=rom_size(h[4],h[9]&15u,16384);r.chr_bytes=rom_size(h[5],h[9]>>4,8192);
  r.prg_ram=ram(h[10]&15u)+ram(h[10]>>4);r.chr_ram=ram(h[11]&15u)+ram(h[11]>>4);
  if(h[12]&3u)return 10; /* Current simulation is NTSC only. */
  if((h[14]&3u)||r.submapper)return 6;
 }else{
  r.prg_bytes=(uint32_t)h[4]*16384u;r.chr_bytes=(uint32_t)h[5]*8192u;
  r.prg_ram=(h[8]?h[8]:1u)*8192u;r.chr_ram=r.chr_bytes?0:8192;
  if(h[9]&1u)return 10;
 }
 switch(r.mapper){case 0:case 1:case 2:case 3:case 4:case 7:case 40:case 66:break;default:return 4;}
 if(!r.prg_bytes||r.prg_bytes>MY2K_NES_PRG_MAX||r.chr_bytes>MY2K_NES_CHR_MAX||r.prg_bytes%16384||r.chr_bytes%8192)return 5;
 if(r.prg_ram>8192||r.chr_ram>8192||(!r.chr_bytes&&!r.chr_ram))return 9;
 if((r.mapper==0&&(r.prg_bytes>32768||r.chr_bytes>8192))||
    (r.mapper==1&&(r.prg_bytes>524288u||r.chr_bytes>131072u))||
    (r.mapper==2&&r.chr_bytes)||
    (r.mapper==3&&(r.prg_bytes>32768||!r.chr_bytes))||
    (r.mapper==7&&(r.chr_bytes||r.prg_bytes%32768))||
    (r.mapper==40&&(r.prg_bytes!=65536||r.chr_bytes!=8192))||
    (r.mapper==66&&(!r.chr_bytes||r.prg_bytes%32768)))return 6;
 r.prg_offset=16u+((r.flags&4u)?512u:0u);r.chr_offset=r.prg_offset+r.prg_bytes;
 if(r.chr_offset>size||r.chr_bytes!=size-r.chr_offset)return 5;
 *out=r;return 0;
}
