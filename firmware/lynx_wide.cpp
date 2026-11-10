/* SPDX-License-Identifier: BSD-3-Clause
 * LWD034: join the original four-byte blocks across their noncarry group
 * control. Preserve every RCART0/JOYSTICK read and original read-time CPU,
 * stack, device state, guest cycles and logical short/scan counters.
 */
#include "lynx_wide.h"
#include "lynx_scan.h"
#include "lynx_cost.h"
#include <cstring>
#include "system.h"
#if R8_LYNX_WIDE
extern "C" void n3_lynx_service(void);
static inline uint32_t lwd_word(const UBYTE *p){uint32_t v;std::memcpy(&v,p,4);return v;}
static inline bool lwd_signature(const UBYTE *p){
    // All word addresses are aligned offsets within the original RAM. memcpy
    // avoids aliasing/type-punning; masked preceding bytes are not assumed.
    return (lwd_word(p+0x2bc)&0xffffff00u)==0xfcb2ad00u &&
        lwd_word(p+0x2c0)==0x2fd02ae6u && p[0x2f3]==0x60 &&
        (lwd_word(p+0x310)&0xffffff00u)==0x2003a000u &&
        lwd_word(p+0x314)==0xbdaa02bdu && lwd_word(p+0x318)==0x2c45c000u &&
        lwd_word(p+0x31c)==0x10882c85u && lwd_word(p+0x320)==0xfcb0adf2u &&
        lwd_word(p+0x324)==0x572003f0u && lwd_word(p+0x328)==0xd021e603u &&
        lwd_word(p+0x32c)==0xd022e6e4u && p[0x330]==0xe0 && p[0x331]==0x60;
}
#if defined(__arm__)
#define LWD_HOT __attribute__((section(".time_critical.lynx_wide"),noinline))
#else
#define LWD_HOT __attribute__((noinline))
#endif
LWD_HOT ULONG C65C02::TryBootWide(ULONG last_service,ULONG frame_begin)
{
#if defined(_LYNXDBG) || defined(TRACE_CPU) || R8_LYNX_CPU_PROBE || !R8_LYNX_HOTSET || R8_LYNX_SHORT_MISS
    return 0;
#else
    if(mPC!=0x313 || !lcs_active || lcs_data.mode!=2 || lwd_data.mode!=2)return 0;
    ++lwd_data.entries;
    CSystem &sys=static_cast<CSystem &>(mSystem);
    if(gSystemCPUSleep || gSystemIRQ || gSystemNMI || gSystemHalt ||
       gEndOfFrame || gSingleStepMode || gSingleStepModeSprites || gBreakpointHit ||
       (sys.mMemMapReg&1) || mY<0 || mY>3 || mSP<0 || mSP>255 ||
       mRamPointer[0x2a]==255 || !lwd_signature(mRamPointer))return 0;
    const ULONG cycle=gSystemCycleCount;
    const ULONG service_age=(cycle-last_service)&0x7fffffff;
    const ULONG frame_age=(cycle-frame_begin)&0x7fffffff;
    if(service_age>=8192 || frame_age>=1000000 || cycle>=0xf0000000u || gNextTimerEvent<=cycle)return 0;
    ULONG budget=8191-service_age;
    if(999999-frame_age<budget)budget=999999-frame_age;
    if(gNextTimerEvent-cycle-1<budget)budget=gNextTimerEvent-cycle-1;
    if(0xf0000000u-cycle-1<budget)budget=0xf0000000u-cycle-1;
    if(gCPUWakeupTime){
        if(gCPUWakeupTime<=cycle)return 0;
        if(gCPUWakeupTime-cycle-1<budget)budget=gCPUWakeupTime-cycle-1;
    }
    // The guarded FCxx mapping and owned devices cannot change inside this
    // deadline-bounded loop. Call the same original Susie body directly;
    // RCART0 still selects the original bank and updates EEPROM every byte.
    CSusie *const susie=sys.mSusie;
    UBYTE *const ram=mRamPointer;
    const int sp=mSP;
    ULONG hits=0,bytes=0,groups=0;
    for(;;){
        ULONG n=budget/151;
        if(n>(ULONG)mY+1)n=mY+1;
        if(n>255u-ram[0x2a])n=255u-ram[0x2a];
        if(!n)break;
        // The same chunk the original LCS032 would accept at this PC. No
        // signature can change inside: writes touch stack/zero page only,
        // device reads do not write guest RAM, and no deadline is crossed.
        ++lcs_data.probes;++lcs_data.batches;
        lcs_data.bytes+=n;lcs_data.cycles+=151*n;
        bytes+=n;hits+=9*n;budget-=151*n;
        for(ULONG i=0;i<n;++i){
            ram[0x100+sp]=0x03;mSP=(sp-1)&255;
            ram[0x100+mSP]=0x15;mSP=(sp-2)&255;
            mOpcode=0xad;mOperand=0xfcb2;mPC=0x2c0;
            gSystemCycleCount+=42;mA=susie->CSusie::Peek(0xfcb2);
            ++ram[0x2a];mSP=sp;mX=mA;
            mA=ram[0xc000+mX]^ram[0x2c];ram[0x2c]=(UBYTE)mA;
            mY=(mY-1)&255;mN=mY&0x80;mZ=(mY==0);
            mOpcode=0x10;mOperand=0x2c;mPC=mN?0x321:0x313;
            gSystemCycleCount+=109;
        }
        // Original dispatch handles incomplete byte groups, counter carries,
        // and any boundary too close to finish the complete control sequence.
        if(mPC!=0x321 || budget<55 || ram[0x21]==255)break;
        mOpcode=0xad;mOperand=0xfcb0;mPC=0x324;
        gSystemCycleCount+=16;mA=susie->CSusie::Peek(0xfcb0);mN=mA&0x80;mZ=(mA==0);
        ++hits;budget-=16;
        // Nonzero input keeps the original BEQ/JSR and Mikie side effects.
        if(mA)break;
        // BEQ + INC21 + BNE + LDY#3. All four are original short hits.
        gSystemCycleCount+=39;budget-=39;
        ++ram[0x21];mY=3;mN=0;mZ=0;
        mOpcode=0xa0;mOperand=0x312;mPC=0x313;hits+=4;++groups;
    }
    if(bytes){
        ++lwd_data.accepted;lwd_data.bytes+=bytes;lwd_data.groups+=groups;
        if(bytes>lwd_data.max_bytes)lwd_data.max_bytes=bytes;
        lwd_data.cycles+=gSystemCycleCount-cycle;
    }
    return hits;
#endif
}
#include "lynx_wide_frame.inc"
#endif
