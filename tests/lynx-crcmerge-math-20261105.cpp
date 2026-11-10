/* Exercise actual extracted CRC and actual composition code with an independent
 * bit-at-a-time oracle; no emulator services or persistent state involved. */
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "lynx_crc.h"
#include "lynx_crcmerge.h"
#include "native-crc.inc"
static uint32_t rng=0x39c0ffee;
static uint32_t next(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static uint32_t reference(const uint8_t*p,size_t n){
    uint32_t v=~0u;
    while(n--){v^=*p++;for(unsigned b=0;b<8;b++)v=(v&1u)?(v>>1)^0xedb88320u:v>>1;}
    return ~v;
}
static unsigned cases;
static void test(const uint8_t*p,size_t n,size_t split){
    uint32_t a=lynx_crc32(p,split),b=lynx_crc32(p+split,n-split),whole=reference(p,n);
    assert(lynx_crc32(p,n)==whole);
    assert(lcm_combine(a,b,n-split)==whole);
    if(n>=64){
        uint32_t payload=lynx_crc32(p+64,n-64);
        uint32_t merged=lcm_selector==2?lcm_combine(lynx_crc32(p,64),payload,n-64):lynx_crc32(p,n);
        assert(merged==whole&&payload==reference(p+64,n-64));
    }
    ++cases;
}
int main(int argc,char**argv){
    assert(argc==2);const char*known="123456789";
    assert(lynx_crc32(known,9)==0xcbf43926u);
    for(unsigned split=0;split<=9;split++)test((const uint8_t*)known,9,split);
    std::vector<uint8_t> data(1048640+16);
    for(size_t i=0;i<data.size();i++)data[i]=(uint8_t)next();
    for(unsigned i=0;i<4096;i++){
        size_t n=i<1025?i:next()%4097,offset=next()%8,split=next()%(n+1);
        test(data.data()+offset,n,split);
    }
    for(unsigned b0:{0x100u,0x200u,0x400u,0x800u})for(unsigned b1:{0u,0x100u,0x200u,0x400u,0x800u}){
        size_t n=64+(b0+b1)*256;
        for(unsigned offset:{0u,1u,3u})test(data.data()+offset,n,64);
    }
    for(unsigned value:{0u,255u}){
        memset(data.data(),value,65536+64);
        for(unsigned split:{0u,1u,63u,64u,65u,65535u,65536u,65600u})test(data.data(),65600,split);
    }
    FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
    assert(n<=data.size()&&fread(data.data(),1,n,f)==n&&!fclose(f));
    test(data.data(),n,64);
    assert(lynx_crc32(data.data(),n)==0x9b62aaffu&&lynx_crc32(data.data()+64,n-64)==0xdc8713eeu);
    /* Arithmetic length edges and composition associativity without allocating
     * the logical repeated messages. Host covers64-bit size_t; target covers32. */
    uint32_t chunk=lynx_crc32(data.data(),257),x=chunk;size_t len=257;
    for(unsigned i=0;i<sizeof(size_t)*8-9;i++){
        uint32_t two=lcm_combine(x,x,len);
        assert(lcm_combine(lcm_combine(x,x,len),x,len)==lcm_combine(x,two,2*len));
        assert(lcm_combine(x,0,0)==x);
        x=two;len*=2;++cases;
    }
    printf("PASS_CRCMERGE_NATIVE mode=%u cases=%u known=9b62aaff payload=dc8713ee wordbits=%zu\n",lcm_selector,cases,sizeof(size_t)*8);
}
