/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_ADMIT_H
#define LYNX_ADMIT_H
#include <stdbool.h>
#include <stdint.h>
/* Guest-frame ages, not wall-clock or input-to-display latency. The pending
 * episode survives replacement of its newest pixels, so starvation is visible.
 * All distances use modulo-2^32 frame arithmetic, like platform frame counts. */
typedef struct {
    uint32_t first,latest,max_age,last_submit,max_gap;
    bool pending;
} la_counts;
extern const volatile uint32_t la_selector;
extern uint32_t la_mode;
extern la_counts la_stats;
void la_reset(void);
void la_offer(uint32_t frame,bool was_pending);
void la_finish(uint32_t frame,bool accepted);
void la_cancel(void);
uint32_t la_age(uint32_t frame);
uint32_t la_gap(uint32_t frame);
#endif
