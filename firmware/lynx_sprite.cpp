/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_sprite.h"
#include <cstring>
#if R8_LYNX_SPRITE
#if !defined(__arm__) && !defined(LSP_UNIT_TEST)
#include <chrono>
uint32_t lsp_clock_us(void){return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
#endif
#if defined(LSP_UNIT_TEST)
extern "C" uint32_t lsp_test_paint(void *);
static uint32_t paint(void *p){return lsp_test_paint(p);}
#else
#include "system.h"
static uint32_t paint(void *p){return static_cast<CSusie *>(p)->PaintSprites();}
#endif
lsp_bin lsp_bins[2];
uint32_t lsp_frame,lsp_active,lsp_ready,lsp_cal_sum,lsp_cal_max;
static uint32_t frame_us;
void lsp_calibrate(void){
 lsp_cal_sum=lsp_cal_max=0;
 for(unsigned i=0;i<256;i++){
  uint32_t begin=lsp_clock_us(),dt=lsp_clock_us()-begin;
  lsp_cal_sum+=dt;if(dt>lsp_cal_max)lsp_cal_max=dt;
 }
}
void lsp_reset(void){memset(lsp_bins,0,sizeof lsp_bins);lsp_frame=lsp_active=frame_us=0;lsp_ready=1;}
void lsp_frame_begin(void){lsp_active=(lsp_frame/240)%2;++lsp_frame;frame_us=0;}
void lsp_frame_done(uint32_t cycles){
 lsp_bin &s=lsp_bins[lsp_active];++s.frames;s.cycles+=cycles;
 if(frame_us>s.frame_max_us)s.frame_max_us=frame_us;
}
/* Full original draw, including every return value and collision write.
 * Both phases count calls; ON additionally times EVERY call with the us32 clock.
 * Wall time includes interrupts and the return clock read, not an IRQ-free cost. */
uint32_t lsp_paint(void *susie){
 if(!lsp_ready)return paint(susie);
 lsp_bin &s=lsp_bins[lsp_active];++s.calls;
 uint32_t start=0;if(lsp_active)start=lsp_clock_us();
 const uint32_t cycles=paint(susie);
 if(lsp_active){
  const uint32_t dt=lsp_clock_us()-start;
  s.sprite_us+=dt;frame_us+=dt;++s.measured;
  if(dt>s.max_us)s.max_us=dt;
  ++s.histogram[dt<64?0:dt<1000?1:dt<4000?2:3];
 }
 s.guest_cycles+=cycles;if(cycles)++s.nonzero;
 return cycles;
}
#endif
