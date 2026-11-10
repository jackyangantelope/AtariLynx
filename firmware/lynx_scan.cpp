/* SPDX-License-Identifier: BSD-3-Clause
 * Exact eleven-instruction loader block. Every mapped cartridge read still
 * invokes the original core at its original guest cycle, with the original
 * CPU register/stack state visible to the device. No cached cartridge bytes,
 * EEPROM elision, invented checksum, guest time advance or service coalescing.
 */
#include "lynx_scan.h"
#include "lynx_cost.h"
#include <cstring>
#include "system.h"
#if R8_LYNX_SCAN
extern "C" void n3_lynx_service(void);
#if defined(__arm__)
#define LCS_HOT __attribute__((section(".time_critical.lynx_scan_byte"),noinline))
#else
#define LCS_HOT __attribute__((noinline))
#endif
LCS_HOT ULONG C65C02::TryBootScan(ULONG last_service,ULONG frame_begin)
{
#if defined(_LYNXDBG) || defined(TRACE_CPU) || R8_LYNX_CPU_PROBE || !R8_LYNX_HOTSET || R8_LYNX_SHORT_MISS
    return 0;
#else
    if(mPC!=0x0313 || !lcs_active || lcs_data.mode!=2)return 0;
    ++lcs_data.probes;
    const ULONG cycle=gSystemCycleCount;
    CSystem &sys=static_cast<CSystem &>(mSystem);
    static const UBYTE read_code[]={0xad,0xb2,0xfc,0xe6,0x2a,0xd0,0x2f};
    static const UBYTE sum_code[]={0x20,0xbd,0x02,0xaa,0xbd,0x00,0xc0,0x45,0x2c,0x85,0x2c,0x88,0x10,0xf2};
    if(gSystemCPUSleep || gSystemIRQ || gSystemNMI || gSystemHalt ||
       gEndOfFrame || gSingleStepMode || gSingleStepModeSprites ||
       gBreakpointHit || (sys.mMemMapReg&1) || mY<0 || mY>3 ||
       mSP<0 || mSP>255 || mRamPointer[0x2a]==255 ||
       std::memcmp(mRamPointer+0x2bd,read_code,sizeof read_code) ||
       std::memcmp(mRamPointer+0x313,sum_code,sizeof sum_code) ||
       mRamPointer[0x2f3]!=0x60){++lcs_data.guard;return 0;}
    const ULONG age_service=(cycle-last_service)&0x7fffffff;
    const ULONG age_frame=(cycle-frame_begin)&0x7fffffff;
    if(age_service>=8192 || age_frame>=1000000 || cycle>=0xf0000000u ||
       gNextTimerEvent<=cycle){++lcs_data.boundary;return 0;}
    ULONG budget=8191-age_service;
    if(999999-age_frame<budget)budget=999999-age_frame;
    if(gNextTimerEvent-cycle-1<budget)budget=gNextTimerEvent-cycle-1;
    if(0xf0000000u-cycle-1<budget)budget=0xf0000000u-cycle-1;
    if(gCPUWakeupTime){
        if(gCPUWakeupTime<=cycle){++lcs_data.boundary;return 0;}
        if(gCPUWakeupTime-cycle-1<budget)budget=gCPUWakeupTime-cycle-1;
    }
    // Complete only noncarry byte iterations, each exactly151 original cycles.
    // Original dispatch handles the low-byte carry and all threshold edges.
    ULONG n=budget/151;
    if(n>(ULONG)mY+1)n=mY+1;
    if(n>255u-mRamPointer[0x2a])n=255u-mRamPointer[0x2a];
    if(!n){++lcs_data.boundary;return 0;}
    for(ULONG i=0;i<n;++i){
        // JSR0313->02BD (26 cycles), then LDA FCB2 (16 cycles).
        const int sp=mSP;
        mRamPointer[0x100+mSP]=0x03;mSP=(mSP-1)&255;
        mRamPointer[0x100+mSP]=0x15;mSP=(mSP-1)&255;
        mOpcode=0xad;mOperand=0xfcb2;mPC=0x02c0;
        gSystemCycleCount+=42;
        mA=mSystem.Peek_CPU(0xfcb2);
        // INC2A, taken BNE02F3, RTS, TAX, LDA C000,X, EOR2C, STA2C,
        // DEY and BPL. No observable service/device boundary lies inside.
        ++mRamPointer[0x2a];mSP=sp;mX=mA;
        mA=mRamPointer[0xc000+mX]^mRamPointer[0x2c];
        mRamPointer[0x2c]=(UBYTE)mA;
        mY=(mY-1)&255;mN=mY&0x80;mZ=(mY==0);
        mOpcode=0x10;mOperand=0x2c;mPC=mN?0x0321:0x0313;
        gSystemCycleCount+=109;
    }
    ++lcs_data.batches;lcs_data.bytes+=n;lcs_data.cycles+=151*n;
    // EOR45 and DEY88 are the two original short-dispatch misses.
    return 9*n;
#endif
}

/* A separate startup frame body is generated from the frozen031 short frame.
 * Gameplay keeps its original function and instruction placement. */
#include "lynx_scan_frame.inc"
#endif
