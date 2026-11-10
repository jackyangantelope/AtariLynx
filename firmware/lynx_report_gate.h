/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_REPORT_GATE_H
#define LYNX_REPORT_GATE_H
#include <stdbool.h>
#include <stdint.h>
enum { LQ_REPORT_US=3500, LQ_CORE_GAP_US=5000 };
typedef struct {
    uint32_t tries,allow,source,tx,bypass,over_budget;
    uint64_t wait_us,max_wait_us,max_cost_us;
    bool pending;
} lq_counts;
extern const volatile uint32_t lq_selector;
extern uint32_t lq_mode;
extern lq_counts lq_stats;
void lq_reset(void);
bool lq_allow(uint64_t now,uint64_t deadline,uint64_t audio_next,
              unsigned queued,bool active,bool tx_empty);
void lq_finish(uint64_t cost);
#endif
