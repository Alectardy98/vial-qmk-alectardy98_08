/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#define main baseline_readonly_test_main
#include "test_main_fs.c"
#include <stdlib.h>
#undef main
#pragma GCC diagnostic pop
static uint8_t *large_image;static unsigned writes,syncs,fail_sync_on;static bool fail_write,fail_sync,change_write;
static bool write_sector(void *ctx,uint32_t lba,const uint8_t in[512]){
 (void)ctx;assert(lba<(large_image?70000u:SECTORS));writes++;
 if(fail_write)return false;
 memcpy((large_image?large_image:image)+lba*512,in,512);if(change_write){gen++;change_write=false;}return true;
}
static bool mutable_read(void *ctx,uint32_t lba,uint8_t out[512]){if(!large_image)return read_sector(ctx,lba,out);if(lba>=70000)return false;memcpy(out,large_image+lba*512,512);return true;}
static bool sync_media(void *ctx){(void)ctx;syncs++;return !fail_sync && syncs!=fail_sync_on;}
static unsigned open_write(const char *name,bool okay){uint8_t p[32];packet(p,37);p[2]=255;p[3]=0;memcpy(p+4,name,strlen(name)+1);process(p,okay);return p[3];}
int main(void){
 fixture();format=16;my2k_main_fs_backend b={.media=media,.read_sector=mutable_read,.write_sector=write_sector,.sync=sync_media};my2k_main_fs_init(&b);
 unsigned h=open_write("ASSET.CRS",true);assert(h<4 && writes && syncs);
 for(unsigned offset=0;offset<700;){
  unsigned n=700-offset;if(n>24)n=24;uint8_t p[32];packet(p,38);p[2]=h;p[3]=n;w32(p+4,offset);
  for(unsigned i=0;i<n;i++){p[8+i]=(uint8_t)((offset+i)^0xa5);}
  unsigned old_syncs=syncs;process(p,true);assert(p[3]==n && syncs>old_syncs);offset+=n;
  if(offset==48)assert(open_write("asset.crs",true)==h); /* retry must not truncate */
 }
 uint8_t p[32];packet(p,38);p[2]=h;p[3]=25;process(p,false);
 packet(p,38);p[2]=h;p[3]=1;w32(p+4,1000);process(p,false);
 packet(p,40);p[2]=h;process(p,true);packet(p,40);p[2]=h;process(p,true);
 my2k_main_fs_init(&b);h=open_name("ASSET.CRS",200,true);
 for(unsigned offset=0;offset<700;offset+=24){packet(p,39);p[2]=h;p[3]=24;w32(p+4,offset);process(p,true);unsigned n=700-offset;if(n>24)n=24;assert(p[3]==n);for(unsigned i=0;i<n;i++)assert(p[8+i]==(uint8_t)((offset+i)^0xa5));}
 packet(p,38);p[2]=h;p[3]=1;process(p,false);packet(p,40);p[2]=h;process(p,true);
 h=open_name("HELLO.TXT",255,true);packet(p,41);p[2]=0;process(p,false);packet(p,40);p[2]=h;process(p,true);
 packet(p,41);p[2]=0;process(p,true);unsigned before=writes;packet(p,41);p[2]=0;process(p,true);assert(writes==before); /* lost reply replay must not delete SECOND */
 h=open_name("SECOND.BIN",255,true);packet(p,40);p[2]=h;process(p,true);
 before=writes;fail_sync=true;open_write("PREFAIL.CRS",false);assert(writes==before);packet(p,41);p[2]=0;process(p,false);assert(writes==before);fail_sync=false;
 h=open_write("FAIL.CRS",true);fail_write=true;packet(p,38);p[2]=h;p[3]=4;memcpy(p+8,"TEST",4);process(p,false);fail_write=false;
 my2k_main_fs_init(&b);h=open_write("SYNC.CRS",true);fail_sync=true;packet(p,38);p[2]=h;p[3]=4;process(p,false);fail_sync=false;
 my2k_main_fs_init(&b);change_write=true;open_write("MEDIA.CRS",false);
 my2k_main_fs_init(&b);b.write_sector=NULL;my2k_main_fs_init(&b);before=writes;open_write("DENIED.CRS",false);assert(writes==before);
 /* Unlink succeeds, but its final wrapper flush fails. A retry of row0
  * must preserve SECOND.BIN rather than deleting a shifted directory entry. */
 fixture();format=16;b.write_sector=write_sector;my2k_main_fs_init(&b);
 fail_sync_on=syncs+3;packet(p,41);p[2]=0;process(p,false);assert(syncs==fail_sync_on);
 before=writes;packet(p,41);p[2]=0;process(p,false);assert(writes==before);fail_sync_on=0;
 h=open_name("SECOND.BIN",255,true);packet(p,40);p[2]=h;process(p,true);
 /* Writable FAT32 volume, independent cold-mount readback and unlink. */
 format=32;large_image=calloc(70000u,512u);assert(large_image);for(unsigned l=0;l<634;l++)assert(read_sector(NULL,l,large_image+l*512));
 b.write_sector=write_sector;my2k_main_fs_init(&b);h=open_write("FAT32.CRS",true);
 packet(p,38);p[2]=h;p[3]=24;memcpy(p+8,"FAT32-WRITE-READBACK-TEST!",24);process(p,true);packet(p,40);p[2]=h;process(p,true);
 my2k_main_fs_init(&b);h=open_name("FAT32.CRS",255,true);packet(p,39);p[2]=h;p[3]=24;process(p,true);assert(!memcmp(p+8,"FAT32-WRITE-READBACK-TEST!",24));packet(p,40);p[2]=h;process(p,true);packet(p,41);p[2]=0;process(p,true);free(large_image);large_image=NULL;
 puts("PASS: real FatFs FD37create/38write+sync/40close/41delete,700byte cold-remount readback,retries,mode/range/busy/failure/media-generation checks");
}
