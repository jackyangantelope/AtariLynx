uint32_t lynx_crc32(const void *data,size_t n){
    const uint8_t *p=(const uint8_t*)data;uint32_t c=0xffffffffu;
#if R8_LYNX_CRC_TABLE
    if(lcrc_selector==2){
        while(n--)c=(c>>8)^lcrc_table[(c^*p++)&255u];
        return ~c;
    }
#endif
    while(n--){c^=*p++;for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320u&-(c&1u));}
    return ~c;
}
