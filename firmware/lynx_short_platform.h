/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_short.h"
#if R8_LYNX_SHORT
static void ls_report(void){
 r8_profile_printf("LYNX_SHORT id=LS016 frame=%lu active=%lu mode=%u epoch=240\n",(unsigned long)ls_frame,(unsigned long)ls_active,R8_LYNX_SHORT_MODE);
 for(unsigned b=0;b<2;b++){
  const ls_bin *s=&ls_bins[b];
  r8_profile_printf("LYNX_SHORT_BIN bin=%u frames=%lu cycles=%llu core_us=%llu fast=%lu tested=%lu\n",b,(unsigned long)s->frames,(unsigned long long)s->cycles,(unsigned long long)s->core_us,(unsigned long)s->fast,(unsigned long)s->tested);
 }
}
#endif
