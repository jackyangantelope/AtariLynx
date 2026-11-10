/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_RUN_H
#define LYNX_RUN_H
#include <stdint.h>
#if R8_LYNX_RUN
#ifdef __cplusplus
extern "C" {
#endif
extern const volatile uint32_t lrn_selector;
extern uint32_t lrn_mode,lrn_lines,lrn_eligible,lrn_alias,lrn_runs,lrn_pixels;
void lrn_reset(void);
/* Only called with 2..16 visible pixels and a nonoverlapping source line.
 * Pixel1 and pixel2 target opposite nibbles: combining their store preserves
 * the final byte, with no device/CPU callback between the original writes.
 * The caller retains all original guest read/modify/write cycle charges. */
static inline void lrn_fill(uint8_t *dest,unsigned odd,unsigned color,unsigned n){
 if(odd){*dest=(*dest&0xf0u)|color;++dest;--n;}
 unsigned pairs=n>>1;uint8_t value=(uint8_t)(color*17u);
 while(pairs--)*dest++=value;
 if(n&1)*dest=(*dest&0x0fu)|(color<<4);
}
#ifdef __cplusplus
}
#endif
#endif
#endif
