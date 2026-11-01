/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_lock_rules.h"
#include <string.h>
uint32_t my2k_lock_crc(const uint8_t *p,size_t n){
 uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;
}
static uint32_t get32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
bool my2k_lock_validate(const uint8_t *p){
 if(!p||p[0]!=1||p[1]>8||p[2]>1||p[3]||p[7]||get32(p+72)!=my2k_lock_crc(p,72))return false;
 for(unsigned i=0;i<8;i++){
  const uint8_t *r=p+8+i*8;
  if(i>=p[1]){for(unsigned k=0;k<8;k++)if(r[k])return false;continue;}
  if(r[0]>MY2K_LOCK_LAYER||r[1]>1||r[2]>=MY2K_LOCK_LAYER_COUNT||r[3]>MY2K_LOCK_RAINBOW||r[4]>15)return false;
  if(r[0]!=MY2K_LOCK_LAYER&&r[2])return false;
 }
 return true;
}
void my2k_lock_defaults(uint8_t *p){memset(p,0,76);p[0]=1;uint32_t c=my2k_lock_crc(p,72);for(unsigned i=0;i<4;i++)p[72+i]=(uint8_t)(c>>(8*i));}
__attribute__((noinline,noclone,externally_visible)) my2k_rgb_color my2k_lock_evaluate(const uint8_t *p,uint8_t locks,uint32_t layers,uint32_t elapsed,uint8_t *selected){
 *selected=255;
 for(unsigned i=0;i<p[1];i++){
  const uint8_t *r=p+8+i*8;
  bool match=r[0]==MY2K_LOCK_CAPS?(locks&2):r[0]==MY2K_LOCK_NUM?(locks&1):r[0]==MY2K_LOCK_SCROLL?(locks&4):r[0]==MY2K_LOCK_LAYER?(layers&(UINT32_C(1)<<r[2])):false;
  if(!r[1]||!match)continue;
  *selected=(uint8_t)i;
  /* 4s at speed0 through250ms at15; pure wall-clock phases in either USB mode. */
  uint32_t period=4000u/(r[4]+1u),phase=elapsed%period,level=255;
  if(r[3]==MY2K_LOCK_BLINK)level=phase<period/2?255:0;
  if(r[3]==MY2K_LOCK_BREATH){uint32_t x=phase*510u/period;level=x<=255?x:510-x;}
  if(r[3]==MY2K_LOCK_PULSE)level=255u-phase*255u/period;
  my2k_rgb_color color={r[5],r[6],r[7]};
  if(r[3]==MY2K_LOCK_RAINBOW){uint8_t peak=r[5]>r[6]?r[5]:r[6];if(r[7]>peak)peak=r[7];color=my2k_rgb_hsv((uint8_t)(phase*256u/period),255,peak);}
  color.r=(uint16_t)color.r*level/255u;color.g=(uint16_t)color.g*level/255u;color.b=(uint16_t)color.b*level/255u;
  return color;
 }
 return p[2]?(my2k_rgb_color){p[4],p[5],p[6]}:(my2k_rgb_color){0,0,0};
}
