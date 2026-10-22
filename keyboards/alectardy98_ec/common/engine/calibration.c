// SPDX-License-Identifier: GPL-3.0-or-later
/* Copyright 2023 Cipulot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Cipulot attribution applies to adapted EC runtime fields/threshold scaling. Transactional capture, qualification and explicit record handling are later EC42 work.
 * Modified for the EC42 framework; attribution updated 2026-10-21. */
// Explicit RAM capture -> review -> validate -> activate -> verify -> save.
// No calibration deadlines, ADC conversions, or unchecked persistence in USB callbacks.
#include "calibration.h"
#include "ec_diagnostic.h"
#include "diag.h"
#include <string.h>
#define CELLS (MATRIX_ROWS*MATRIX_COLS)
#define RECORD_BYTES sizeof(eeprom_ec_config_t)
uint8_t ec_cal_state;
uint16_t ec_cal_session;
static eeprom_ec_config_t candidate, previous;
static uint16_t low[CELLS],high[CELLS],latest[CELLS],window_low[CELLS],window_high[CELLS];
static uint8_t counts[CELLS], activity[CELLS], window_count[CELLS];
static bool active_valid,evidence_required;
bool ec_cal_active_valid(void){return active_valid;}
void ec_load_runtime_record(void);
static uint16_t g16(const uint8_t *p){return (uint16_t)p[0]<<8|p[1];}
static void p16(uint8_t *p,uint16_t n){p[0]=n>>8;p[1]=n;}
static uint16_t cell(const eeprom_ec_config_t *x,unsigned i){
uint16_t v;memcpy(&v,(const uint8_t*)x+9+2*i,2);return v;
}
static void setcell(eeprom_ec_config_t *x,unsigned i,uint16_t v){
memcpy((uint8_t*)x+9+2*i,&v,2);
}
static bool sampled(unsigned i){return !is_unused_position(i/MATRIX_COLS,i%MATRIX_COLS);}
static uint8_t method(const eeprom_ec_config_t *x){return cell(x,EC_CAL_MAGIC_INDEX)==EC_CAL_MAGIC&&cell(x,EC_CAL_METHOD_INDEX)==EC_CAL_PARTIAL?EC_CAL_PARTIAL:255;}
uint8_t ec_cal_method(void){return method(&eeprom_ec_config);}
uint16_t ec_cal_bottom(uint8_t r,uint8_t c){unsigned v=cell(&eeprom_ec_config,r*MATRIX_COLS+c);return v&1023;}
bool ec_cal_enabled(uint8_t r,uint8_t c){return active_valid&&(cell(&eeprom_ec_config,r*MATRIX_COLS+c)&EC_CAL_ENABLED)&&ec_config.rescaled_mode_0_actuation_threshold[r][c]<1023;}
static uint32_t checksum(const eeprom_ec_config_t *x){
    uint32_t crc=0xffffffff;const uint8_t *b=(const uint8_t*)x;
    for(unsigned i=0;i<sizeof(*x);++i){
        unsigned slot=i>=9?(i-9)/2:CELLS;
        uint8_t v=(slot==EC_CAL_CRC_LO_INDEX||slot==EC_CAL_CRC_HI_INDEX)?0:b[i];
        crc^=v;for(uint8_t j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320:0);
    }return ~crc;
}
static void seal(eeprom_ec_config_t *x,uint8_t m){
    setcell(x,EC_CAL_MAGIC_INDEX,EC_CAL_MAGIC);setcell(x,EC_CAL_METHOD_INDEX,m);
    uint32_t crc=checksum(x);setcell(x,EC_CAL_CRC_LO_INDEX,crc);setcell(x,EC_CAL_CRC_HI_INDEX,crc>>16);
}
#include "record_storage.inc"
bool ec_cal_record_valid(const eeprom_ec_config_t *x){
    if((cell(x,EC_CAL_METHOD_INDEX)&0xfe)==0x40&&!packed(x))return false;
    if(packed(x))return unpack_record(x,0,0);
    uint8_t m=method(x);if(m!=EC_CAL_PARTIAL)return false;
    if(m==EC_CAL_PARTIAL)for(unsigned i=0;i<CELLS;++i){if(!sampled(i))continue;unsigned v=cell(x,i);
        if(((v&EC_CAL_QUALIFIED)&&!(v&1023))||(v&0x1c00)||((v&EC_CAL_ENABLED)&&(!(v&EC_CAL_QUALIFIED)||(v&EC_CAL_RETRY)))||(!(v&EC_CAL_QUALIFIED)&&(v&~EC_CAL_RETRY)))return false;
    }
    return m==EC_CAL_PARTIAL&&checksum(x)==((uint32_t)cell(x,EC_CAL_CRC_HI_INDEX)<<16|cell(x,EC_CAL_CRC_LO_INDEX));
}
static bool manual_boundaries(const eeprom_ec_config_t *x,unsigned i,uint16_t v,uint16_t *p,uint16_t *r,uint8_t override){
    uint16_t n=ec_config.noise_floor[i/MATRIX_COLS][i%MATRIX_COLS];
    if(v>1023||v<n+BOTTOMING_CALIBRATION_THRESHOLD||x->mode_0_actuation_threshold>=1023||x->mode_0_release_threshold>=x->mode_0_actuation_threshold)return false;
    uint16_t press=override?(override==99?1022:((unsigned)override*1023+50)/100):x->mode_0_actuation_threshold;
    unsigned gap=x->mode_0_actuation_threshold-x->mode_0_release_threshold;
    if(press<=gap)return false;
    *p=rescale(press,0,1023,n,v);*r=rescale(press-gap,0,1023,n,v);
    // Keep room for a strict RT movement above the floor, even at deepest VA.
    if(*r>v-6)*r=v-6;
    return *p>*r&&*r>n&&*p<v;
}
static bool boundaries(const eeprom_ec_config_t *x,unsigned i,uint16_t *p,uint16_t *r){
    uint16_t v=cell(x,i);
    if(method(x)==EC_CAL_PARTIAL){if(!(v&EC_CAL_ENABLED))return false;v&=1023;}
    return manual_boundaries(x,i,v,p,r,method(x)==EC_CAL_PARTIAL?va_overrides[i]:0);
}
static void load(void){
    ec_config.actuation_mode=eeprom_ec_config.actuation_mode;
    ec_config.mode_0_actuation_threshold=eeprom_ec_config.mode_0_actuation_threshold;
    ec_config.mode_0_release_threshold=eeprom_ec_config.mode_0_release_threshold;
    ec_config.mode_1_initial_deadzone_offset=eeprom_ec_config.mode_1_initial_deadzone_offset;
    ec_config.mode_1_actuation_offset=eeprom_ec_config.mode_1_actuation_offset;
    ec_config.mode_1_release_offset=eeprom_ec_config.mode_1_release_offset;
}
bool ec_cal_runtime(void){
    active_valid=ec_cal_record_valid(&eeprom_ec_config);
    bool valid=active_valid;load();
    if(!valid)ec_config.actuation_mode=0;
    for(unsigned i=0;i<CELLS;++i){uint8_t r=i/MATRIX_COLS,c=i%MATRIX_COLS;uint16_t p=1023,q=1022;
        bool ok=valid&&sampled(i)&&boundaries(&eeprom_ec_config,i,&p,&q);
        if(!ok){p=1023;q=1022;}
        ec_config.rescaled_mode_0_actuation_threshold[r][c]=p;
        ec_config.rescaled_mode_0_release_threshold[r][c]=q;
        ec_config.extremum[r][c]=ec_config.noise_floor[r][c];
        activity[i]&=~4;window_low[i]=window_high[i]=ec_config.noise_floor[r][c];
    }return true;
}
bool ec_cal_import_valid(const eeprom_ec_config_t *record){
    eeprom_ec_config_t decoded;uint8_t overrides[CELLS]={0};bool is_packed=packed(record);
    if(!is_packed||!unpack_record(record,&decoded,overrides))return false;
    record=&decoded;
    if(!ec_cal_record_valid(record)||record->actuation_mode>1||!record->mode_0_actuation_threshold||record->mode_0_actuation_threshold>1023||record->mode_0_release_threshold>=record->mode_0_actuation_threshold||!record->mode_1_initial_deadzone_offset||record->mode_1_initial_deadzone_offset>1023||!record->mode_1_actuation_offset||!record->mode_1_release_offset)return false;
    for(unsigned i=0;i<CELLS;++i){if(!sampled(i))continue;uint16_t p,r;
        if(!(cell(record,i)&EC_CAL_ENABLED))continue;
        unsigned v=cell(record,i)&1023;
        if(!manual_boundaries(record,i,v,&p,&r,overrides[i]))return false;
        if(record->actuation_mode){unsigned lo=4,hi=v-r-2;
            if(record->mode_1_actuation_offset<lo||record->mode_1_release_offset<lo||record->mode_1_actuation_offset>hi||record->mode_1_release_offset>hi)return false;
        }
    }return true;
}
__attribute__((cold)) static bool store_raw(const eeprom_ec_config_t *record){
    if(!ec_record_storage_ready()||!packed(record)||!ec_cal_record_valid(record))return false;
    // Quarantine BEFORE changing payload. Torn writes remain tagged/CRC-invalid.
    // Only packed records may reach this persistence boundary.
    eeprom_ec_config_t check;
    if(ec_record_read(&check,0,sizeof(check))!=sizeof(check))return false;
    uint16_t magic=EC_CAL_MAGIC;setcell(&check,EC_CAL_MAGIC_INDEX,magic);
    uint16_t invalid=(uint16_t)checksum(&check)^0xffff;
    const unsigned mo=9+EC_CAL_MAGIC_INDEX*2,co=9+EC_CAL_CRC_LO_INDEX*2;
    ec_record_write(&magic,mo,2);ec_record_write(&invalid,co,2);
    if(ec_record_read(&check,0,sizeof(check))!=sizeof(check))return false;
    if(cell(&check,EC_CAL_MAGIC_INDEX)!=EC_CAL_MAGIC||ec_cal_record_valid(&check))return false;
    ec_record_write(record,0,mo);
    ec_record_write((const uint8_t*)record+mo+2,mo+2,sizeof(*record)-mo-2);
    if(ec_record_read(&check,0,sizeof(check))!=sizeof(check))return false;
    setcell(&check,EC_CAL_MAGIC_INDEX,cell(record,EC_CAL_MAGIC_INDEX));
    if(memcmp(&check,record,sizeof(check)))return false;
    magic=cell(record,EC_CAL_MAGIC_INDEX);ec_record_write(&magic,mo,2);
    if(ec_record_read(&check,0,sizeof(check))!=sizeof(check))return false;
    return ec_record_storage_ready()&&!memcmp(&check,record,sizeof(check))&&ec_cal_record_valid(&check);
}
static bool covered(unsigned i){
    return counts[i]>=16&&high[i]>=low[i]+BOTTOMING_CALIBRATION_THRESHOLD&&high[i]>=ec_config.noise_floor[i/MATRIX_COLS][i%MATRIX_COLS]+BOTTOMING_CALIBRATION_THRESHOLD;
}
// Session statuses: 0 untouched, 1 firmware-valid, 2 needs sustained movement,
// 3 invalid proposed thresholds/RT range, 4 firmware-impossible.
static bool key_valid(unsigned i,uint16_t v,bool evidence){
    uint16_t p,r;if(!manual_boundaries(&candidate,i,v,&p,&r,va_overrides[i]))return false;
    if(evidence&&r<=low[i])return false;
    if(candidate.actuation_mode){
        unsigned max=v>r+1?v-r-2:0,min=4;
        if(candidate.mode_1_actuation_offset<min||candidate.mode_1_release_offset<min||candidate.mode_1_actuation_offset>max||candidate.mode_1_release_offset>max)return false;
    }return true;
}
static uint8_t capture_status(unsigned i){
    if(!sampled(i))return 4;
    if(counts[i]<16||high[i]<low[i]+BOTTOMING_CALIBRATION_THRESHOLD)return 0;
    if(!covered(i))return 2;
    return key_valid(i,high[i],true)?1:3;
}
uint8_t ec_cal_cell_flags(uint8_t r,uint8_t c){
    unsigned i=r*MATRIX_COLS+c;uint16_t p,q;
    bool valid=active_valid&&boundaries(&eeprom_ec_config,i,&p,&q);
    return (valid?8:0)|((active_valid&&ec_cal_method()==EC_CAL_PARTIAL&&(cell(&eeprom_ec_config,i)&EC_CAL_QUALIFIED))?128:0)|((ec_cal_state==CAL_MANUAL||ec_cal_state==CAL_REVIEW||ec_cal_state==CAL_READY)?capture_status(i)<<4:0);
}
void ec_cal_abort(void){
    if(ec_cal_state==CAL_APPLIED){eeprom_ec_config=previous;ec_load_runtime_record();}
    ec_cal_state=CAL_IDLE;ec_config.bottoming_calibration=false;
    ec_cal_runtime();
}
void ec_cal_sample(uint8_t r,uint8_t c,uint16_t raw){
    unsigned i=r*MATRIX_COLS+c;
    latest[i]=raw;
    if(ec_cal_state!=CAL_MANUAL||!sampled(i))return;
    if(counts[i]!=255)++counts[i];
    if(raw==1023)activity[i]|=2;
    // Sustained 16-scan block qualification, never raw/logical key activity.
    if(!window_count[i]||raw<window_low[i])window_low[i]=raw;
    if(!window_count[i]||raw>window_high[i])window_high[i]=raw;
    if(++window_count[i]==16){
        if(window_high[i]<low[i])low[i]=window_high[i];
        if(window_low[i]>high[i])high[i]=window_low[i];
        window_count[i]=0;
    }
    ec_config.bottoming_reading[r][c]=high[i];
}

static bool validate(void){
    if(!ec_record_storage_ready())return false;
    if(candidate.actuation_mode>1||!candidate.mode_0_actuation_threshold||candidate.mode_0_actuation_threshold>=1023||candidate.mode_0_release_threshold>=candidate.mode_0_actuation_threshold||!candidate.mode_1_initial_deadzone_offset||candidate.mode_1_initial_deadzone_offset>=1023||!candidate.mode_1_actuation_offset||!candidate.mode_1_release_offset)return false;
    if(method(&candidate)!=EC_CAL_PARTIAL)return false;
    bool any=false;
    for(unsigned i=0;i<CELLS;++i){if(!sampled(i))continue;
        if(evidence_required&&capture_status(i)==1)any=true;
        if(!(cell(&candidate,i)&EC_CAL_ENABLED))continue;
        if(!key_valid(i,cell(&candidate,i)&1023,false))return false;
        if(!evidence_required)any=true;
    }return any;
}

// Canonical native words are LE, wire words BE; override tail stays bytes.
_Static_assert(__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__,"EC record byte order");
static unsigned native_offset(unsigned o){
if(o>=9+2*CELLS+8)return o;
return (o==0||o==7||o==8)?o:((o&1)?o+1:o-1);}
uint8_t ec_record_byte(const eeprom_ec_config_t *x,unsigned o){return ((const uint8_t*)x)[native_offset(o)];}
void ec_record_set_byte(eeprom_ec_config_t *x,unsigned o,uint8_t v){((uint8_t*)x)[native_offset(o)]=v;}
bool ec_cal_protocol(uint8_t op,const uint8_t *p,uint8_t n,uint8_t *out,uint8_t *length,uint8_t *error){
    if(ec_va_protocol(op,p,n,out,length,error))return true;
    if(op<0x20||op>0x27)return false;
    *length=0;*error=1;
    if(op==0x20){
        if(n==1&&p[0]==4){ec_cal_abort();*error=0;return true;}
        if(n==1&&p[0]==6&&(ec_cal_state==CAL_REVIEW||ec_cal_state==CAL_READY)){memset(window_count,0,sizeof(window_count));ec_cal_state=CAL_MANUAL;*error=0;return true;}
        // Retry only the selected cell; clear its proposal/evidence, never EEPROM.
        if(n==3&&p[0]==5&&ec_cal_state==CAL_MANUAL&&g16(p+1)<CELLS){
            unsigned i=g16(p+1);if(!sampled(i))return true;
            counts[i]=activity[i]=window_count[i]=0;low[i]=1023;high[i]=0;setcell(&candidate,i,ec_cal_method()==EC_CAL_PARTIAL&&active_valid?cell(&eeprom_ec_config,i):0);*error=0;return true;
        }
        if((n==1||n==2)&&p[0]==1&&(n==1||p[1]<=1)){
            if((va_status&&va_status!=4)||(ec_cal_state!=CAL_IDLE&&ec_cal_state!=CAL_REVIEW&&ec_cal_state!=CAL_READY))return true;
            candidate=eeprom_ec_config;if(n==2)candidate.actuation_mode=p[1];
            setcell(&candidate,EC_CAL_MAGIC_INDEX,EC_CAL_MAGIC);setcell(&candidate,EC_CAL_METHOD_INDEX,EC_CAL_PARTIAL);
            memset(counts,0,sizeof(counts));memset(activity,0,sizeof(activity));memset(window_count,0,sizeof(window_count));
            for(unsigned i=0;i<CELLS;++i){low[i]=1023;high[i]=0;}
            evidence_required=true;if(++ec_cal_session==0)++ec_cal_session;
            ec_cal_state=CAL_MANUAL;ec_config.bottoming_calibration=true;
            p16(out,ec_cal_session);*length=2;*error=0;return true;
        }
        if(n==1&&p[0]==3&&ec_cal_state==CAL_MANUAL){
            for(unsigned i=0;i<CELLS;++i){if(!sampled(i))continue;unsigned status=capture_status(i);
                if(status==1)setcell(&candidate,i,high[i]|EC_CAL_QUALIFIED|EC_CAL_ENABLED);
                else if(status==2||status==3)setcell(&candidate,i,(cell(&candidate,i)&~EC_CAL_ENABLED)|EC_CAL_RETRY);
            }
            seal(&candidate,EC_CAL_PARTIAL);ec_cal_state=CAL_REVIEW;*error=0;return true;
        }return true;
    }
    if(n<2||g16(p)!=ec_cal_session||!ec_cal_session){*error=4;return true;}
    if(op==0x23){
        if(n!=5||!p[4]||p[4]>17||g16(p+2)+p[4]>RECORD_BYTES||ec_cal_state<CAL_REVIEW)return true;
        memcpy(out,p+2,3);for(unsigned j=0;j<p[4];++j)out[3+j]=ec_record_byte(&candidate,g16(p+2)+j);*length=3+p[4];*error=0;return true;
    }
    if(op==0x27){
        if(n!=4||(ec_cal_state!=CAL_REVIEW&&ec_cal_state!=CAL_MANUAL)||g16(p+2)>=CELLS)return true;
        unsigned i=g16(p+2);p16(out,i);p16(out+2,low[i]);p16(out+4,high[i]);out[6]=counts[i];out[7]=activity[i];out[8]=capture_status(i);*length=9;*error=0;return true;
    }
    if(n!=2)return true;
    if(op==0x22){
        if(ec_cal_state==CAL_READY){*error=validate()?0:3;return true;}
        if(ec_cal_state!=CAL_REVIEW)return true;
        seal(&candidate,EC_CAL_PARTIAL);
        if(!validate()){*error=3;return true;}ec_cal_state=CAL_READY;*error=0;return true;
    }
    if(op==0x24){
        if(ec_cal_state!=CAL_READY||!validate()){*error=3;return true;}
        for(unsigned i=0;i<CELLS;++i){if(!sampled(i)||!(cell(&candidate,i)&EC_CAL_ENABLED))continue;uint16_t a,b;
            if(!boundaries(&candidate,i,&a,&b)||(latest[i]>=b)){*error=3;return true;}}
        previous=eeprom_ec_config;eeprom_ec_config=candidate;ec_cal_runtime();ec_cal_state=CAL_APPLIED;
        for(unsigned i=0;i<CELLS;++i){if(!sampled(i)||!(cell(&candidate,i)&EC_CAL_ENABLED))continue;uint16_t a,b;boundaries(&candidate,i,&a,&b);
            if(a!=ec_config.rescaled_mode_0_actuation_threshold[i/MATRIX_COLS][i%MATRIX_COLS]||b!=ec_config.rescaled_mode_0_release_threshold[i/MATRIX_COLS][i%MATRIX_COLS]){ec_cal_abort();return true;}}
        *error=0;return true;
    }
    if(op==0x25){
        if(ec_cal_state!=CAL_APPLIED)return true;
        for(unsigned i=0;i<CELLS;++i){if(!sampled(i)||!(cell(&candidate,i)&EC_CAL_ENABLED))continue;uint16_t a,b;
            if(!boundaries(&candidate,i,&a,&b)||( (latest[i]>=b||(matrix_get_row(i/MATRIX_COLS)&((matrix_row_t)1<<(i%MATRIX_COLS)))))){ec_cal_abort();*error=3;return true;}}
        if(!ec_cal_store(&candidate)){ec_cal_abort();*error=3;return true;}
        ec_cal_state=CAL_IDLE;ec_config.bottoming_calibration=false;
    ec_cal_runtime();*error=0;return true;
    }
    return true;
}

#include "actuation_control.inc"
