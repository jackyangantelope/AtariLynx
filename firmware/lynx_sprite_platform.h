/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_sprite.h"
#if R8_LYNX_SPRITE
uint32_t lsp_clock_us(void){return time_us_32();}
static void lsp_report(void){
 r8_profile_printf("LYNX_SPRITE id=LSP017 frame=%lu active=%lu ready=%lu epoch=240\n",(unsigned long)lsp_frame,(unsigned long)lsp_active,(unsigned long)lsp_ready);
 for(unsigned b=0;b<2;b++){
  const lsp_bin *s=&lsp_bins[b];
  r8_profile_printf("LYNX_SPRITE_BIN bin=%u frames=%lu cycles=%llu core_us=%llu calls=%lu measured=%lu nonzero=%lu guest_cycles=%llu sprite_us=%llu max_us=%lu frame_max_us=%lu lt64=%lu lt1000=%lu lt4000=%lu ge4000=%lu\n",b,(unsigned long)s->frames,(unsigned long long)s->cycles,(unsigned long long)s->core_us,(unsigned long)s->calls,(unsigned long)s->measured,(unsigned long)s->nonzero,(unsigned long long)s->guest_cycles,(unsigned long long)s->sprite_us,(unsigned long)s->max_us,(unsigned long)s->frame_max_us,(unsigned long)s->histogram[0],(unsigned long)s->histogram[1],(unsigned long)s->histogram[2],(unsigned long)s->histogram[3]);
 }
}
#endif
