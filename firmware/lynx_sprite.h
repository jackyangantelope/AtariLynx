/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_SPRITE_H
#define LYNX_SPRITE_H
#include <stdint.h>
#ifndef R8_LYNX_SPRITE
#define R8_LYNX_SPRITE 0
#endif
#if R8_LYNX_SPRITE
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
 uint64_t core_us,cycles,sprite_us,guest_cycles;
 uint32_t frames,calls,measured,nonzero,max_us,frame_max_us;
 uint32_t histogram[4]; /* <64, <1000, <4000, >=4000 us */
} lsp_bin;
extern lsp_bin lsp_bins[2];
extern uint32_t lsp_frame,lsp_active,lsp_ready,lsp_cal_sum,lsp_cal_max;
uint32_t lsp_clock_us(void);
void lsp_calibrate(void);
void lsp_reset(void);
void lsp_frame_begin(void);
void lsp_frame_done(uint32_t cycles);
uint32_t lsp_paint(void *susie);
#ifdef __cplusplus
}
#endif
#endif
#endif
