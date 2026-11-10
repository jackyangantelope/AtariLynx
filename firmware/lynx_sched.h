/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_SCHED_H
#define LYNX_SCHED_H
#include <stdbool.h>
#include <stdint.h>
/* Fixed experimental budgets from LU021 hardware evidence, not WCET proofs. */
#define LP_RENDER_US 2500u
#define LP_CORE_GAP_US 5000u
typedef struct {uint32_t tries,allow,due,slack,source,over_budget;} lp_counts;
extern const volatile uint32_t lp_selector;
extern uint32_t lp_mode;
extern lp_counts lp_stats;
void lp_reset(void);
bool lp_allow(bool idle,uint64_t now_us,uint64_t next,uint32_t clock,
              uint64_t audio_next,unsigned queued);
#endif
