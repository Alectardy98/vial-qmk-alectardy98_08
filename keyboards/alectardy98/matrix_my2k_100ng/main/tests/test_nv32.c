/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "main_nv32.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t flash[2*MY2K_NV_BANK_BYTES],backup[sizeof flash],expected[MY2K_NV_BYTES],actual[MY2K_NV_BYTES];
static my2k_nv32 s,reopened;
static int fail_word=-1,fail_erase=-1;static unsigned words,erases;
static bool read_flash(uint32_t a,void*p,size_t n){assert(a>=MY2K_NV_BASE&&a<=MY2K_NV_BASE+sizeof flash&&n<=MY2K_NV_BASE+sizeof flash-a);memcpy(p,flash+a-MY2K_NV_BASE,n);return true;}
static bool program(uint32_t a,uint32_t v){assert(!(a&3)&&a>=MY2K_NV_BASE&&a<=MY2K_NV_BASE+sizeof flash-4);if(fail_word>=0&&words++==(unsigned)fail_word)return false;if(fail_word<0)words++;uint32_t old;memcpy(&old,flash+a-MY2K_NV_BASE,4);assert((old&v)==v);memcpy(flash+a-MY2K_NV_BASE,&v,4);return true;}
static bool erase(uint32_t a){assert(a==MY2K_NV_BASE||a==MY2K_NV_BASE+MY2K_NV_BANK_BYTES);if(fail_erase>=0&&erases++==(unsigned)fail_erase){memset(flash+a-MY2K_NV_BASE,255,8192);return false;}if(fail_erase<0)erases++;memset(flash+a-MY2K_NV_BASE,255,MY2K_NV_BANK_BYTES);return true;}
static my2k_nv_io io={read_flash,program,erase};
static void init(void){memset(flash,255,sizeof flash);words=erases=0;fail_word=fail_erase=-1;assert(my2k_nv_init(&s,io));memset(expected,0,sizeof expected);}
static void verify(my2k_nv32*p){assert(my2k_nv_read(p,0,actual,sizeof actual));assert(!memcmp(actual,expected,sizeof actual));}
static void reboot(void){assert(my2k_nv_init(&reopened,io));verify(&reopened);s=reopened;}
static void data_and_cache(void){
 init();verify(&s);assert(sizeof s<6500);
 for(unsigned i=0;i<MY2K_NV_BYTES;i++)expected[i]=(uint8_t)(i*37u+(i>>8));
 assert(my2k_nv_write(&s,0,expected,sizeof expected));verify(&s);assert(my2k_nv_flush(&s));assert(s.evictions&&s.commits);reboot();
 uint32_t seed=42;for(unsigned i=0;i<3000;i++){
  seed=seed*1664525u+1013904223u;unsigned at=seed%MY2K_NV_BYTES;uint8_t v=(uint8_t)(seed>>24);expected[at]=v;assert(my2k_nv_write(&s,at,&v,1));
  unsigned before=words;assert(my2k_nv_service(&s,4));assert(words-before<=4);
  if(!(i%79)){assert(my2k_nv_flush(&s));verify(&s);reboot();}
 }assert(my2k_nv_flush(&s));reboot();assert(erases>2);
 assert(!my2k_nv_read(&s,MY2K_NV_BYTES,actual,1));assert(!my2k_nv_write(&s,MY2K_NV_BYTES-1,actual,2));assert(!my2k_nv_write(&s,UINT32_MAX,actual,1));
 assert(my2k_nv_clear(&s));memset(expected,0,sizeof expected);verify(&s);assert(my2k_nv_flush(&s));reboot();
 puts("PASS full32KiB, cache pressure, random writes, bounds, clears, repeated rotation/restart; RAM<6500B");
}
static void cuts(void){
 init();expected[0]=1;expected[16384]=2;expected[MY2K_NV_BYTES-1]=3;assert(my2k_nv_write(&s,0,expected,sizeof expected));assert(my2k_nv_flush(&s));memcpy(backup,flash,sizeof flash);
 uint8_t next=42;
 for(unsigned cut=0;cut<=210;cut++){
  memcpy(flash,backup,sizeof flash);fail_word=-1;assert(my2k_nv_init(&s,io));assert(my2k_nv_write(&s,123,&next,1));words=0;fail_word=(int)cut;bool done=my2k_nv_flush(&s);fail_word=-1;assert(my2k_nv_init(&reopened,io));
  if(done){expected[123]=next;}verify(&reopened);
  if(cut==0||cut==16||cut==69||cut==70||cut==140||cut==209){s=reopened;uint8_t resume=99;assert(my2k_nv_write(&s,23000,&resume,1));assert(my2k_nv_flush(&s));expected[23000]=resume;reboot();expected[23000]=0;}
  expected[123]=0;
 }
 /* Force next append to rotate. Cut every word of header and checkpoint. */
 memcpy(flash,backup,sizeof flash);assert(my2k_nv_init(&s,io));s.slot=MY2K_NV_SLOTS;assert(my2k_nv_write(&s,123,&next,1));words=0;assert(my2k_nv_flush(&s));unsigned total=words;
 for(unsigned cut=0;cut<=total;cut++){
  memcpy(flash,backup,sizeof flash);fail_word=-1;assert(my2k_nv_init(&s,io));s.slot=MY2K_NV_SLOTS;assert(my2k_nv_write(&s,123,&next,1));words=0;fail_word=(int)cut;bool done=my2k_nv_flush(&s);fail_word=-1;assert(my2k_nv_init(&reopened,io));
  if(done){expected[123]=next;}verify(&reopened);expected[123]=0;
 }
 memcpy(flash,backup,sizeof flash);assert(my2k_nv_init(&s,io));s.slot=MY2K_NV_SLOTS;assert(my2k_nv_write(&s,123,&next,1));erases=0;fail_erase=0;assert(!my2k_nv_flush(&s));fail_erase=-1;assert(my2k_nv_init(&reopened,io));verify(&reopened);
 printf("PASS211 append and%u rotation program-word cuts, interrupted erase, old-bank recovery\n",total+1);
}
static void concurrent_changes(void){
 init();uint8_t a=1,b=2;assert(my2k_nv_write(&s,0,&a,1));assert(my2k_nv_flush(&s));
 assert(my2k_nv_write(&s,0,&b,1));for(unsigned i=0;i<80;i++)assert(my2k_nv_service(&s,4));
 a=3;assert(my2k_nv_write(&s,0,&a,1));assert(my2k_nv_flush(&s));expected[0]=3;reboot();
 unsigned before=words;assert(my2k_nv_write(&s,0,&a,1));assert(my2k_nv_flush(&s));assert(words==before);
 /* Torn newer record cannot convert a pending batch into a valid commit. */
 assert(my2k_nv_write(&s,700,&b,1));assert(my2k_nv_flush(&s));expected[700]=2;reboot();
 puts("PASS modifications during in-flight page, same-image no-write, committed-state recovery");
}
int main(void){data_and_cache();cuts();concurrent_changes();printf("my2k_nv32 state=%zu bytes\n",sizeof s);return 0;}
