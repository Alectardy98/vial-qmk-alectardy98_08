// SPDX-License-Identifier: GPL-3.0-or-later
// Included by calibration.c: reuse its canonical record, CRC and verified store.
static const bool va_active=true;
static uint8_t va_overrides[CELLS];
static uint16_t va_revision,va_saved;
static uint8_t va_status; // 0 verified saved, 1 applied/pending, 2 saving, 3 error, 4 calibration required (RAM only)
static uint32_t va_changed;
static bool store_raw(const eeprom_ec_config_t *record);
#define RADIX 307100U
static bool code(uint16_t w,uint32_t *c){
 if(w==0||w==0x2000){*c=w?1:0;return true;}
 unsigned b=w&1023,flags=w&~1023U;if(!b)return false;
 if(flags==0x4000)*c=1+b;
 else if(flags==0x6000)*c=1024+b;
 else if(flags==0xc000)*c=2047+b;
 else return false;
 return true;
}
static bool codec_pack(uint8_t b[208],const eeprom_ec_config_t *canon,const uint8_t overrides[CELLS],uint8_t extra){
 if(extra>15)return false;
 memset(b,0,208);b[0]=extra;
 for(unsigned i=0;i<CELLS;i++){
  if(!sampled(i))continue;
  uint32_t c;if(!code(cell(canon,i),&c)||overrides[i]>99)return false;
  uint32_t carry=c*100+overrides[i];
  for(unsigned j=0;j<208;j++){uint32_t v=(uint32_t)b[j]*RADIX+carry;b[j]=v;carry=v>>8;}
  if(carry)return false;
 }return true;
}
static bool codec_unpack(uint8_t b[208],eeprom_ec_config_t *canon,uint8_t overrides[CELLS],uint8_t *extra){
 for(unsigned i=CELLS;i-->0;){
  if(!sampled(i))continue;
  uint32_t carry=0;
  for(unsigned j=208;j-->0;){uint32_t v=(carry<<8)|b[j];b[j]=v/RADIX;carry=v%RADIX;}
  if(overrides)overrides[i]=carry%100;
  unsigned c=carry/100;uint16_t word;
  if(c<2)word=c?0x2000:0;
  else{unsigned group=(c-2)/1023,bottom=(c-2)%1023+1;static const uint16_t flags[]={0x4000,0x6000,0xc000};word=flags[group]|bottom;}
  if(canon)setcell(canon,i,word);
 }
 *extra=b[0];if(*extra>15)return false;
 for(unsigned j=1;j<208;j++)if(b[j])return false;
 return true;
}

static bool packed(const eeprom_ec_config_t *x){
    return cell(x,EC_CAL_MAGIC_INDEX)==EC_CAL_MAGIC && (cell(x,EC_CAL_METHOD_INDEX)&0xfe)==0x40;
}
static bool crc_valid(const eeprom_ec_config_t *x){
    return checksum(x)==((uint32_t)cell(x,EC_CAL_CRC_HI_INDEX)<<16|cell(x,EC_CAL_CRC_LO_INDEX));
}
static bool reserved_byte(unsigned i){return i==0||i==7||i==8||i==83||i==84||i==125||i==126||i==133||i==134||i==165||i==166;}
__attribute__((noinline)) static bool unpack_record(const eeprom_ec_config_t *x,eeprom_ec_config_t *canon,uint8_t *overrides){
    if(!packed(x)||!crc_valid(x))return false;
    uint8_t b[208],extra;unsigned j=0;
    const uint8_t *raw=(const uint8_t*)x;
    for(unsigned i=0;i<RECORD_BYTES;i++)if(!reserved_byte(i))b[j++]=raw[i];
    if(canon)memset(canon,0,sizeof(*canon));
    if(!codec_unpack(b,canon,overrides,&extra))return false;
    uint32_t fields=(uint32_t)raw[0]|(uint32_t)raw[7]<<8|(uint32_t)raw[8]<<16|(uint32_t)raw[126]<<24;
    unsigned press=fields&1023,release=(fields>>10)&1023,rtp=(fields>>20)&255,rtr=(fields>>28)|((unsigned)extra<<4);
    if(!press||press>=1023||release>=press||!release||!rtp||!rtr)return false;
    if(canon){
        canon->actuation_mode=raw[125]&1;
        canon->mode_0_actuation_threshold=press;canon->mode_0_release_threshold=release;
        canon->mode_1_initial_deadzone_offset=press;canon->mode_1_actuation_offset=rtp;canon->mode_1_release_offset=rtr;
    }
    if(canon)seal(canon,EC_CAL_PARTIAL);
    return true;
}
static bool pack_record(const eeprom_ec_config_t *canon,eeprom_ec_config_t *x){
    if(method(canon)!=EC_CAL_PARTIAL||!ec_cal_record_valid(canon))return false;
    uint8_t b[208];unsigned j=0;
    uint32_t f=canon->mode_0_actuation_threshold|((uint32_t)canon->mode_0_release_threshold<<10)|((uint32_t)canon->mode_1_actuation_offset<<20)|((uint32_t)canon->mode_1_release_offset<<28);
    if(!codec_pack(b,canon,va_overrides,canon->mode_1_release_offset>>4))return false;
    uint8_t *raw=(uint8_t*)x;memset(x,0,sizeof(*x));j=0;
    for(unsigned i=0;i<RECORD_BYTES;i++)if(!reserved_byte(i))raw[i]=b[j++];
    raw[0]=f;raw[7]=f>>8;raw[8]=f>>16;raw[126]=f>>24;raw[125]=0x40|canon->actuation_mode;
    setcell(x,EC_CAL_MAGIC_INDEX,EC_CAL_MAGIC);
    uint32_t crc=checksum(x);setcell(x,EC_CAL_CRC_LO_INDEX,crc);setcell(x,EC_CAL_CRC_HI_INDEX,crc>>16);
    return true;
}
void ec_cal_empty(void){
    memset(&eeprom_ec_config,0,sizeof eeprom_ec_config);
    memset(va_overrides,0,sizeof va_overrides);
    eeprom_ec_config.mode_0_actuation_threshold=DEFAULT_MODE_0_ACTUATION_LEVEL;
    eeprom_ec_config.mode_0_release_threshold=DEFAULT_MODE_0_RELEASE_LEVEL;
    eeprom_ec_config.mode_1_initial_deadzone_offset=DEFAULT_MODE_0_ACTUATION_LEVEL;
    eeprom_ec_config.mode_1_actuation_offset=DEFAULT_MODE_1_ACTUATION_OFFSET;
    eeprom_ec_config.mode_1_release_offset=DEFAULT_MODE_1_RELEASE_OFFSET;
    seal(&eeprom_ec_config,EC_CAL_PARTIAL);
    va_status=4;va_revision=va_saved=0;
}
void ec_va_load(void){
    va_status=0;va_revision=va_saved=0;memset(va_overrides,0,sizeof va_overrides);
    eeprom_ec_config_t decoded;
    if(unpack_record(&eeprom_ec_config,&decoded,va_overrides)&&ec_cal_record_valid(&decoded))eeprom_ec_config=decoded;
    else ec_cal_empty();
}
bool ec_cal_store(const eeprom_ec_config_t *record){
    eeprom_ec_config_t encoded;
    if(!pack_record(record,&encoded)||!store_raw(&encoded))return false;
    va_status=0;va_saved=va_revision;return true;
}
