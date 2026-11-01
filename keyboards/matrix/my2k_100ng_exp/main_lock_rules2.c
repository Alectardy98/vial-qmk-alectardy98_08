/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_lock_rules.h"
#include <string.h>
uint32_t my2k_lock_crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
static uint32_t get(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
bool my2k_lock_validate(const uint8_t*p){
 if(!p||p[0]!=2||p[1]>8||p[2]>1||p[3]||p[7]||get(p+124)!=my2k_lock_crc(p,124))return false;
 for(unsigned i=0;i<8;i++){
  const uint8_t*r=p+8+i*8;
  if(i>=p[1]){for(unsigned j=0;j<8;j++)if(r[j])return false;continue;}
  if(r[0]>4||r[1]>1||r[3]>4||r[4]>15)return false;
  if(r[0]==4?(r[2]<1||r[2]>3):r[2]!=0)return false;
 }
 for(unsigned i=0;i<5;i++){const uint8_t*r=p+72+i*8;if(r[0]>1||r[1]>4||r[2]>15||r[6]||r[7])return false;}
 for(unsigned i=112;i<124;i++)if(p[i])return false;
 return true;
}
void my2k_lock_defaults(uint8_t*p){
 memset(p,0,128);p[0]=2;
 /* Preserve the existing R1 default HSV0/0/255 (logical white). GUI marker
  * colours identify hardware groups; they are not a new current setting. */
 for(unsigned i=0;i<5;i++){uint8_t*r=p+72+i*8;r[0]=1;r[2]=8;r[3]=r[4]=r[5]=255;}
 uint32_t c=my2k_lock_crc(p,124);for(unsigned i=0;i<4;i++)p[124+i]=(uint8_t)(c>>(8*i));
}
my2k_rgb_color my2k_lock_effect(my2k_rgb_color c,uint8_t effect,uint8_t speed,uint32_t elapsed){
 uint32_t period=4000u/(speed+1u),phase=elapsed%period,level=255;
 if(effect==1)level=phase<period/2?255:0;
 else if(effect==2){uint32_t x=phase*510u/period;level=x<=255?x:510-x;}
 else if(effect==3)level=255u-phase*255u/period;
 else if(effect==4){uint8_t v=c.r>c.g?c.r:c.g;if(c.b>v)v=c.b;c=my2k_rgb_hsv((uint8_t)(phase*256u/period),255,v);}
 c.r=(uint16_t)c.r*level/255u;c.g=(uint16_t)c.g*level/255u;c.b=(uint16_t)c.b*level/255u;return c;
}
__attribute__((noinline,noclone,externally_visible))
my2k_rgb_color my2k_lock_evaluate(const uint8_t*p,uint8_t locks,uint32_t layers,uint32_t elapsed,uint8_t*selected){
 *selected=255;
 for(unsigned i=0;i<p[1];i++){
  const uint8_t*r=p+8+i*8;
  bool match=r[0]==1?(locks&2):r[0]==2?(locks&1):r[0]==3?(locks&4):r[0]==4?(layers&(UINT32_C(1)<<r[2])):false;
  if(!r[1]||!match)continue;
  *selected=(uint8_t)i;
  return my2k_lock_effect((my2k_rgb_color){r[5],r[6],r[7]},r[3],r[4],elapsed);
 }
 return p[2]?(my2k_rgb_color){p[4],p[5],p[6]}:(my2k_rgb_color){0,0,0};
}
