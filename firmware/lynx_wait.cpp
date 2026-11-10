/* SPDX-License-Identifier: BSD-3-Clause
 * RP2350 alteration: exact event-free LDA FD0A / taken BNE iteration batching.
 * Device deadlines are derived from counter state, not a cached read value.
 * No state writes before all guards; every boundary uses original execution.
 */
#include "lynx_wait.h"
#include "lynx_hash.h"
#include "lynx_bound.h"
#include "lynx_sprite.h"
#include <cstring>
#include "system.h"
#if R8_LYNX_WAIT
#ifndef LW_MODE
#define LW_MODE 1
#endif
static_assert(LW_MODE==1 || LW_MODE==2,"fixed OFF=1, ON=2");
const volatile uint32_t lw_selector=LW_MODE;
uint32_t lw_mode;
lw_bin lw_bins[2];
uint32_t lw_frames,lw_input_hash,lw_sample_frame,lw_raw_hash;
void lw_reset(void){memset(lw_bins,0,sizeof lw_bins);lw_mode=lw_selector==2?2:1;lw_frames=lw_sample_frame=lw_raw_hash=0;lw_input_hash=2166136261u;}
void lw_frame_done(uint16_t buttons,const uint16_t *pixels){
 ++lw_frames;lw_input_hash=(lw_input_hash^buttons)*16777619u;
 if(lw_frames==1 || (lw_frames&63)==0){
  lw_sample_frame=lw_frames;
#if R8_LYNX_HASH_SWITCH
  if(lh_mode==2){++lh_raw_skipped;}
  else
#endif
  {
  lw_raw_hash=2166136261u;
  for(unsigned i=0;i<160*102;++i)lw_raw_hash=(lw_raw_hash^pixels[i])*16777619u;
  }
 }
}
#if defined(__arm__)
#define LW_HOT(name) __attribute__((section(".time_critical." #name),noinline))
#else
#define LW_HOT(name) __attribute__((noinline))
#endif

LW_HOT(lynx_wait_cpu) ULONG C65C02::TryFDWait(ULONG last_service,ULONG frame_begin)
{
 lw_bin &s=lw_bins[lsp_active];++s.probes;
 const ULONG cycle=gSystemCycleCount;
 CSystem &sys=static_cast<CSystem &>(mSystem);
 if(mPC>0xfbfb || mOpcode!=0xd0 || mOperand!=0xfd0a || !mA ||
    mZ || mN!=(mA&0x80) || (sys.mMemMapReg&2) ||
    mRamPointer[mPC]!=0xad || mRamPointer[mPC+1]!=0x0a ||
    mRamPointer[mPC+2]!=0xfd || mRamPointer[mPC+3]!=0xd0 ||
    mRamPointer[mPC+4]!=0xfb || gSystemCPUSleep || gSystemIRQ ||
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

 return 2*n;
}

LW_HOT(lynx_wait_device) ULONG CMikie::BatchFD0A(ULONG budget,ULONG expected)
{
#if R8_LYNX_BOUND
 if(lb_mode==2)return BatchFD0ABound(budget,expected);
#endif
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

 if(gAudioEnabled) {
  if(gAudioLastUpdateCycle>cycle || cycle-gAudioLastUpdateCycle>=HANDY_AUDIO_SAMPLE_PERIOD)return 0;
  bound(HANDY_AUDIO_SAMPLE_PERIOD-(cycle-gAudioLastUpdateCycle));
 }
 struct Counter {ULONG *current,*last,*in,*out;ULONG divide;};
 Counter t[6];unsigned count=0;
 auto add=[&](ULONG &current,ULONG &last,ULONG &in,ULONG &out,ULONG link){
  if(current>255 || last>cycle || link>7)return false;
  ULONG divide=4+link,span=(current+1)<<divide,age=cycle-last;
  if(age>=span)return false;
  bound(span-age);t[count++]={&current,&last,&in,&out,divide};return true;
 };
 if(!add(mTIM_0_CURRENT,mTIM_0_LAST_COUNT,mTIM_0_BORROW_IN,mTIM_0_BORROW_OUT,mTIM_0_LINKING) ||
    !add(mTIM_7_CURRENT,mTIM_7_LAST_COUNT,mTIM_7_BORROW_IN,mTIM_7_BORROW_OUT,mTIM_7_LINKING))return 0;

#define LW_AUDIO_BOUND(N) \
 if(gAudioEnabled && mAUDIO_##N##_ENABLE_COUNT && \
    (mAUDIO_##N##_ENABLE_RELOAD || !mAUDIO_##N##_TIMER_DONE) && \
    mAUDIO_##N##_VOLUME && mAUDIO_##N##_BKUP) { \
  if(mAUDIO_##N##_LINKING>=7 || !add(mAUDIO_##N##_CURRENT,mAUDIO_##N##_LAST_COUNT, \
     mAUDIO_##N##_BORROW_IN,mAUDIO_##N##_BORROW_OUT,mAUDIO_##N##_LINKING))return 0; \
 }
 LW_AUDIO_BOUND(0) LW_AUDIO_BOUND(1) LW_AUDIO_BOUND(2) LW_AUDIO_BOUND(3)
#undef LW_AUDIO_BOUND
 ULONG n=limit/22;
 if(n<2)return 0;
 const ULONG final=cycle+22*n-6;
 ULONG next=gCPUWakeupTime?gCPUWakeupTime:0xffffffffu;
 for(unsigned i=0;i<count;++i){
  Counter &v=t[i];ULONG old=*v.last;
  ULONG dec=(final-old)>>v.divide;
  ULONG prev=(final-22-old)>>v.divide;
  *v.last=old+(dec<<v.divide);*v.current-=dec;
  *v.in=dec!=prev;*v.out=FALSE;
  ULONG prediction=final+((*v.current+1)<<v.divide);
  if(prediction<next)next=prediction;
 }
 if(mTIM_2_ENABLE_COUNT){mTIM_2_LAST_LINK_CARRY=FALSE;mTIM_2_BORROW_IN=FALSE;mTIM_2_BORROW_OUT=FALSE;}
 gNextTimerEvent=next;
 gSystemCycleCount=final+6;
 return n;
}
#endif

#if R8_LYNX_WAIT && LW_ORACLE && !defined(__arm__)
#include <cstdio>
#include <cstdlib>
unsigned lw_oracle_active;
static void wait_fail(const char *what){fprintf(stderr,"WAIT_ORACLE_MISMATCH %s cycle=%u\n",what,gSystemCycleCount);abort();}
ULONG CMikie::BatchFD0AOracle(ULONG budget,ULONG expected) {
 CMikie candidate(*this);
 ULONG *globals[]={&gSystemCycleCount,&gNextTimerEvent,&gCPUWakeupTime,&gIRQEntryCycle,&gCPUBootAddress,&gBreakpointHit,&gSingleStepMode,&gSingleStepModeSprites,&gSystemIRQ,&gSystemNMI,&gSystemCPUSleep,&gSystemCPUSleep_Saved,&gSystemHalt,&gThrottleMaxPercentage,&gThrottleLastTimerCount,&gThrottleNextCycleCheckpoint,&gEndOfFrame,&gTimerCount,&gRenderFrame,&gAudioEnabled,&gAudioBufferPointer,&gAudioLastUpdateCycle};

 ULONG before[22],after[22];
 for(unsigned i=0;i<22;++i)before[i]=*globals[i];
 lw_oracle_active=1;
 ULONG n=candidate.BatchFD0A(budget,expected);
 for(unsigned i=0;i<22;++i){after[i]=*globals[i];*globals[i]=before[i];}
 for(ULONG i=0;i<n;++i){
  if(gSystemCycleCount>=gNextTimerEvent)wait_fail("scheduled event before LDA");
  gSystemCycleCount+=16;Update();
  if((UBYTE)mTIM_2_CURRENT!=expected || gSystemIRQ || gSystemCPUSleep || gEndOfFrame)wait_fail("read or event changed");
  if(gSystemCycleCount>=gNextTimerEvent)wait_fail("scheduled event before BNE");
  gSystemCycleCount+=6;
 }
 lw_oracle_active=0;
 for(unsigned i=0;i<22;++i)if(*globals[i]!=after[i])wait_fail("global");
 if(mDisplayAddress!=candidate.mDisplayAddress)wait_fail("mDisplayAddress");
 if(mAudioInputComparator!=candidate.mAudioInputComparator)wait_fail("mAudioInputComparator");
 if(mTimerStatusFlags!=candidate.mTimerStatusFlags)wait_fail("mTimerStatusFlags");
 if(mTimerInterruptMask!=candidate.mTimerInterruptMask)wait_fail("mTimerInterruptMask");
 if(mDisplayRotate!=candidate.mDisplayRotate)wait_fail("mDisplayRotate");
 if(mDisplayRotate_Pending!=candidate.mDisplayRotate_Pending)wait_fail("mDisplayRotate_Pending");
 if(memcmp(mPalette,candidate.mPalette,sizeof mPalette))wait_fail("mPalette");
 if(memcmp(mColourMap,candidate.mColourMap,sizeof mColourMap))wait_fail("mColourMap");
 if(mIODAT!=candidate.mIODAT)wait_fail("mIODAT");
 if(mIODIR!=candidate.mIODIR)wait_fail("mIODIR");
 if(mIODAT_REST_SIGNAL!=candidate.mIODAT_REST_SIGNAL)wait_fail("mIODAT_REST_SIGNAL");
 if(mDISPCTL_DMAEnable!=candidate.mDISPCTL_DMAEnable)wait_fail("mDISPCTL_DMAEnable");
 if(mDISPCTL_Flip!=candidate.mDISPCTL_Flip)wait_fail("mDISPCTL_Flip");
 if(mDISPCTL_FourColour!=candidate.mDISPCTL_FourColour)wait_fail("mDISPCTL_FourColour");
 if(mDISPCTL_Colour!=candidate.mDISPCTL_Colour)wait_fail("mDISPCTL_Colour");
 if(mTIM_0_BKUP!=candidate.mTIM_0_BKUP)wait_fail("mTIM_0_BKUP");
 if(mTIM_0_ENABLE_RELOAD!=candidate.mTIM_0_ENABLE_RELOAD)wait_fail("mTIM_0_ENABLE_RELOAD");
 if(mTIM_0_ENABLE_COUNT!=candidate.mTIM_0_ENABLE_COUNT)wait_fail("mTIM_0_ENABLE_COUNT");
 if(mTIM_0_LINKING!=candidate.mTIM_0_LINKING)wait_fail("mTIM_0_LINKING");
 if(mTIM_0_CURRENT!=candidate.mTIM_0_CURRENT)wait_fail("mTIM_0_CURRENT");
 if(mTIM_0_TIMER_DONE!=candidate.mTIM_0_TIMER_DONE)wait_fail("mTIM_0_TIMER_DONE");
 if(mTIM_0_LAST_CLOCK!=candidate.mTIM_0_LAST_CLOCK)wait_fail("mTIM_0_LAST_CLOCK");
 if(mTIM_0_BORROW_IN!=candidate.mTIM_0_BORROW_IN)wait_fail("mTIM_0_BORROW_IN");
 if(mTIM_0_BORROW_OUT!=candidate.mTIM_0_BORROW_OUT)wait_fail("mTIM_0_BORROW_OUT");
 if(mTIM_0_LAST_LINK_CARRY!=candidate.mTIM_0_LAST_LINK_CARRY)wait_fail("mTIM_0_LAST_LINK_CARRY");
 if(mTIM_0_LAST_COUNT!=candidate.mTIM_0_LAST_COUNT)wait_fail("mTIM_0_LAST_COUNT");
 if(mTIM_1_BKUP!=candidate.mTIM_1_BKUP)wait_fail("mTIM_1_BKUP");
 if(mTIM_1_ENABLE_RELOAD!=candidate.mTIM_1_ENABLE_RELOAD)wait_fail("mTIM_1_ENABLE_RELOAD");
 if(mTIM_1_ENABLE_COUNT!=candidate.mTIM_1_ENABLE_COUNT)wait_fail("mTIM_1_ENABLE_COUNT");
 if(mTIM_1_LINKING!=candidate.mTIM_1_LINKING)wait_fail("mTIM_1_LINKING");
 if(mTIM_1_CURRENT!=candidate.mTIM_1_CURRENT)wait_fail("mTIM_1_CURRENT");
 if(mTIM_1_TIMER_DONE!=candidate.mTIM_1_TIMER_DONE)wait_fail("mTIM_1_TIMER_DONE");
 if(mTIM_1_LAST_CLOCK!=candidate.mTIM_1_LAST_CLOCK)wait_fail("mTIM_1_LAST_CLOCK");
 if(mTIM_1_BORROW_IN!=candidate.mTIM_1_BORROW_IN)wait_fail("mTIM_1_BORROW_IN");
 if(mTIM_1_BORROW_OUT!=candidate.mTIM_1_BORROW_OUT)wait_fail("mTIM_1_BORROW_OUT");
 if(mTIM_1_LAST_LINK_CARRY!=candidate.mTIM_1_LAST_LINK_CARRY)wait_fail("mTIM_1_LAST_LINK_CARRY");
 if(mTIM_1_LAST_COUNT!=candidate.mTIM_1_LAST_COUNT)wait_fail("mTIM_1_LAST_COUNT");
 if(mTIM_2_BKUP!=candidate.mTIM_2_BKUP)wait_fail("mTIM_2_BKUP");
 if(mTIM_2_ENABLE_RELOAD!=candidate.mTIM_2_ENABLE_RELOAD)wait_fail("mTIM_2_ENABLE_RELOAD");
 if(mTIM_2_ENABLE_COUNT!=candidate.mTIM_2_ENABLE_COUNT)wait_fail("mTIM_2_ENABLE_COUNT");
 if(mTIM_2_LINKING!=candidate.mTIM_2_LINKING)wait_fail("mTIM_2_LINKING");
 if(mTIM_2_CURRENT!=candidate.mTIM_2_CURRENT)wait_fail("mTIM_2_CURRENT");
 if(mTIM_2_TIMER_DONE!=candidate.mTIM_2_TIMER_DONE)wait_fail("mTIM_2_TIMER_DONE");
 if(mTIM_2_LAST_CLOCK!=candidate.mTIM_2_LAST_CLOCK)wait_fail("mTIM_2_LAST_CLOCK");
 if(mTIM_2_BORROW_IN!=candidate.mTIM_2_BORROW_IN)wait_fail("mTIM_2_BORROW_IN");
 if(mTIM_2_BORROW_OUT!=candidate.mTIM_2_BORROW_OUT)wait_fail("mTIM_2_BORROW_OUT");
 if(mTIM_2_LAST_LINK_CARRY!=candidate.mTIM_2_LAST_LINK_CARRY)wait_fail("mTIM_2_LAST_LINK_CARRY");
 if(mTIM_2_LAST_COUNT!=candidate.mTIM_2_LAST_COUNT)wait_fail("mTIM_2_LAST_COUNT");
 if(mTIM_3_BKUP!=candidate.mTIM_3_BKUP)wait_fail("mTIM_3_BKUP");
 if(mTIM_3_ENABLE_RELOAD!=candidate.mTIM_3_ENABLE_RELOAD)wait_fail("mTIM_3_ENABLE_RELOAD");
 if(mTIM_3_ENABLE_COUNT!=candidate.mTIM_3_ENABLE_COUNT)wait_fail("mTIM_3_ENABLE_COUNT");
 if(mTIM_3_LINKING!=candidate.mTIM_3_LINKING)wait_fail("mTIM_3_LINKING");
 if(mTIM_3_CURRENT!=candidate.mTIM_3_CURRENT)wait_fail("mTIM_3_CURRENT");
 if(mTIM_3_TIMER_DONE!=candidate.mTIM_3_TIMER_DONE)wait_fail("mTIM_3_TIMER_DONE");
 if(mTIM_3_LAST_CLOCK!=candidate.mTIM_3_LAST_CLOCK)wait_fail("mTIM_3_LAST_CLOCK");
 if(mTIM_3_BORROW_IN!=candidate.mTIM_3_BORROW_IN)wait_fail("mTIM_3_BORROW_IN");
 if(mTIM_3_BORROW_OUT!=candidate.mTIM_3_BORROW_OUT)wait_fail("mTIM_3_BORROW_OUT");
 if(mTIM_3_LAST_LINK_CARRY!=candidate.mTIM_3_LAST_LINK_CARRY)wait_fail("mTIM_3_LAST_LINK_CARRY");
 if(mTIM_3_LAST_COUNT!=candidate.mTIM_3_LAST_COUNT)wait_fail("mTIM_3_LAST_COUNT");
 if(mTIM_4_BKUP!=candidate.mTIM_4_BKUP)wait_fail("mTIM_4_BKUP");
 if(mTIM_4_ENABLE_RELOAD!=candidate.mTIM_4_ENABLE_RELOAD)wait_fail("mTIM_4_ENABLE_RELOAD");
 if(mTIM_4_ENABLE_COUNT!=candidate.mTIM_4_ENABLE_COUNT)wait_fail("mTIM_4_ENABLE_COUNT");
 if(mTIM_4_LINKING!=candidate.mTIM_4_LINKING)wait_fail("mTIM_4_LINKING");
 if(mTIM_4_CURRENT!=candidate.mTIM_4_CURRENT)wait_fail("mTIM_4_CURRENT");
 if(mTIM_4_TIMER_DONE!=candidate.mTIM_4_TIMER_DONE)wait_fail("mTIM_4_TIMER_DONE");
 if(mTIM_4_LAST_CLOCK!=candidate.mTIM_4_LAST_CLOCK)wait_fail("mTIM_4_LAST_CLOCK");
 if(mTIM_4_BORROW_IN!=candidate.mTIM_4_BORROW_IN)wait_fail("mTIM_4_BORROW_IN");
 if(mTIM_4_BORROW_OUT!=candidate.mTIM_4_BORROW_OUT)wait_fail("mTIM_4_BORROW_OUT");
 if(mTIM_4_LAST_LINK_CARRY!=candidate.mTIM_4_LAST_LINK_CARRY)wait_fail("mTIM_4_LAST_LINK_CARRY");
 if(mTIM_4_LAST_COUNT!=candidate.mTIM_4_LAST_COUNT)wait_fail("mTIM_4_LAST_COUNT");
 if(mTIM_5_BKUP!=candidate.mTIM_5_BKUP)wait_fail("mTIM_5_BKUP");
 if(mTIM_5_ENABLE_RELOAD!=candidate.mTIM_5_ENABLE_RELOAD)wait_fail("mTIM_5_ENABLE_RELOAD");
 if(mTIM_5_ENABLE_COUNT!=candidate.mTIM_5_ENABLE_COUNT)wait_fail("mTIM_5_ENABLE_COUNT");
 if(mTIM_5_LINKING!=candidate.mTIM_5_LINKING)wait_fail("mTIM_5_LINKING");
 if(mTIM_5_CURRENT!=candidate.mTIM_5_CURRENT)wait_fail("mTIM_5_CURRENT");
 if(mTIM_5_TIMER_DONE!=candidate.mTIM_5_TIMER_DONE)wait_fail("mTIM_5_TIMER_DONE");
 if(mTIM_5_LAST_CLOCK!=candidate.mTIM_5_LAST_CLOCK)wait_fail("mTIM_5_LAST_CLOCK");
 if(mTIM_5_BORROW_IN!=candidate.mTIM_5_BORROW_IN)wait_fail("mTIM_5_BORROW_IN");
 if(mTIM_5_BORROW_OUT!=candidate.mTIM_5_BORROW_OUT)wait_fail("mTIM_5_BORROW_OUT");
 if(mTIM_5_LAST_LINK_CARRY!=candidate.mTIM_5_LAST_LINK_CARRY)wait_fail("mTIM_5_LAST_LINK_CARRY");
 if(mTIM_5_LAST_COUNT!=candidate.mTIM_5_LAST_COUNT)wait_fail("mTIM_5_LAST_COUNT");
 if(mTIM_6_BKUP!=candidate.mTIM_6_BKUP)wait_fail("mTIM_6_BKUP");
 if(mTIM_6_ENABLE_RELOAD!=candidate.mTIM_6_ENABLE_RELOAD)wait_fail("mTIM_6_ENABLE_RELOAD");
 if(mTIM_6_ENABLE_COUNT!=candidate.mTIM_6_ENABLE_COUNT)wait_fail("mTIM_6_ENABLE_COUNT");
 if(mTIM_6_LINKING!=candidate.mTIM_6_LINKING)wait_fail("mTIM_6_LINKING");
 if(mTIM_6_CURRENT!=candidate.mTIM_6_CURRENT)wait_fail("mTIM_6_CURRENT");
 if(mTIM_6_TIMER_DONE!=candidate.mTIM_6_TIMER_DONE)wait_fail("mTIM_6_TIMER_DONE");
 if(mTIM_6_LAST_CLOCK!=candidate.mTIM_6_LAST_CLOCK)wait_fail("mTIM_6_LAST_CLOCK");
 if(mTIM_6_BORROW_IN!=candidate.mTIM_6_BORROW_IN)wait_fail("mTIM_6_BORROW_IN");
 if(mTIM_6_BORROW_OUT!=candidate.mTIM_6_BORROW_OUT)wait_fail("mTIM_6_BORROW_OUT");
 if(mTIM_6_LAST_LINK_CARRY!=candidate.mTIM_6_LAST_LINK_CARRY)wait_fail("mTIM_6_LAST_LINK_CARRY");
 if(mTIM_6_LAST_COUNT!=candidate.mTIM_6_LAST_COUNT)wait_fail("mTIM_6_LAST_COUNT");
 if(mTIM_7_BKUP!=candidate.mTIM_7_BKUP)wait_fail("mTIM_7_BKUP");
 if(mTIM_7_ENABLE_RELOAD!=candidate.mTIM_7_ENABLE_RELOAD)wait_fail("mTIM_7_ENABLE_RELOAD");
 if(mTIM_7_ENABLE_COUNT!=candidate.mTIM_7_ENABLE_COUNT)wait_fail("mTIM_7_ENABLE_COUNT");
 if(mTIM_7_LINKING!=candidate.mTIM_7_LINKING)wait_fail("mTIM_7_LINKING");
 if(mTIM_7_CURRENT!=candidate.mTIM_7_CURRENT)wait_fail("mTIM_7_CURRENT");
 if(mTIM_7_TIMER_DONE!=candidate.mTIM_7_TIMER_DONE)wait_fail("mTIM_7_TIMER_DONE");
 if(mTIM_7_LAST_CLOCK!=candidate.mTIM_7_LAST_CLOCK)wait_fail("mTIM_7_LAST_CLOCK");
 if(mTIM_7_BORROW_IN!=candidate.mTIM_7_BORROW_IN)wait_fail("mTIM_7_BORROW_IN");
 if(mTIM_7_BORROW_OUT!=candidate.mTIM_7_BORROW_OUT)wait_fail("mTIM_7_BORROW_OUT");
 if(mTIM_7_LAST_LINK_CARRY!=candidate.mTIM_7_LAST_LINK_CARRY)wait_fail("mTIM_7_LAST_LINK_CARRY");
 if(mTIM_7_LAST_COUNT!=candidate.mTIM_7_LAST_COUNT)wait_fail("mTIM_7_LAST_COUNT");
 if(mAUDIO_0_BKUP!=candidate.mAUDIO_0_BKUP)wait_fail("mAUDIO_0_BKUP");
 if(mAUDIO_0_ENABLE_RELOAD!=candidate.mAUDIO_0_ENABLE_RELOAD)wait_fail("mAUDIO_0_ENABLE_RELOAD");
 if(mAUDIO_0_ENABLE_COUNT!=candidate.mAUDIO_0_ENABLE_COUNT)wait_fail("mAUDIO_0_ENABLE_COUNT");
 if(mAUDIO_0_LINKING!=candidate.mAUDIO_0_LINKING)wait_fail("mAUDIO_0_LINKING");
 if(mAUDIO_0_CURRENT!=candidate.mAUDIO_0_CURRENT)wait_fail("mAUDIO_0_CURRENT");
 if(mAUDIO_0_TIMER_DONE!=candidate.mAUDIO_0_TIMER_DONE)wait_fail("mAUDIO_0_TIMER_DONE");
 if(mAUDIO_0_LAST_CLOCK!=candidate.mAUDIO_0_LAST_CLOCK)wait_fail("mAUDIO_0_LAST_CLOCK");
 if(mAUDIO_0_BORROW_IN!=candidate.mAUDIO_0_BORROW_IN)wait_fail("mAUDIO_0_BORROW_IN");
 if(mAUDIO_0_BORROW_OUT!=candidate.mAUDIO_0_BORROW_OUT)wait_fail("mAUDIO_0_BORROW_OUT");
 if(mAUDIO_0_LAST_LINK_CARRY!=candidate.mAUDIO_0_LAST_LINK_CARRY)wait_fail("mAUDIO_0_LAST_LINK_CARRY");
 if(mAUDIO_0_LAST_COUNT!=candidate.mAUDIO_0_LAST_COUNT)wait_fail("mAUDIO_0_LAST_COUNT");
 if(mAUDIO_0_VOLUME!=candidate.mAUDIO_0_VOLUME)wait_fail("mAUDIO_0_VOLUME");
 if(mAUDIO_0_INTEGRATE_ENABLE!=candidate.mAUDIO_0_INTEGRATE_ENABLE)wait_fail("mAUDIO_0_INTEGRATE_ENABLE");
 if(mAUDIO_0_WAVESHAPER!=candidate.mAUDIO_0_WAVESHAPER)wait_fail("mAUDIO_0_WAVESHAPER");
 if(mAUDIO_1_BKUP!=candidate.mAUDIO_1_BKUP)wait_fail("mAUDIO_1_BKUP");
 if(mAUDIO_1_ENABLE_RELOAD!=candidate.mAUDIO_1_ENABLE_RELOAD)wait_fail("mAUDIO_1_ENABLE_RELOAD");
 if(mAUDIO_1_ENABLE_COUNT!=candidate.mAUDIO_1_ENABLE_COUNT)wait_fail("mAUDIO_1_ENABLE_COUNT");
 if(mAUDIO_1_LINKING!=candidate.mAUDIO_1_LINKING)wait_fail("mAUDIO_1_LINKING");
 if(mAUDIO_1_CURRENT!=candidate.mAUDIO_1_CURRENT)wait_fail("mAUDIO_1_CURRENT");
 if(mAUDIO_1_TIMER_DONE!=candidate.mAUDIO_1_TIMER_DONE)wait_fail("mAUDIO_1_TIMER_DONE");
 if(mAUDIO_1_LAST_CLOCK!=candidate.mAUDIO_1_LAST_CLOCK)wait_fail("mAUDIO_1_LAST_CLOCK");
 if(mAUDIO_1_BORROW_IN!=candidate.mAUDIO_1_BORROW_IN)wait_fail("mAUDIO_1_BORROW_IN");
 if(mAUDIO_1_BORROW_OUT!=candidate.mAUDIO_1_BORROW_OUT)wait_fail("mAUDIO_1_BORROW_OUT");
 if(mAUDIO_1_LAST_LINK_CARRY!=candidate.mAUDIO_1_LAST_LINK_CARRY)wait_fail("mAUDIO_1_LAST_LINK_CARRY");
 if(mAUDIO_1_LAST_COUNT!=candidate.mAUDIO_1_LAST_COUNT)wait_fail("mAUDIO_1_LAST_COUNT");
 if(mAUDIO_1_VOLUME!=candidate.mAUDIO_1_VOLUME)wait_fail("mAUDIO_1_VOLUME");
 if(mAUDIO_1_INTEGRATE_ENABLE!=candidate.mAUDIO_1_INTEGRATE_ENABLE)wait_fail("mAUDIO_1_INTEGRATE_ENABLE");
 if(mAUDIO_1_WAVESHAPER!=candidate.mAUDIO_1_WAVESHAPER)wait_fail("mAUDIO_1_WAVESHAPER");
 if(mAUDIO_2_BKUP!=candidate.mAUDIO_2_BKUP)wait_fail("mAUDIO_2_BKUP");
 if(mAUDIO_2_ENABLE_RELOAD!=candidate.mAUDIO_2_ENABLE_RELOAD)wait_fail("mAUDIO_2_ENABLE_RELOAD");
 if(mAUDIO_2_ENABLE_COUNT!=candidate.mAUDIO_2_ENABLE_COUNT)wait_fail("mAUDIO_2_ENABLE_COUNT");
 if(mAUDIO_2_LINKING!=candidate.mAUDIO_2_LINKING)wait_fail("mAUDIO_2_LINKING");
 if(mAUDIO_2_CURRENT!=candidate.mAUDIO_2_CURRENT)wait_fail("mAUDIO_2_CURRENT");
 if(mAUDIO_2_TIMER_DONE!=candidate.mAUDIO_2_TIMER_DONE)wait_fail("mAUDIO_2_TIMER_DONE");
 if(mAUDIO_2_LAST_CLOCK!=candidate.mAUDIO_2_LAST_CLOCK)wait_fail("mAUDIO_2_LAST_CLOCK");
 if(mAUDIO_2_BORROW_IN!=candidate.mAUDIO_2_BORROW_IN)wait_fail("mAUDIO_2_BORROW_IN");
 if(mAUDIO_2_BORROW_OUT!=candidate.mAUDIO_2_BORROW_OUT)wait_fail("mAUDIO_2_BORROW_OUT");
 if(mAUDIO_2_LAST_LINK_CARRY!=candidate.mAUDIO_2_LAST_LINK_CARRY)wait_fail("mAUDIO_2_LAST_LINK_CARRY");
 if(mAUDIO_2_LAST_COUNT!=candidate.mAUDIO_2_LAST_COUNT)wait_fail("mAUDIO_2_LAST_COUNT");
 if(mAUDIO_2_VOLUME!=candidate.mAUDIO_2_VOLUME)wait_fail("mAUDIO_2_VOLUME");
 if(mAUDIO_2_INTEGRATE_ENABLE!=candidate.mAUDIO_2_INTEGRATE_ENABLE)wait_fail("mAUDIO_2_INTEGRATE_ENABLE");
 if(mAUDIO_2_WAVESHAPER!=candidate.mAUDIO_2_WAVESHAPER)wait_fail("mAUDIO_2_WAVESHAPER");
 if(mAUDIO_3_BKUP!=candidate.mAUDIO_3_BKUP)wait_fail("mAUDIO_3_BKUP");
 if(mAUDIO_3_ENABLE_RELOAD!=candidate.mAUDIO_3_ENABLE_RELOAD)wait_fail("mAUDIO_3_ENABLE_RELOAD");
 if(mAUDIO_3_ENABLE_COUNT!=candidate.mAUDIO_3_ENABLE_COUNT)wait_fail("mAUDIO_3_ENABLE_COUNT");
 if(mAUDIO_3_LINKING!=candidate.mAUDIO_3_LINKING)wait_fail("mAUDIO_3_LINKING");
 if(mAUDIO_3_CURRENT!=candidate.mAUDIO_3_CURRENT)wait_fail("mAUDIO_3_CURRENT");
 if(mAUDIO_3_TIMER_DONE!=candidate.mAUDIO_3_TIMER_DONE)wait_fail("mAUDIO_3_TIMER_DONE");
 if(mAUDIO_3_LAST_CLOCK!=candidate.mAUDIO_3_LAST_CLOCK)wait_fail("mAUDIO_3_LAST_CLOCK");
 if(mAUDIO_3_BORROW_IN!=candidate.mAUDIO_3_BORROW_IN)wait_fail("mAUDIO_3_BORROW_IN");
 if(mAUDIO_3_BORROW_OUT!=candidate.mAUDIO_3_BORROW_OUT)wait_fail("mAUDIO_3_BORROW_OUT");
 if(mAUDIO_3_LAST_LINK_CARRY!=candidate.mAUDIO_3_LAST_LINK_CARRY)wait_fail("mAUDIO_3_LAST_LINK_CARRY");
 if(mAUDIO_3_LAST_COUNT!=candidate.mAUDIO_3_LAST_COUNT)wait_fail("mAUDIO_3_LAST_COUNT");
 if(mAUDIO_3_VOLUME!=candidate.mAUDIO_3_VOLUME)wait_fail("mAUDIO_3_VOLUME");
 if(mAUDIO_3_INTEGRATE_ENABLE!=candidate.mAUDIO_3_INTEGRATE_ENABLE)wait_fail("mAUDIO_3_INTEGRATE_ENABLE");
 if(mAUDIO_3_WAVESHAPER!=candidate.mAUDIO_3_WAVESHAPER)wait_fail("mAUDIO_3_WAVESHAPER");
 if(memcmp(mAUDIO_OUTPUT,candidate.mAUDIO_OUTPUT,sizeof mAUDIO_OUTPUT))wait_fail("mAUDIO_OUTPUT");
 if(memcmp(mAUDIO_ATTEN,candidate.mAUDIO_ATTEN,sizeof mAUDIO_ATTEN))wait_fail("mAUDIO_ATTEN");
 if(mSTEREO!=candidate.mSTEREO)wait_fail("mSTEREO");
 if(mPAN!=candidate.mPAN)wait_fail("mPAN");
 if(mUART_RX_IRQ_ENABLE!=candidate.mUART_RX_IRQ_ENABLE)wait_fail("mUART_RX_IRQ_ENABLE");
 if(mUART_TX_IRQ_ENABLE!=candidate.mUART_TX_IRQ_ENABLE)wait_fail("mUART_TX_IRQ_ENABLE");
 if(mUART_RX_COUNTDOWN!=candidate.mUART_RX_COUNTDOWN)wait_fail("mUART_RX_COUNTDOWN");
 if(mUART_TX_COUNTDOWN!=candidate.mUART_TX_COUNTDOWN)wait_fail("mUART_TX_COUNTDOWN");
 if(mUART_SENDBREAK!=candidate.mUART_SENDBREAK)wait_fail("mUART_SENDBREAK");
 if(mUART_TX_DATA!=candidate.mUART_TX_DATA)wait_fail("mUART_TX_DATA");
 if(mUART_RX_DATA!=candidate.mUART_RX_DATA)wait_fail("mUART_RX_DATA");
 if(mUART_RX_READY!=candidate.mUART_RX_READY)wait_fail("mUART_RX_READY");
 if(mUART_PARITY_ENABLE!=candidate.mUART_PARITY_ENABLE)wait_fail("mUART_PARITY_ENABLE");
 if(mUART_PARITY_EVEN!=candidate.mUART_PARITY_EVEN)wait_fail("mUART_PARITY_EVEN");
 if(mUART_CABLE_PRESENT!=candidate.mUART_CABLE_PRESENT)wait_fail("mUART_CABLE_PRESENT");
 if(mUART_TX_CALLBACK_OBJECT!=candidate.mUART_TX_CALLBACK_OBJECT)wait_fail("mUART_TX_CALLBACK_OBJECT");
 if(memcmp(mUART_Rx_input_queue,candidate.mUART_Rx_input_queue,sizeof mUART_Rx_input_queue))wait_fail("mUART_Rx_input_queue");
 if(mUART_Rx_input_ptr!=candidate.mUART_Rx_input_ptr)wait_fail("mUART_Rx_input_ptr");
 if(mUART_Rx_output_ptr!=candidate.mUART_Rx_output_ptr)wait_fail("mUART_Rx_output_ptr");
 if(mUART_Rx_waiting!=candidate.mUART_Rx_waiting)wait_fail("mUART_Rx_waiting");
 if(mUART_Rx_framing_error!=candidate.mUART_Rx_framing_error)wait_fail("mUART_Rx_framing_error");
 if(mUART_Rx_overun_error!=candidate.mUART_Rx_overun_error)wait_fail("mUART_Rx_overun_error");
 if(mpDisplayCurrent!=candidate.mpDisplayCurrent)wait_fail("mpDisplayCurrent");
 if(mpRamPointer!=candidate.mpRamPointer)wait_fail("mpRamPointer");
 if(mLynxLine!=candidate.mLynxLine)wait_fail("mLynxLine");
 if(mLynxLineDMACounter!=candidate.mLynxLineDMACounter)wait_fail("mLynxLineDMACounter");
 if(mLynxAddr!=candidate.mLynxAddr)wait_fail("mLynxAddr");
 if(mAudioSampleRate!=candidate.mAudioSampleRate)wait_fail("mAudioSampleRate");
 if(mDisplayFormat!=candidate.mDisplayFormat)wait_fail("mDisplayFormat");
 if(mDisplayPitch!=candidate.mDisplayPitch)wait_fail("mDisplayPitch");
 if(mpUART_TX_CALLBACK!=candidate.mpUART_TX_CALLBACK)wait_fail("UART callback");

 return n;
}
#endif
