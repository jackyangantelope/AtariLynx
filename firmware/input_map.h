/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "retro8.h"
static inline bool n3_key(const r8_input *s,unsigned k){for(int i=0;i<6;i++)if(s->keys[i]==k)return true;return false;}
static inline unsigned n3_pad(const r8_input *s){
    unsigned p=s->pads[0];
    if(n3_key(s,0x52))p|=R8_UP;if(n3_key(s,0x51))p|=R8_DOWN;
    if(n3_key(s,0x50))p|=R8_LEFT;if(n3_key(s,0x4f))p|=R8_RIGHT;
    if(n3_key(s,0x1d)||n3_key(s,0x2c))p|=R8_A;
    if(n3_key(s,0x1b))p|=R8_B;if(n3_key(s,0x28))p|=R8_START;
    if(n3_key(s,0x2b))p|=R8_SELECT;
    if(n3_key(s,0x04))p|=R8_X;if(n3_key(s,0x16))p|=R8_Y;
    return p;
}
