/* SPDX-License-Identifier: BSD-3-Clause
 * Fixed 51-byte arithmetic for the original boot decryptor. Same truncation,
 * conditional subtraction, exponent and accumulation; no guest time changes.
 * Local bounded workspaces replace transient heap allocations. Original generic
 * implementation remains the reference and handles all other lengths/modes.
 */
#include "lynx_bootmem.h"
#include <cstring>
#if R8_LYNX_BOOTMEM
static inline uint32_t read_be(const unsigned char *p){
    uint32_t v;std::memcpy(&v,p,4);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    v=__builtin_bswap32(v);
#endif
    return v;
}
static inline void write_be(unsigned char *p,uint32_t v){
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    v=__builtin_bswap32(v);
#endif
    std::memcpy(p,&v,4);
}
static inline void twice(unsigned char *p){
    uint32_t carry=0;
    for(int i=47;i>=3;i-=4){uint32_t v=read_be(p+i);write_be(p+i,(v<<1)|carry);carry=v>>31;}
    for(int i=2;i>=0;--i){unsigned v=2u*p[i]+carry;p[i]=(unsigned char)v;carry=v>>8;}
}
static inline void add(unsigned char *p,const unsigned char *v){
    uint32_t carry=0;
    for(int i=47;i>=3;i-=4){uint64_t s=(uint64_t)read_be(p+i)+read_be(v+i)+carry;write_be(p+i,(uint32_t)s);carry=(uint32_t)(s>>32);}
    for(int i=2;i>=0;--i){unsigned s=(unsigned)p[i]+v[i]+carry;p[i]=(unsigned char)s;carry=s>>8;}
}
static inline int subtract(unsigned char *p,const unsigned char *v){
    // Inputs are disjoint local result and immutable modulus. Compare before
    // writing so a negative original subtraction leaves every byte unchanged.
    if(std::memcmp(p,v,51)<0)return 0;
    uint32_t borrow=0;
    for(int i=47;i>=3;i-=4){uint32_t a=read_be(p+i);uint64_t b=(uint64_t)read_be(v+i)+borrow;write_be(p+i,a-(uint32_t)b);borrow=(uint64_t)a<b;}
    for(int i=2;i>=0;--i){unsigned a=p[i],b=(unsigned)v[i]+borrow;p[i]=(unsigned char)(a-b);borrow=a<b;}
    return 1;
}
static void multiply(unsigned char *out,const unsigned char *a,const unsigned char *b,const unsigned char *mod){
    std::memset(out,0,51);
    for(unsigned i=0;i<51;++i){unsigned char bits=b[i];
        for(unsigned j=0;j<8;++j){
            twice(out);bool increment=(bits&128)!=0;bits=(unsigned char)(bits<<1);
            if(increment){add(out,a);if(subtract(out,mod))subtract(out,mod);}
            else subtract(out,mod);
        }
    }
}
extern "C" int lbm_decrypt_block(int accumulator,unsigned char *result,const unsigned char *encrypted,const unsigned char *modulus){
    unsigned char a[51],b[51],tmp[51];
    for(unsigned i=0;i<51;++i)b[50-i]=encrypted[i];
    multiply(a,b,b,modulus);std::memcpy(tmp,a,51);multiply(a,b,tmp,modulus);
    for(int i=50;i>0;--i){accumulator=(accumulator+a[i])&255;*result++=(unsigned char)accumulator;}
    return accumulator;
}
#endif
