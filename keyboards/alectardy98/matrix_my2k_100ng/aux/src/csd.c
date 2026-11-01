/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "aux_platform.h"
static uint32_t bits(const uint32_t w[4],unsigned high,unsigned low){
    uint32_t value=0;for(unsigned n=high+1;n-- >low;)value=(value<<1)|((w[(127u-n)/32u]>>(n%32u))&1u);
    return value;
}
bool aux_csd_sectors(const uint32_t words[4],uint32_t *sectors){
    if(!words||!sectors)return false;
    uint64_t count;
    uint32_t version=bits(words,127,126);
    if(version==1){count=((uint64_t)bits(words,69,48)+1u)*1024u;}
    else if(version==0){
        unsigned block_bits=bits(words,83,80),mult=bits(words,49,47);
        if(block_bits>11u)return false;
        uint64_t bytes=((uint64_t)bits(words,73,62)+1u)<<(mult+2u+block_bits);
        if(bytes<512u||bytes%512u)return false;
        count=bytes/512u;
    }else return false;
    if(!count||count>UINT32_MAX)return false;
    *sectors=(uint32_t)count;return true;
}
