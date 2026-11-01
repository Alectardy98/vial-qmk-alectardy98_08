/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_nes_library.h"
#include "aux_nes.h"
#include "aux_work.h"
#include "my2k_nes_rom.h"
#include "my2k_gbc_rom.h"
#include "my2k_oled_config.h"
#include "ff.h"
#include <string.h>
static const char *const roots[2]={"0:/nes/","0:/gbc/"};
static const char *const configs[2][3]={{"0:/nes/R1ROM.CFG","0:/nes/R1ROM.TMP","0:/nes/R1ROM.BAK"},{"0:/gbc/R1ROM.CFG","0:/gbc/R1ROM.TMP","0:/gbc/R1ROM.BAK"}};
static bool close_fault;
static bool save_writing;
bool aux_nes_library_ready(void){return !close_fault;}
bool aux_nes_library_writing(void){return save_writing;}
void aux_nes_library_close_failed(void){close_fault=true;}
static bool close_file(FIL *f){if(f_close(f)==FR_OK)return true;close_fault=true;return false;}
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
static bool name_ok(const char *s,unsigned gb){
 size_t n=strlen(s);if(n<(gb?4u:5u)||n>95||strchr(s,'/')||strchr(s,'\\')||strchr(s,':')||strstr(s,".."))return false;
 for(unsigned i=0;i<n;i++)if((uint8_t)s[i]<32)return false;
 if(gb){const char *e=strrchr(s,'.');return e&&(e[1]=='g'||e[1]=='G')&&(e[2]=='b'||e[2]=='B')&&(!e[3]||((e[3]=='c'||e[3]=='C')&&!e[4]));}
 const char *e=s+n-4;return e[0]=='.'&&(e[1]=='n'||e[1]=='N')&&(e[2]=='e'||e[2]=='E')&&(e[3]=='s'||e[3]=='S');
}
static bool record(const char *path,uint8_t data[128],unsigned gb){
 FIL f;UINT got=0;
 if(f_open(&f,path,FA_READ)!=FR_OK)return false;
 bool ok=f_size(&f)==128&&f_read(&f,data,128,&got)==FR_OK&&got==128;
 if(!close_file(&f))ok=false;
 return ok&&!memcmp(data,gb?"GRC1":"NRC1",4)&&data[4]==1&&data[5]>0&&data[5]<=95&&data[8+data[5]]==0&&
        strlen((char*)data+8)==data[5]&&name_ok((char*)data+8,gb)&&word(data+124)==my2k_oled_crc(data,124);
}
static bool selected(char path[160],unsigned gb){
 if(close_fault)return false;
 uint8_t data[128];if(!record(configs[gb][0],data,gb)&&(close_fault||!record(configs[gb][2],data,gb)))return false;
 strcpy(path,roots[gb]);strcat(path,(char*)data+8);return true;
}
static unsigned info(const char *name,uint32_t *size,uint8_t h[16],my2k_nes_rom_info *r,unsigned gb){
 char path[160];strcpy(path,roots[gb]);strcat(path,name);FIL f;UINT got=0;
 if(f_open(&f,path,FA_READ)!=FR_OK)return 2;
 *size=(uint32_t)f_size(&f);uint8_t header[336]={0};unsigned bytes=gb?336u:16u;bool ok=f_read(&f,header,bytes,&got)==FR_OK&&got==bytes;
 memcpy(h,header+(gb?320:0),16);
 if(!close_file(&f))ok=false;
 if(!ok)return 2;
 if(!gb)return (unsigned)my2k_nes_rom_parse(h,*size,r);
 my2k_gbc_rom_info g={0};unsigned e=(unsigned)my2k_gbc_rom_parse(header,*size,&g);r->prg_bytes=g.rom_bytes;r->chr_bytes=g.ram_bytes;r->mapper=e?header[0x147]:g.mbc;return e;
}
static uint8_t request(unsigned op,uint32_t cursor,const uint8_t *data,unsigned n,uint8_t *out,unsigned *out_size,unsigned gb){
 *out_size=0;if(close_fault)return 5;
 if(aux_nes_active())return 4;
 if(op==11){
  if(n||cursor>=512)return 3;
  memset(out,0,16);memcpy(out,gb?"GRL1":"NRL1",4);out[4]=1;out[5]=24;
  char selected_path[160]={0};(void)selected(selected_path,gb);
  DIR dir;FILINFO file;if(f_opendir(&dir,gb?"0:/gbc":"0:/nes")!=FR_OK)return 5;
  bool ok=true,ended=false;unsigned count=0;
  for(unsigned i=0;i<512;i++){
   if(f_readdir(&dir,&file)!=FR_OK){ok=false;break;}
   if(!file.fname[0]){ended=true;break;}
   if(i<cursor)continue;
   if((file.fattrib&AM_DIR)||!name_ok(file.fname,gb))continue;
   if(count==24){out[7]=1;put(out+12,i);ended=true;break;}
   uint8_t *row=out+16+112u*count;memset(row,0,112);
   uint8_t h[16]={0};my2k_nes_rom_info r={0};uint32_t size=0;
   unsigned error=info(file.fname,&size,h,&r,gb);
   if(close_fault){ok=false;break;}
   put(row,size);put(row+4,r.prg_bytes);put(row+8,r.chr_bytes);
   unsigned mapper=(h[6]>>4)|(h[7]&240u);if((h[7]&12u)==8u)mapper|=(h[8]&15u)<<8;
   if(gb||!error)mapper=r.mapper;
   row[12]=(uint8_t)mapper;row[13]=(uint8_t)(mapper>>8);row[14]=(uint8_t)error;
   row[15]=selected_path[0]&&!strcmp(selected_path+7,file.fname);strcpy((char*)row+16,file.fname);
   count++;aux_work_checkpoint();
  }
  if(!ended)out[7]=2;
  if(f_closedir(&dir)!=FR_OK){close_fault=true;ok=false;}
  if(!ok)return 5;
  out[6]=(uint8_t)count;*out_size=16+112u*count;return 2;
 }
 if(op!=12||cursor||n<(gb?5u:6u)||n>96||data[n-1]||memchr(data,0,n-1)||!name_ok((const char*)data,gb))return 3;
 uint8_t h[16],save[128]={0};my2k_nes_rom_info r;uint32_t size=0;
 if(info((const char*)data,&size,h,&r,gb))return 8;
 FILINFO st;if(f_stat(configs[gb][1],&st)!=FR_NO_FILE||f_stat(configs[gb][2],&st)!=FR_NO_FILE)return 9;
 memcpy(save,gb?"GRC1":"NRC1",4);save[4]=1;save[5]=(uint8_t)(n-1);memcpy(save+8,data,n);
 put(save+104,size);put(save+108,my2k_oled_crc(h,16));put(save+124,my2k_oled_crc(save,124));
 FIL f;UINT wrote=0;
 if(f_open(&f,configs[gb][1],FA_CREATE_NEW|FA_WRITE)!=FR_OK)return 5;
 bool ok=f_write(&f,save,128,&wrote)==FR_OK&&wrote==128&&f_sync(&f)==FR_OK;
 if(!close_file(&f))ok=false;
 if(!ok)return 5; /* Retain uncertain temporary record for inspection. */
 FRESULT old=f_stat(configs[gb][0],&st);bool had_old=old==FR_OK;
 if(old!=FR_OK&&old!=FR_NO_FILE)return 5;
 if(had_old&&f_rename(configs[gb][0],configs[gb][2])!=FR_OK)return 5;
 if(f_rename(configs[gb][1],configs[gb][0])!=FR_OK)return 9;
 uint8_t verify[128];if(!record(configs[gb][0],verify,gb)||memcmp(verify,save,128))return 9;
 if(had_old&&f_unlink(configs[gb][2])!=FR_OK)return 9;
 memcpy(out,gb?"GRS1":"NRS1",4);put(out+4,word(save+124));*out_size=8;return 2;
}
static void save_path(char out[64],const char *name,const char *ext,unsigned gb){
 static const char hex[]="0123456789abcdef";uint32_t crc=my2k_oled_crc((const uint8_t*)name,strlen(name));
 strcpy(out,gb?"0:/gbc/saves/":"0:/nes/saves/");for(unsigned i=0;i<8;i++)out[13+i]=hex[(crc>>(28u-4u*i))&15u];
 strcpy(out+21,ext);
}
static int load_ram(const char *path,const char *name,uint8_t *ram,unsigned bytes,uint8_t *rtc,unsigned gb){
 FIL f;UINT got=0;uint8_t h[128];FRESULT opened=f_open(&f,path,FA_READ);
 if(opened==FR_NO_FILE||opened==FR_NO_PATH)return 0;
 if(opened!=FR_OK)return -1;
 bool ok=f_size(&f)==128u+bytes&&f_read(&f,h,128,&got)==FR_OK&&got==128&&
         !memcmp(h,gb?"GSV1":"NSV1",4)&&h[4]==1&&h[5]==strlen(name)&&!memcmp(h+8,name,strlen(name)+1)&&
         word(h+124)==my2k_oled_crc(h,124);
 if(ok)ok=f_read(&f,ram,bytes,&got)==FR_OK&&got==bytes&&word(h+104)==my2k_oled_crc(ram,bytes)&&(!gb||word(h+108)==bytes);
 if(!close_file(&f))ok=false;
 if(ok&&rtc)memcpy(rtc,h+112,5);
 aux_work_checkpoint();return ok?1:-1;
}
static int ram_operation(const char *name,uint8_t *ram,uint8_t *scratch,bool save,unsigned bytes,uint8_t *rtc,unsigned gb){
 if(close_fault||!name_ok(name,gb)||!ram||!scratch||bytes>(gb?65536u:8192u))return -1;
 if(gb&&bytes!=0&&bytes!=256&&bytes!=512&&bytes!=2048&&bytes!=8192&&bytes!=32768&&bytes!=65536)return -1;
 char final[64],tmp[64],backup[64];save_path(final,name,".sav",gb);save_path(tmp,name,".tmp",gb);save_path(backup,name,".bak",gb);
 if(!save){int found=load_ram(final,name,ram,bytes,rtc,gb);if(found==1)return 1;if(close_fault)return -1;int old=load_ram(backup,name,ram,bytes,rtc,gb);return old==1?1:found<0||old<0?-1:0;}
 FILINFO st;FRESULT dir=f_mkdir(gb?"0:/gbc/saves":"0:/nes/saves");if(dir!=FR_OK&&(dir!=FR_EXIST||f_stat(gb?"0:/gbc/saves":"0:/nes/saves",&st)!=FR_OK||!(st.fattrib&AM_DIR)))return -1;
 if(f_stat(tmp,&st)!=FR_NO_FILE||f_stat(backup,&st)!=FR_NO_FILE)return -1;
 FRESULT current=f_stat(final,&st);bool had=current==FR_OK;
 if(current!=FR_OK&&current!=FR_NO_FILE)return -1;
 if(had&&load_ram(final,name,scratch,bytes,NULL,gb)!=1)return -1; /* Never overwrite foreign/corrupt saves. */
 uint8_t h[128]={0};memcpy(h,gb?"GSV1":"NSV1",4);h[4]=1;h[5]=(uint8_t)strlen(name);strcpy((char*)h+8,name);
 put(h+104,my2k_oled_crc(ram,bytes));if(gb)put(h+108,bytes);if(rtc)memcpy(h+112,rtc,5);put(h+124,my2k_oled_crc(h,124));
 FIL f;UINT wrote=0;if(f_open(&f,tmp,FA_WRITE|FA_CREATE_NEW)!=FR_OK)return -1;
 bool ok=f_write(&f,h,128,&wrote)==FR_OK&&wrote==128&&f_write(&f,ram,bytes,&wrote)==FR_OK&&wrote==bytes&&f_sync(&f)==FR_OK;
 if(!close_file(&f))ok=false;
 if(!ok||load_ram(tmp,name,scratch,bytes,NULL,gb)!=1||memcmp(ram,scratch,bytes))return -1;
 if(had&&f_rename(final,backup)!=FR_OK)return -1;
 if(f_rename(tmp,final)!=FR_OK)return -1;
 if(load_ram(final,name,scratch,bytes,NULL,gb)!=1||memcmp(ram,scratch,bytes))return -1;
 if(had&&f_unlink(backup)!=FR_OK)return -1;
 return 1;
}
int aux_nes_library_ram(const char *name,uint8_t ram[8192],uint8_t scratch[8192],bool save){
 if(save_writing)return -1;
 save_writing=save;
 int result=ram_operation(name,ram,scratch,save,8192,NULL,0);
 save_writing=false;
 return result;
}

bool aux_nes_library_selected(char p[160]){return selected(p,0);}
bool aux_gbc_library_selected(char p[160]){return selected(p,1);}
uint8_t aux_nes_library_request(unsigned op,uint32_t c,const uint8_t *d,unsigned n,uint8_t *o,unsigned *s){return request(op,c,d,n,o,s,0);}
uint8_t aux_gbc_library_request(unsigned op,uint32_t c,const uint8_t *d,unsigned n,uint8_t *o,unsigned *s){return request(op-2u,c,d,n,o,s,1);}
int aux_gbc_library_ram(const char *name,uint8_t *ram,uint8_t *scratch,unsigned bytes,uint8_t rtc[5],bool save){
 if(save_writing)return -1;
 save_writing=save;
 int result=ram_operation(name,ram,scratch,save,bytes,rtc,1);save_writing=false;return result;
}
