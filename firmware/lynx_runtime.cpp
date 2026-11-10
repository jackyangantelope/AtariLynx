#include "lynx_cost.h"
#include "lynx_load_diag.h"
#include "lynx_boot_pcm.h"
#include "lynx_crc.h"
#include "lynx_crcmerge.h"
#include "lynx_wide.h"
/* SPDX-License-Identifier: BSD-3-Clause */
#include "retro8.h"
#include "input_map.h"
#include "lynx_diag.h"
#include "lynx_fd.h"
#include "lynx_encode.h"
#include "lynx_run.h"
#include "lynx_bound.h"
#include "lynx_hash.h"
#include "lynx_render.h"
#include "lynx_short.h"
#include "lynx_sprite.h"
#include <cstdlib>
#include <cstring>
#include "system.h"
static CSystem *machine;
static uint8_t *arena;
static size_t capacity,used;
static uint16_t *pixels;
static r8_audio_fn output;
static r8_service_fn service;
static const char *error="";
static uint32_t elapsed=266667,phase;
static int16_t previous_l,previous_r;
static uint8_t saved[2048];static size_t saved_size;
static lynx_diag_stats diag;
static bool video_requested=true;
/* Two 256-entry SRAM tables produce the exact doubled RGB555 pixel. */
static uint32_t rgb_low[256],rgb_high[256];
#if defined(__arm__)
#define LYNX_HOT(name) __attribute__((section(".time_critical." #name),noinline))
#else
#define LYNX_HOT(name)
#endif
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
void lynx_diag_get(lynx_diag_stats *out){
    *out=diag;
    if(machine){out->rotation=machine->mMikie->GetRotation();out->pending_rotation=machine->mMikie->GetPendingRotation();}
}
void lynx_diag_interval_reset(void){diag.raw={};diag.converted={};}
void lynx_diag_request_video(void){video_requested=true;}
int lynx_diag_wants_video(void){return video_requested;}
void r8_restore_save(const uint8_t *data,size_t n){
    saved_size=(data&&n<=sizeof saved)?n:0;
    if(saved_size)memcpy(saved,data,n);
}
size_t r8_export_save(uint8_t *data,size_t n){
    if(!machine||!machine->mEEPROM->Available())return 0;
    size_t bytes=machine->mEEPROM->Size();
    if(n<bytes)return 0;
    machine->mEEPROM->CopyTo(data);return bytes;
}
extern "C" void *n3_cart_alloc(size_t n){
    n=(n+7)&~size_t(7);if(n>capacity-used)abort();void *p=arena+used;used+=n;return p;
}
extern "C" void n3_cart_free(void *p){(void)p;}
extern "C" void n3_lynx_service(void){if(service)service();}
extern "C" LYNX_HOT(n3_lynx_pcm) void n3_lynx_pcm(int16_t l,int16_t r){
#if R8_LYNX_FD_ORACLE
    if(lfd_oracle_pcm(l,r,gSystemCycleCount,gAudioLastUpdateCycle))return;
#endif
#if LW_ORACLE && !defined(__arm__)
    extern unsigned lw_oracle_active;
    if(lw_oracle_active)abort();
#endif
    /* Count all pairs, but remove full-rate min/max/change instrumentation. */
    diag.raw.count++;
    /* 32 kHz -> 44.1 kHz linear interpolation, exact rational sample count. */
    phase+=44100;
    while(phase>=32000){phase-=32000;
        /* Keep both weights signed: unsigned promotion corrupts negative PCM.
           Weights sum to 44100, so every int16 input fits an int32 numerator. */
        int32_t weight=(int32_t)phase;
        int32_t a=((int32_t)previous_l*weight+(int32_t)l*(44100-weight))/44100;
        int32_t b=((int32_t)previous_r*weight+(int32_t)r*(44100-weight))/44100;
        diag.converted.count++;
        output((int16_t)a,(int16_t)b);
    }
    previous_l=l;previous_r=r;
}
#if R8_LYNX_BOOT_PCM
extern "C" bool n3_lynx_pcm_batch(int16_t l,int16_t r,uint32_t raw){
    static_assert(HANDY_AUDIO_SAMPLE_PERIOD==500,"LBP029 input rate is 32k at 16MHz");
    if(!lbp_active || lbp_mode!=2 || raw<LBP_MIN_RAW || raw>LBP_MAX_RAW ||
       !lbp_ready || !lbp_emit || !lbp_ready())return false;
    /* The first input may interpolate against a different previous pair.
     * Keep its original signed rounding and one/two output callbacks. */
    n3_lynx_pcm(l,r);gAudioLastUpdateCycle+=HANDY_AUDIO_SAMPLE_PERIOD;
    uint32_t remaining=raw-1;
    uint64_t scaled=(uint64_t)phase+(uint64_t)remaining*44100u;
    uint32_t converted=(uint32_t)(scaled/32000u);
    phase=(uint32_t)(scaled%32000u);
    /* All following interpolations have equal endpoints, hence exact l/r.
     * No event/service occurs inside the original constant-sample loop. */
    diag.raw.count+=remaining;diag.converted.count+=converted;
    lbp_emit(l,r,converted);
    gAudioLastUpdateCycle+=(uint32_t)((uint64_t)remaining*HANDY_AUDIO_SAMPLE_PERIOD);
    return true;
}
#endif
bool r8_init(void *bulk,size_t n,r8_audio_fn a,r8_service_fn s){
    if(n<2*1024*1024)return false;pixels=(uint16_t*)bulk;arena=(uint8_t*)bulk+65536;capacity=n-65536;output=a;service=s;
    for(unsigned i=0;i<256;i++){
        unsigned lo=((i>>6)&31)<<5|((i&31)<<10);
        unsigned v=i<<8,hi=(v>>11)|(((v>>6)&31)<<5);
        rgb_low[i]=lo|(lo<<16);rgb_high[i]=hi|(hi<<16);
    }
    return true;
}
static unsigned le16(const uint8_t *p){return p[0]|(p[1]<<8);}
bool r8_load(const uint8_t *game,size_t n,const uint8_t *bios,size_t bn,const char *name){
#if R8_LYNX_BOOT_CPU
    lbc_reset();
#if R8_LYNX_SCAN
    lcs_reset();
#if R8_LYNX_WIDE
    lwd_reset();
#endif
#endif
#endif
#if R8_LYNX_BOOT_PCM
    lbp_reset();
#endif
    lld_phase(LLD_VALIDATE);
    (void)bios;(void)bn;(void)name;error="";
    if(!game||n<64||memcmp(game,"LYNX",4)){error="Need headered .lnx cartridge";return false;}
    unsigned b0=le16(game+4),b1=le16(game+6);
    if((b0!=0x100&&b0!=0x200&&b0!=0x400&&b0!=0x800)||
       (b1!=0&&b1!=0x100&&b1!=0x200&&b1!=0x400&&b1!=0x800)||game[59]||game[58]){
        error="Unsupported LNX bank/rotation";return false;
    }
    size_t expected=64+(b0+b1)*256;
    if(n!=expected){error="Truncated/oversized LNX";return false;}
    if(game[64]<251){error="Invalid LNX boot block count";return false;}
#if R8_LYNX_SHORT
    ls_active=0;
#endif
#if R8_LYNX_SPRITE
    lsp_ready=0;
#if R8_LYNX_COST
    lmc_ready=0;
#endif
#endif
    diag={};video_requested=true;
#if R8_LYNX_CRC_MERGE
    if(lcm_selector==2){
        /* The validated input is immutable here. Scan every payload byte once;
         * combine its finalized CRC with the separately read64-byte header.
         * The independent post-construction bank scan below remains intact. */
        lld_phase(LLD_PAYLOAD_CRC);
        diag.payload_crc=lynx_crc32(game+64,n-64);
        lld_phase(LLD_ROM_CRC);
        diag.rom_crc=lcm_combine(lynx_crc32(game,64),diag.payload_crc,n-64);
    }else
#endif
    {
        lld_phase(LLD_ROM_CRC);
        diag.rom_crc=lynx_crc32(game,n);
        lld_phase(LLD_PAYLOAD_CRC);
        diag.payload_crc=lynx_crc32(game+64,n-64);
    }
    lld_phase(LLD_SAVE_CRC);
    diag.save_bytes=(uint32_t)saved_size;diag.save_crc=lynx_crc32(saved,saved_size);
    lld_phase(LLD_CLEAR);
    if(machine){delete machine;machine=nullptr;}used=0;phase=0;previous_l=previous_r=0;elapsed=266667;
    memset(pixels,0,65536);gPrimaryFrameBuffer=(UBYTE*)pixels;
    lld_phase(LLD_CONSTRUCT);
    machine=new CSystem(game,n,MIKIE_PIXEL_FORMAT_16BPP_565,32000);
    machine->mMikie->SetRotation(MIKIE_NO_ROTATE);
    lld_phase(LLD_BANK_CRC);
    diag.bank_crc=lynx_crc32(arena,(size_t)b0*256);
    lld_phase(LLD_APPLY);
    if(machine->mEEPROM->Available()&&saved_size==(size_t)machine->mEEPROM->Size()){
        machine->mEEPROM->InitFrom((char*)saved,saved_size);
        diag.save_applied=1;
    }
    gAudioEnabled=TRUE;
    lld_phase(LLD_BOOT);
    lld_boot_begin(gSystemCycleCount);
#if R8_LYNX_BOOT_PCM
    lbp_active=1;
#if R8_LYNX_SCAN
    lcs_active=1;
#endif
#endif
    /* Cartridge boot precedes video timer setup. Allow up to four emulated seconds. */
#if R8_LYNX_BOOT_CPU
    /* Startup evaluates scalar short instructions only. Explicitly suppress
       FD0A wait batching on both cold start and reentry; restore before the
       existing post-boot reset (also on failure). Guest scheduling is intact. */
    const uint32_t prior_wait_mode=lw_mode;lw_mode=1;
    for(int i=0;i<64&&!gEndOfFrame&&!gSystemHalt;i++){
        lld_boot_call();uint32_t start=lld_clock(),cycles=gSystemCycleCount;
        uint32_t hits=0;
        if(lbc_data.mode==2){
#if R8_LYNX_SCAN
#if R8_LYNX_WIDE
            if(lcs_data.mode==2 && lwd_data.mode==2)hits=machine->UpdateWideFrame(true);else
#endif
            if(lcs_data.mode==2)hits=machine->UpdateScanFrame(true);else
#endif
            hits=machine->UpdateShortFrame(true);
        }
        else machine->UpdateFrame(true);
        lbc_call_end(start,gSystemCycleCount-cycles,hits);
    }
    lw_mode=prior_wait_mode;
#else
    for(int i=0;i<64&&!gEndOfFrame&&!gSystemHalt;i++){lld_boot_call();machine->UpdateFrame(true);}
#endif
#if R8_LYNX_BOOT_PCM
    lbp_active=0;
#if R8_LYNX_SCAN
    lcs_active=0;
#endif
    lbp_data.raw_total=diag.raw.count;lbp_data.converted_total=diag.converted.count;
    lbp_data.phase=phase;lbp_data.previous=(uint16_t)previous_l|((uint32_t)(uint16_t)previous_r<<16);
#endif
    lld_boot_end(gSystemCycleCount,gEndOfFrame,gSystemHalt);
    lld_phase(LLD_RESET);
    if(!gEndOfFrame){error="Lynx boot did not start video";return false;}
#if R8_LYNX_SHORT
    ls_reset();
#if R8_LYNX_WAIT
    lw_reset();
#if R8_LYNX_ENCODE
    le_reset();
#if R8_LYNX_RUN
    lrn_reset();
#if R8_LYNX_BOUND
    lb_reset();
#if R8_LYNX_HASH_SWITCH
    lh_reset();
#if R8_LYNX_RENDER_PAIR
    lr_reset();
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#if R8_LYNX_SPRITE
    lsp_reset();
#if R8_LYNX_COST
    lmc_reset();
#endif
#endif
    return true;
}
void r8_controls(const r8_input *s){
    unsigned p=n3_pad(s),v=0;
    if(p&R8_A)v|=BUTTON_A;if(p&R8_B)v|=BUTTON_B;
    if(p&R8_UP)v|=BUTTON_UP;if(p&R8_DOWN)v|=BUTTON_DOWN;
    if(p&R8_LEFT)v|=BUTTON_LEFT;if(p&R8_RIGHT)v|=BUTTON_RIGHT;
    if(p&R8_START)v|=BUTTON_PAUSE;if(p&R8_SELECT)v|=BUTTON_OPT1;
    if(p&R8_X)v|=BUTTON_OPT1;if(p&R8_Y)v|=BUTTON_OPT2;
    machine->SetButtonData(v);
    diag.buttons=(uint16_t)v;
}
void r8_frame(void){
#if R8_LYNX_SPRITE
    lsp_frame_begin();
#endif
#if R8_LYNX_SHORT
    ls_frame_begin();
#endif
    uint32_t start=gSystemCycleCount;
#if R8_LYNX_SHORT
    if(ls_should_probe())ls_probe_done(machine->UpdateShortFrame(true));
    else machine->UpdateFrame(true);
#else
    machine->UpdateFrame(true);
#endif
    elapsed=(gSystemCycleCount-start)&0x7fffffff;
    if(!elapsed)elapsed=266667;
    if(!gEndOfFrame)error="Lynx VBL timeout";
    diag.frame++;
#if R8_LYNX_WAIT
    lw_frame_done((uint16_t)machine->GetButtonData(),pixels);
#endif
#if R8_LYNX_SHORT
    ls_frame_done(elapsed);
#endif
#if R8_LYNX_SPRITE
    lsp_frame_done(elapsed);
#if R8_LYNX_COST
    lmc_frame_done(elapsed);
#endif
#endif
}
LYNX_HOT(r8_render) void r8_render(uint16_t *dst){
    /* Every active pixel is overwritten; only the 18-line borders need clear.
       memcpy of 4 bytes is alias/alignment-safe and compiles to word stores. */
    memset(dst,0,320*18*2);memset(dst+320*222,0,320*18*2);
#if R8_LYNX_RENDER_PAIR
    ++lr_calls;
    if(lr_mode==2){
        // Two independent 16-bit lanes. Same RGB565 -> RGB555 bits as the
        // legacy tables; memcpy preserves C++ alignment/aliasing semantics.
        for(unsigned y=0;y<102;y++){
            const uint16_t *src=pixels+y*160;uint16_t *row=dst+(y*2+18)*320;
            for(unsigned x=0;x<160;x+=2){
                uint32_t v;memcpy(&v,src+x,sizeof(v));
                uint32_t c=((v&0xf800f800u)>>11)|((v&0x07c007c0u)>>1)|((v&0x001f001fu)<<10);
                uint32_t lo=(c&0xffffu)*0x00010001u,hi=(c>>16)*0x00010001u;
                memcpy(row+x*2,&lo,4);memcpy(row+x*2+2,&hi,4);
                memcpy(row+320+x*2,&lo,4);memcpy(row+320+x*2+2,&hi,4);
            }
        }
    }else
#endif
    {
    for(unsigned y=0;y<102;y++){
        const uint16_t *src=pixels+y*160;uint16_t *row=dst+(y*2+18)*320;
        for(unsigned x=0;x<160;x++){
            unsigned v=src[x];uint32_t pair=rgb_low[v&255]|rgb_high[v>>8];
            memcpy(row+x*2,&pair,4);memcpy(row+320+x*2,&pair,4);
        }
    }
    }
    /* Sparse, same-frame readback. Hash each uint16 value, including borders.
       This verifies content in memory; it is not physical HDMI acceptance. */
    if(video_requested){
        video_requested=false;diag.video_frame=diag.frame;diag.video_samples++;
#if R8_LYNX_HASH_SWITCH
        // Keep request/sample bookkeeping. Zero hash/nz fields mean unavailable
        // in fixed ON mode, explicitly declared by LYNX_HASH_CFG/LYNX_HASH.
        if(lh_mode==2){++lh_video_skipped;}
        else
#endif
        {
        diag.raw_hash=diag.rgb_hash=2166136261u;diag.raw_nonzero=diag.rgb_nonzero=0;
        for(unsigned i=0;i<160*102;i++){uint16_t v=pixels[i];diag.raw_hash=(diag.raw_hash^v)*16777619u;diag.raw_nonzero+=(v!=0);}
        for(unsigned i=0;i<320*240;i++){uint16_t v=dst[i];diag.rgb_hash=(diag.rgb_hash^v)*16777619u;diag.rgb_nonzero+=(v!=0);}
        }
    }
}
const char *r8_error(void){return error;}
uint32_t r8_clock(void){return 16000000;}
uint32_t r8_frame_cycles(void){return elapsed;}
uint16_t r8_pc(void){if(!machine)return 0;C6502_REGS r;machine->mCpu->GetRegs(r);return r.PC;}
uint8_t r8_peek(uint16_t a){return machine?machine->GetRamPointer()[a]:0;}

#if defined(LYNX_SHORT_HOST_STATE) && !defined(__arm__)
/* Test-only canonical state and framebuffer checkpoints, never linked on RP2350. */
extern "C" int lynx_test_state(const char *path){
    FILE *f=fopen(path,"wb");if(!f)return 0;
    bool ok=machine->ContextSave(f);
    C6502_REGS regs={};machine->mCpu->GetRegs(regs);
    ok=ok&&fwrite(&regs,sizeof regs,1,f)==1&&fwrite(pixels,32640,1,f)==1;
    return fclose(f)==0&&ok;
}
#endif
