/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Independent Pan Docs MBC7 / Microchip93LC56 x16 protocol implementation.
 * This is a virtual cartridge: the existing mapped D-pad supplies tilt.
 * EEPROM writes change bounded cartridge RAM; filesystem save stays at exit. */
#include "my2k_gbc_mbc7.h"
#include <string.h>
static struct {uint8_t *ram;uint16_t command,word,x,y;uint8_t enabled,armed,buttons,io,out,phase,bits,address,write_enable;bool dirty;} s;
enum {WAIT_START,COMMAND,READ_WORD,WRITE_WORD,WRITE_ALL,DONE};
static uint16_t word(unsigned a){return (uint16_t)((uint16_t)s.ram[a*2u]<<8|s.ram[a*2u+1u]);}
static void store(unsigned a,uint16_t v){if(word(a)!=v){s.ram[a*2u]=(uint8_t)(v>>8);s.ram[a*2u+1u]=(uint8_t)v;s.dirty=true;}}
void my2k_mbc7_init(uint8_t ram[256]){memset(&s,0,sizeof s);s.ram=ram;s.x=s.y=0x8000;s.out=1;}
void my2k_mbc7_buttons(uint8_t b){s.buttons=b;}
bool my2k_mbc7_dirty(void){return s.dirty;}
uint8_t my2k_mbc7_read(uint16_t a){
 if(!s.enabled||a>=0xb000)return 255;
 switch(a&0xf0u){
 case 0x20:return (uint8_t)s.x;case 0x30:return (uint8_t)(s.x>>8);
 case 0x40:return (uint8_t)s.y;case 0x50:return (uint8_t)(s.y>>8);
 case 0x60:return 0;case 0x80:return (uint8_t)(s.io|s.out);default:return 255;
 }
}
static void clock_bit(unsigned bit){
 if(s.phase==WAIT_START){if(bit){s.phase=COMMAND;s.bits=0;s.command=0;}return;}
 if(s.phase==COMMAND){
  s.command=(uint16_t)((s.command<<1)|bit);if(++s.bits!=10)return;
  unsigned op=s.command>>8;s.address=(uint8_t)(s.command&127u);s.bits=0;s.phase=DONE;s.out=1;
  if(op==2){s.phase=READ_WORD;s.word=word(s.address);s.bits=16;s.out=0;}
  else if(op==1){s.phase=WRITE_WORD;s.word=0;}
  else if(op==3){if(s.write_enable)store(s.address,65535);}
  else switch((s.command>>6)&3u){
   case 0:s.write_enable=0;break;case 3:s.write_enable=1;break;
   case 2:if(s.write_enable)for(unsigned i=0;i<128;i++)store(i,65535);break;
   case 1:s.phase=WRITE_ALL;s.word=0;break;
  }
 }else if(s.phase==READ_WORD){
  s.out=(uint8_t)(s.word>>15);s.word=(uint16_t)(s.word<<1);
  if(!--s.bits){s.address=(uint8_t)((s.address+1u)&127u);s.word=word(s.address);s.bits=16;}
 }else if(s.phase==WRITE_WORD||s.phase==WRITE_ALL){
  s.word=(uint16_t)((s.word<<1)|bit);if(++s.bits!=16)return;
  if(s.write_enable){if(s.phase==WRITE_WORD)store(s.address,s.word);else for(unsigned i=0;i<128;i++)store(i,s.word);}
  s.phase=DONE;s.out=1;
 }
}
void my2k_mbc7_write(uint16_t a,uint8_t v){
 if(a>=0x4000&&a<0x6000){s.enabled=v==0x40;return;}
 if(!s.enabled||a<0xa000||a>=0xb000)return;
 switch(a&0xf0u){
 case 0:if(v==0x55){s.x=s.y=0x8000;s.armed=1;}break;
 case 0x10:if(v==0xaa&&s.armed){
  int x=((s.buttons&64u)?112:0)-((s.buttons&128u)?112:0);
  int y=((s.buttons&16u)?112:0)-((s.buttons&32u)?112:0);
  s.x=(uint16_t)(0x81d0+x);s.y=(uint16_t)(0x81d0+y);s.armed=0;
 }break;
 case 0x80:
  if(!(v&128u)){s.phase=WAIT_START;s.bits=0;s.out=1;}
  else if(!(s.io&128u)){s.phase=WAIT_START;s.bits=0;s.out=1;}
  if((v&128u)&&!(s.io&64u)&&(v&64u))clock_bit((v>>1)&1u);
  s.io=v&0xc2u;break;
 default:break;
 }
}
