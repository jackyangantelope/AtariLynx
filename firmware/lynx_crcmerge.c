/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_crcmerge.h"
#if R8_LYNX_CRC_MERGE
#ifndef LCM_MODE
#define LCM_MODE 1
#endif
_Static_assert(LCM_MODE==1 || LCM_MODE==2,"Fixed OFF=1 ON=2");
const volatile uint32_t lcm_selector=LCM_MODE;

static uint32_t apply(const uint32_t columns[32],uint32_t value){
    uint32_t result=0;
    for(unsigned bit=0;value;value>>=1,++bit)
        if(value&1u)result^=columns[bit];
    return result;
}

uint32_t lcm_combine(uint32_t first,uint32_t second,size_t second_bytes){
    if(!second_bytes)return first;

    uint32_t storage[2][32];
    uint32_t *op=storage[0],*squared=storage[1];
    for(unsigned bit=0;bit<32;++bit){
        uint32_t value=1u<<bit;
        for(unsigned n=0;n<8;++n)value=(value>>1)^(0xedb88320u&-(value&1u));
        op[bit]=value;
    }
    for(;;){
        if(second_bytes&1u)first=apply(op,first);
        second_bytes>>=1;
        if(!second_bytes)break;
        for(unsigned bit=0;bit<32;++bit)squared[bit]=apply(op,op[bit]);
        uint32_t *swap=op;op=squared;squared=swap;
    }
    return first^second;
}
#endif
