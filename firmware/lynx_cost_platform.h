/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_cost.h"
#include "lynx_fd.h"
#if R8_LYNX_COST
__attribute__((section(".time_critical.lynx_cost_clock"),noinline)) uint32_t lmc_clock_us(void){return time_us_32();}
static void lmc_report(void){
 for(unsigned b=0;b<2;b++){
  const lmc_bin *s=&lmc_bins[b];
  r8_profile_printf("LYNX_COST_BIN id=LMC019 bin=%u frames=%lu cycles=%llu core_us=%llu event_calls=%lu event_us=%llu event_max=%lu fd0a_calls=%lu fd0a_us=%llu fd0a_max=%lu other_calls=%lu other_us=%llu other_max=%lu\n",b,(unsigned long)s->frames,(unsigned long long)s->cycles,(unsigned long long)s->core_us,(unsigned long)s->slot[0].calls,(unsigned long long)s->slot[0].us,(unsigned long)s->slot[0].max_us,(unsigned long)s->slot[1].calls,(unsigned long long)s->slot[1].us,(unsigned long)s->slot[1].max_us,(unsigned long)s->slot[2].calls,(unsigned long long)s->slot[2].us,(unsigned long)s->slot[2].max_us);
 }
#if R8_LYNX_FD_FAST
 for(unsigned b=0;b<2;b++)r8_profile_printf("LYNX_FD_BIN id=LFD010 bin=%u fast=%lu fallback=%lu\n",b,(unsigned long)lfd_fast[b],(unsigned long)lfd_fallback[b]);
#endif

}
#endif
