/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_WAIT_H
#define LYNX_WAIT_H
#include <stdint.h>
#ifndef R8_LYNX_WAIT
#define R8_LYNX_WAIT 0
#endif
#ifndef LW_ORACLE
#define LW_ORACLE 0
#endif
#if R8_LYNX_WAIT
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t probes,batches,loops;uint64_t cycles;} lw_bin;
extern lw_bin lw_bins[2];
extern uint32_t lw_mode;
extern const volatile uint32_t lw_selector;
extern uint32_t lw_frames,lw_input_hash,lw_sample_frame,lw_raw_hash;
void lw_reset(void);
void lw_frame_done(uint16_t buttons,const uint16_t *pixels);
#ifdef __cplusplus
}
#endif
#endif
#endif
