/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_COST_H
#define LYNX_COST_H
#include <stdint.h>
#ifndef R8_LYNX_COST
#define R8_LYNX_COST 0
#endif
#if R8_LYNX_COST
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t us;uint32_t calls,max_us;} lmc_slot;
typedef struct {lmc_slot slot[3];uint64_t core_us,cycles;uint32_t frames,reserved;} lmc_bin;
extern lmc_bin lmc_bins[2];
extern uint32_t lmc_ready,lmc_cal_sum,lmc_cal_max;
uint32_t lmc_clock_us(void);
void lmc_calibrate(void);
void lmc_reset(void);
void lmc_frame_done(uint32_t cycles);
void lmc_update(void *mikie,unsigned origin);
#ifdef __cplusplus
}
#endif
#endif
#endif
