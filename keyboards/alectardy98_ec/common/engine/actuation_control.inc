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
/* Dynamic movement/extremum behavior adapts Cipulot ec_update_key. USB controls, filtering, VA-first activation and persistence are later EC42 work.
 * Modified for the EC42 framework; attribution updated 2026-10-21. */
// Foreground only. USB commands and scanner task are serialized by QMK.
bool ec_actuators_blocked(void);
static bool va_released(void){
    for(unsigned row=0;row<MATRIX_ROWS;row++)if(matrix_get_row(row))return false;
    for(unsigned i=0;i<CELLS;i++)if(sampled(i)&&(cell(&candidate,i)&EC_CAL_ENABLED)){
        uint16_t p,r;if(!manual_boundaries(&candidate,i,cell(&candidate,i)&1023,&p,&r,va_overrides[i])||latest[i]>=r)return false;
    }return true;
}
void ec_va_thresholds(uint8_t row,uint8_t col){
    unsigned i=row*MATRIX_COLS+col;uint16_t p=1023,r=1022;
    if(!boundaries(&eeprom_ec_config,i,&p,&r)){p=1023;r=1022;}
    ec_config.rescaled_mode_0_actuation_threshold[row][col]=p;
    ec_config.rescaled_mode_0_release_threshold[row][col]=r;
}
void ec_va_disconnected(void){
    if(!va_status)return;
    if(ec_record_read(&eeprom_ec_config,0,sizeof(eeprom_ec_config))!=sizeof(eeprom_ec_config)){active_valid=false;va_status=3;return;}
    ec_va_load();ec_load_runtime_record();
}
void ec_va_task(void){
    if(!va_status||va_status>=3||ec_cal_state||ec_actuators_blocked())return;
    candidate=eeprom_ec_config;
    if(timer_elapsed32(va_changed)<2000||!va_released())return;
    if(va_status==1){va_status=2;return;}
    va_status=ec_cal_store(&eeprom_ec_config)?0:3;
    if(!va_status)va_saved=va_revision;
}
bool ec_cal_restore_record(const eeprom_ec_config_t *record){
    if(!ec_cal_import_valid(record))return false;
    ec_cal_abort();return store_raw(record);
}
bool ec_va_protocol(uint8_t op,const uint8_t *p,uint8_t n,uint8_t *out,uint8_t *length,uint8_t *error){
    if(op!=0x28&&op!=0x29&&op!=0x2a)return false;
    *length=0;*error=1;
    if(op==0x28){
        if(n!=2)return true;
        unsigned i=g16(p);if(i!=65535&&(i>=CELLS||!sampled(i))){*error=3;return true;}
        p16(out,va_revision);p16(out+2,va_saved);out[4]=ec_record_storage_ready()?va_status:3;out[5]=1;
        p16(out+6,eeprom_ec_config.mode_0_actuation_threshold);p16(out+8,eeprom_ec_config.mode_0_release_threshold);
        out[10]=eeprom_ec_config.actuation_mode;out[11]=eeprom_ec_config.mode_1_actuation_offset;out[12]=eeprom_ec_config.mode_1_release_offset;out[13]=i<CELLS?va_overrides[i]:0;
        eeprom_ec_config_t saved;
        if(ec_record_read(&saved,0,sizeof saved)!=sizeof saved){*error=3;return true;}
        uint32_t crc=checksum(&saved);p16(out+14,crc>>16);p16(out+16,crc);p16(out+18,i);*length=20;*error=0;return true;
    }
    if(ec_cal_state||!ec_record_storage_ready()||va_status==3){*error=3;return true;}
    if(op==0x2a){
        if(n!=5||g16(p+3)!=va_revision){*error=4;return true;}
        if(p[0]>1||!p[1]||!p[2]){*error=3;return true;}
        candidate=eeprom_ec_config;candidate.actuation_mode=p[0];candidate.mode_1_actuation_offset=p[1];candidate.mode_1_release_offset=p[2];
        for(unsigned i=0;i<CELLS;i++)if(sampled(i)&&(cell(&candidate,i)&EC_CAL_ENABLED)){
            if(!key_valid(i,cell(&candidate,i)&1023,false)){*error=3;return true;}
        }
    }else{
        if(n!=5||g16(p+3)!=va_revision){*error=4;return true;}
        unsigned i=g16(p);uint8_t value=p[2];
        if((i!=65535&&(i>=CELLS||!sampled(i)))||value>99||(i==65535&&!value)){*error=3;return true;}
        candidate=eeprom_ec_config;
        uint8_t old=0;
        if(i==65535){unsigned gap=candidate.mode_0_actuation_threshold-candidate.mode_0_release_threshold;unsigned q=value==99?1022:((unsigned)value*1023+50)/100;if(q<=gap){*error=3;return true;}candidate.mode_0_actuation_threshold=q;candidate.mode_0_release_threshold=q-gap;}
        else{if(value&&(value==99?1022:((unsigned)value*1023+50)/100)<=candidate.mode_0_actuation_threshold-candidate.mode_0_release_threshold){*error=3;return true;}old=va_overrides[i];va_overrides[i]=value;}
        bool ok=va_released();
        // Initial VA and fixed hysteresis do not set RT movement sensitivity.
        // Clamp only offsets that cannot fit the new released-floor active window.
        if(candidate.actuation_mode){
            unsigned hi=255;bool any=false;
            for(unsigned j=0;j<CELLS;j++)if(sampled(j)&&(cell(&candidate,j)&EC_CAL_ENABLED)){
                uint16_t p,r;unsigned bottom=cell(&candidate,j)&1023;any=true;
                if(!manual_boundaries(&candidate,j,bottom,&p,&r,va_overrides[j])||bottom<=r+5){ok=false;break;}
                if(bottom-r-2<hi)hi=bottom-r-2;
            }
            if(any&&ok){
                if(candidate.mode_1_actuation_offset<4)candidate.mode_1_actuation_offset=4;
                if(candidate.mode_1_release_offset<4)candidate.mode_1_release_offset=4;
                if(candidate.mode_1_actuation_offset>hi)candidate.mode_1_actuation_offset=hi;
                if(candidate.mode_1_release_offset>hi)candidate.mode_1_release_offset=hi;
            }
        }
        if(!ok){if(i<CELLS)va_overrides[i]=old;*error=3;return true;}
    }
    if(!va_released()){*error=3;return true;}
    candidate.mode_1_initial_deadzone_offset=candidate.mode_0_actuation_threshold;
    memcpy(candidate.overrides,va_overrides,CELLS);
    seal(&candidate,EC_CAL_PARTIAL);eeprom_ec_config=candidate;ec_cal_runtime();
    if(++va_revision==0)++va_revision;
    va_status=1;va_changed=timer_read32();p16(out,va_revision);*length=2;*error=0;return true;
}

// Median-of-three suppresses isolated ADC spikes; no additional ADC conversions.
// Capture uses these scratch arrays only while non-idle; the RT path is then suppressed.
void ec_rt_boundaries(uint8_t row,uint8_t col,uint16_t *press,uint16_t *release){
    unsigned i=row*MATRIX_COLS+col;
    uint16_t fixed=ec_config.rescaled_mode_0_release_threshold[row][col];
    *press=ec_config.rescaled_mode_0_actuation_threshold[row][col];*release=fixed;
    if(activity[i]&4){
        *press=ec_config.extremum[row][col]+ec_config.mode_1_actuation_offset;
        int r=(int)ec_config.extremum[row][col]-ec_config.mode_1_release_offset;
        *release=r>fixed?r:fixed;
    }
}
bool ec_rt_key(uint8_t row,uint8_t col,uint16_t raw,bool current){
    unsigned i=row*MATRIX_COLS+col;
    if(ec_cal_state)return false;
    uint16_t a=raw,b=window_low[i],c=window_high[i],t;
    window_high[i]=b;window_low[i]=raw;
    if(a>b){t=a;a=b;b=t;}if(b>c){b=c;}if(a>b)b=a;
    uint16_t press,release;ec_rt_boundaries(row,col,&press,&release);
    if(b<=ec_config.rescaled_mode_0_release_threshold[row][col]){activity[i]&=~4;ec_config.extremum[row][col]=b;return false;}
    if(!(activity[i]&4)){
        if(b>press){activity[i]|=4;ec_config.extremum[row][col]=b;return true;}return false;
    }
    if(current){
        if(b>ec_config.extremum[row][col])ec_config.extremum[row][col]=b;
        if(b<release){ec_config.extremum[row][col]=b;return false;}
    }else{
        if(b<ec_config.extremum[row][col])ec_config.extremum[row][col]=b;
        if(b>press){ec_config.extremum[row][col]=b;return true;}
    }
    return current;
}
