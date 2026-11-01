/* Private engineering backend: real bounded file reads, never a fake XIP address.
 * The target FatFs adapter must replace this host pread implementation. */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef MY2K_DOOM_TARGET
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif
#include "doomtype.h"
#include "my2k_stream_assets.h"
#include "w_wad.h"
#include "i_system.h"
#include "z_zone.h"
/* Renderer-owned512-byte pages; no pointer escapes this bounded copy cache.
 * Extra buffers are borrowed only while Doom is the exclusive active game. */
#ifndef MY2K_DOOM_TARGET
static int wadfd=-1, columnsfd=-1,opaquefd=-1;
#endif
static uint64_t calls, bytes_read, requests;
#ifndef MY2K_PAGE_SIZE
#define MY2K_PAGE_SIZE 512u
#endif
#define MY2K_PAGE_COUNT 56u
#if MY2K_PAGE_SIZE != 512u
#error This profile uses32bounded512-byte pages
#endif
static struct { unsigned file,base,stamp; } pages[MY2K_PAGE_COUNT];
static byte cache_data[8192];
#ifdef MY2K_DOOM_TARGET
static byte *cache_extra[3];
void my2k_doom_cache_buffers(uint8_t *first,uint8_t *second,uint8_t *third){cache_extra[0]=first;cache_extra[1]=second;cache_extra[2]=third;}
#else
static byte host_extra[20480];
static byte *cache_extra[3]={host_extra,host_extra+4096,host_extra+8192};
#endif
static byte *page_bytes(unsigned slot){
 if(slot<16)return cache_data+slot*512u;
 slot-=16;
 unsigned group=slot<16?slot/8:2,offset=slot<16?(slot%8u)*512u:(slot-16u)*512u;
 if(slot>=40||!cache_extra[group])I_Error("MY2K cache ownership");
 return cache_extra[group]+offset;
}
static unsigned clock_stamp;
unsigned my2k_cache_misses[3];
#ifdef MY2K_DOOM_TARGET
extern int my2k_doom_file_read(unsigned,unsigned,void *,unsigned);
#else
static int my2k_doom_file_read(unsigned file,unsigned offset,void *dst,unsigned n){
 int *fd=file==2?&opaquefd:file?&columnsfd:&wadfd;
 if(*fd<0)*fd=open(file==2?"../doom-opaque-private.bin":file?"../doom-composites-private.bin":"../doom1-private.wad",O_RDONLY);
 return *fd>=0 && pread(*fd,dst,n,offset)==(int)n;
}
#endif
static void cached_read(unsigned file,unsigned offset,void *dst,unsigned n){
 unsigned limit=file==2?1378304u:file?675136u:MY2K_WAD_BYTES;
 if(offset>limit||n>limit-offset)I_Error("MY2K asset bounds");
 requests++;
 while(n){
  unsigned base=offset&~(MY2K_PAGE_SIZE-1u);
  unsigned first=(((base>>9)^(base>>14)^(file*5u))&7u)*7u,slot=first;int found=0;
  for(unsigned i=first;i<first+7u;i++){
   if(pages[i].stamp&&pages[i].file==file&&pages[i].base==base){slot=i;found=1;break;}
   if(!pages[i].stamp||pages[i].stamp<pages[slot].stamp)slot=i;
  }
  if(!found){
   unsigned bytes=limit-base;if(bytes>MY2K_PAGE_SIZE)bytes=MY2K_PAGE_SIZE;
   if(!my2k_doom_file_read(file,base,page_bytes(slot),bytes))I_Error("MY2K asset read");
   pages[slot].file=file;pages[slot].base=base;my2k_cache_misses[file]++;calls++;bytes_read+=bytes;
  }
  if(++clock_stamp==0){for(unsigned i=0;i<MY2K_PAGE_COUNT;i++)if(pages[i].stamp)pages[i].stamp=1;clock_stamp=2;}
  pages[slot].stamp=clock_stamp;
  unsigned count=MY2K_PAGE_SIZE-(offset-base);if(count>n)count=n;
  memcpy(dst,page_bytes(slot)+(offset-base),count);dst=(byte*)dst+count;offset+=count;n-=count;
 }
}
void my2k_stream_read(unsigned offset,void *dst,unsigned count){cached_read(0,offset,dst,count);}
static void *cache[MY2K_WAD_LUMPS];
const void *my2k_stream_lump(int lump,int tag){
 if((unsigned)lump>=MY2K_WAD_LUMPS || (unsigned)lump>=(unsigned)numlumps)I_Error("MY2K lump bounds");
 if(!cache[lump]){cache[lump]=Z_Malloc(lumpinfo[lump].size,tag,&cache[lump]);my2k_stream_read(lumpinfo[lump].position,cache[lump],lumpinfo[lump].size);}
 else Z_ChangeTag(cache[lump],tag);
 return cache[lump];
}
extern const unsigned my2k_tcd_offset[],my2k_tcd_size[];
/* A column pointer is borrowed until the next column request; all call sites
 * draw the column before making another request. No concurrent renderer. */
byte *my2k_stream_column(int tex,unsigned offset,unsigned size){
 static byte column[512];
 if((unsigned)tex>=125 || size>sizeof column || offset>my2k_tcd_size[tex] || size>my2k_tcd_size[tex]-offset)I_Error("MY2K composite column bounds");
 cached_read(1,my2k_tcd_offset[tex]+offset,column,size);return column;
}
void my2k_stream_report(void){printf("CACHE files=%u,%u,%u\n",my2k_cache_misses[0],my2k_cache_misses[1],my2k_cache_misses[2]);printf("MY2K_STREAM read_calls=%llu bytes=%llu requests=%llu\n",(unsigned long long)calls,(unsigned long long)bytes_read,(unsigned long long)requests);}

byte *my2k_stream_patch_column(int lump,unsigned offset){
 static byte column[1024];
 if((unsigned)lump>=MY2K_WAD_LUMPS || offset<3 || offset>(unsigned)lumpinfo[lump].size)I_Error("MY2K wall column bounds");
 unsigned start=offset-3,n=lumpinfo[lump].size-start;if(n>sizeof column)n=sizeof column;
 memset(column,0,sizeof column);my2k_stream_read(lumpinfo[lump].position+start,column,n);
 /* The mask walker must find a sentinel inside this bounded window. */
 unsigned at=0;
 while(at<n&&column[at]!=255){if(at+1>=n)I_Error("MY2K short column header");if(at+4u+column[at+1]>=n)I_Error("MY2K masked column exceeds bounded window");at+=4u+column[at+1];}
 if(at>=n)I_Error("MY2K masked column lacks terminator");
 return column+3;
}

byte *my2k_stream_opaque(int tex,unsigned col){
 extern const unsigned my2k_opaque_offset[125];static byte column[128];
 if((unsigned)tex>=125u)I_Error("opaque texture bounds");
 cached_read(2,my2k_opaque_offset[tex]+col*128u,column,128);return column;
}
