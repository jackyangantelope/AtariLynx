/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_cost.h"
#include "lynx_fd.h"
#include "lynx_sprite.h"
#include <cstring>
#if R8_LYNX_COST
#include "system.h"
lmc_bin lmc_bins[2];
#if R8_LYNX_FD_FAST
uint32_t lfd_fast[2],lfd_fallback[2];
#endif
uint32_t lmc_ready,lmc_cal_sum,lmc_cal_max;
#if defined(__arm__)
#define LMC_HOT __attribute__((section(".time_critical.lynx_cost"),noinline))
#else
#define LMC_HOT
uint32_t lmc_clock_us(void){return lsp_clock_us();}
#endif
void lmc_calibrate(void){lmc_cal_sum=lmc_cal_max=0;for(unsigned i=0;i<256;i++){uint32_t b=lmc_clock_us(),d=lmc_clock_us()-b;lmc_cal_sum+=d;if(d>lmc_cal_max)lmc_cal_max=d;}}
void lmc_reset(void){memset(lmc_bins,0,sizeof lmc_bins);lmc_ready=1;
#if R8_LYNX_FD_FAST
 memset(lfd_fast,0,sizeof lfd_fast);memset(lfd_fallback,0,sizeof lfd_fallback);
#endif
}
void lmc_frame_done(uint32_t cycles){lmc_bin &b=lmc_bins[lsp_active];++b.frames;b.cycles+=cycles;}
// origin 8: scheduler; 2: FD0A; other 0..7: other timer counter reads.
// There is no sampling, skipped device work or changed event prediction.
LMC_HOT void lmc_update(void *p,unsigned origin){
 CMikie *m=static_cast<CMikie *>(p);
 if(!lmc_ready){m->Update();return;}
 lmc_slot &s=lmc_bins[lsp_active].slot[origin==8?0:origin==2?1:2];++s.calls;
 uint32_t start=0;if(lsp_active)start=lmc_clock_us();
#if R8_LYNX_FD_FAST
 if(origin==2) {
#if R8_LYNX_FD_ORACLE
  bool fast=m->TryFD0AOracle();
#else
  bool fast=m->TryFD0A();
#endif
  if(fast)++lfd_fast[lsp_active];
  else {++lfd_fallback[lsp_active];m->Update();}
 } else m->Update();
#else
 m->Update();
#endif
 if(lsp_active){uint32_t dt=lmc_clock_us()-start;s.us+=dt;if(dt>s.max_us)s.max_us=dt;}
}
#endif
