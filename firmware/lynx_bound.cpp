/* SPDX-License-Identifier: BSD-3-Clause
 * LWB016: scalar/unrolled version of the frozen LW013 boundary algorithm.
 * No widened eligibility, guest cycle reduction or PCM/event elision.
 */
#include "lynx_bound.h"
#include "lynx_wait.h"
#include "lynx_sprite.h"
#include <cstring>
#include "system.h"
#if R8_LYNX_BOUND
#ifndef LB_MODE
#define LB_MODE 1
#endif
static_assert(LB_MODE==1 || LB_MODE==2,"fixed OFF=1 ON=2");
const volatile uint32_t lb_selector=LB_MODE;
uint32_t lb_mode,lb_devices,lb_accepted,lb_loops,lb_short;
void lb_reset(void){lb_mode=lb_selector==2?2:1;lb_devices=lb_accepted=lb_loops=lb_short=0;}
#if defined(__arm__)
#define LB_HOT(name) __attribute__((section(".time_critical." #name),noinline))
#else
#define LB_HOT(name) __attribute__((noinline))
#endif
// Called ONLY by the AD case after mOpcode=D0, mOperand=FD0A, fetched AD,
// valid RAM PC, and !gSystemCPUSleep were established without intervening calls.
LB_HOT(lynx_bound_cpu) ULONG C65C02::TryFDWaitKnown(ULONG last_service,ULONG frame_begin)
{
 ++lb_short;
 lw_bin &s=lw_bins[lsp_active];++s.probes;
 const ULONG cycle=gSystemCycleCount;
 CSystem &sys=static_cast<CSystem &>(mSystem);
 if(mPC>0xfbfb || !mA ||
    mZ || mN!=(mA&0x80) || (sys.mMemMapReg&2) ||
    mRamPointer[mPC+1]!=0x0a ||
    mRamPointer[mPC+2]!=0xfd || mRamPointer[mPC+3]!=0xd0 ||
    mRamPointer[mPC+4]!=0xfb || gSystemIRQ ||
    gSystemNMI || gSystemHalt || gEndOfFrame || gSingleStepMode ||
    gSingleStepModeSprites || gBreakpointHit) return 0;
 const ULONG since_service=(cycle-last_service)&0x7fffffff;
 const ULONG since_frame=(cycle-frame_begin)&0x7fffffff;
 if(since_service>=8192 || since_frame>=1000000) return 0;
 ULONG budget=8191-since_service;
 if(999999-since_frame<budget)budget=999999-since_frame;
#if LW_ORACLE && !defined(__arm__)
 ULONG n=sys.mMikie->BatchFD0AOracle(budget,mA);
#else
 ULONG n=sys.mMikie->BatchFD0A(budget,mA);
#endif
 if(n){++s.batches;s.loops+=n;s.cycles+=22*n;}
 // CPU already has exactly the final taken-BNE state: A/N/Z/PC/operand/opcode.
 // Return logical short-opcode count to preserve the adaptive retry decision.
 return 2*n;
}

LB_HOT(lynx_bound_device) ULONG CMikie::BatchFD0ABound(ULONG budget,ULONG expected)
{
 ++lb_devices;
 const ULONG cycle=gSystemCycleCount;
 if(cycle>=0xf0000000u || gSystemCPUSleep || gSystemIRQ || gSystemNMI ||
    gSystemHalt || gEndOfFrame || mTimerStatusFlags ||
    !expected || expected!=(UBYTE)mTIM_2_CURRENT ||
    !mTIM_0_ENABLE_COUNT || !mTIM_7_ENABLE_COUNT ||
    !(mTIM_7_ENABLE_RELOAD || !mTIM_7_TIMER_DONE) ||
    (mTIM_1_ENABLE_COUNT|mTIM_3_ENABLE_COUNT|mTIM_4_ENABLE_COUNT|
     mTIM_5_ENABLE_COUNT|mTIM_6_ENABLE_COUNT) ||
    mTIM_0_LINKING>7 || mTIM_7_LINKING>=7 ||
    ((mUART_TX_COUNTDOWN&UART_TX_INACTIVE)&&mUART_TX_IRQ_ENABLE) ||
    (mUART_RX_READY&&mUART_RX_IRQ_ENABLE) || gNextTimerEvent<=cycle) return 0;
 ULONG limit=budget;
 auto bound=[&](ULONG distance){if(!distance)return false;if(distance-1<limit)limit=distance-1;return true;};
 if(!bound(0xf0000000u-cycle) || !bound(gNextTimerEvent-cycle))return 0;
 if(gCPUWakeupTime && (gCPUWakeupTime<=cycle || !bound(gCPUWakeupTime-cycle)))return 0;
 // No raw PCM invocation or resampler mutation inside an accepted batch.
 if(gAudioEnabled) {
  if(gAudioLastUpdateCycle>cycle || cycle-gAudioLastUpdateCycle>=HANDY_AUDIO_SAMPLE_PERIOD)return 0;
  bound(HANDY_AUDIO_SAMPLE_PERIOD-(cycle-gAudioLastUpdateCycle));
 }

 // Snapshot only scalar values. All checks finish before any guest mutation.
 // Explicit fields remove the pointer-array build/indirect commit loop.
 struct Counter {ULONG current,last,divide;};
#define LB_PREP(V,PREFIX) \
 Counter V{PREFIX##_CURRENT,PREFIX##_LAST_COUNT,4+PREFIX##_LINKING}; \
 if(V.current>255 || V.last>cycle || PREFIX##_LINKING>7)return 0; \
 {ULONG span=(V.current+1)<<V.divide,age=cycle-V.last; \
  if(age>=span)return 0;bound(span-age);}
 LB_PREP(t0,mTIM_0)
 LB_PREP(t7,mTIM_7)
#define LB_AUDIO_ACTIVE(N) (gAudioEnabled && mAUDIO_##N##_ENABLE_COUNT && \
 (mAUDIO_##N##_ENABLE_RELOAD || !mAUDIO_##N##_TIMER_DONE) && mAUDIO_##N##_VOLUME && mAUDIO_##N##_BKUP)
 const bool active0=LB_AUDIO_ACTIVE(0),active1=LB_AUDIO_ACTIVE(1),active2=LB_AUDIO_ACTIVE(2),active3=LB_AUDIO_ACTIVE(3);
#define LB_AUDIO_PREP(N) \
 Counter a##N{}; \
 if(active##N){ \
  if(mAUDIO_##N##_LINKING>=7)return 0; \
  LB_PREP(v,mAUDIO_##N) a##N=v; \
 }
 LB_AUDIO_PREP(0) LB_AUDIO_PREP(1) LB_AUDIO_PREP(2) LB_AUDIO_PREP(3)
#undef LB_PREP
#undef LB_AUDIO_ACTIVE
#undef LB_AUDIO_PREP
 ULONG n=limit/22;
 if(n<2)return 0;
 const ULONG final=cycle+22*n-6;
 ULONG next=gCPUWakeupTime?gCPUWakeupTime:0xffffffffu;
#define LB_COMMIT(V,PREFIX) \
 {ULONG dec=(final-V.last)>>V.divide; \
  ULONG prev=(final-22-V.last)>>V.divide; \
  PREFIX##_LAST_COUNT=V.last+(dec<<V.divide);PREFIX##_CURRENT=V.current-dec; \
  PREFIX##_BORROW_IN=dec!=prev;PREFIX##_BORROW_OUT=FALSE; \
  ULONG prediction=final+((V.current-dec+1)<<V.divide);if(prediction<next)next=prediction;}
 LB_COMMIT(t0,mTIM_0) LB_COMMIT(t7,mTIM_7)
 if(active0)LB_COMMIT(a0,mAUDIO_0)
 if(active1)LB_COMMIT(a1,mAUDIO_1)
 if(active2)LB_COMMIT(a2,mAUDIO_2)
 if(active3)LB_COMMIT(a3,mAUDIO_3)
#undef LB_COMMIT
 if(mTIM_2_ENABLE_COUNT){mTIM_2_LAST_LINK_CARRY=FALSE;mTIM_2_BORROW_IN=FALSE;mTIM_2_BORROW_OUT=FALSE;}
 gNextTimerEvent=next;gSystemCycleCount=final+6;
 ++lb_accepted;lb_loops+=n;
 return n;
}
#endif
