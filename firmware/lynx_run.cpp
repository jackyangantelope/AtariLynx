/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_run.h"
#if R8_LYNX_RUN
#ifndef LRN_MODE
#define LRN_MODE 1
#endif
static_assert(LRN_MODE==1 || LRN_MODE==2,"fixed run OFF=1 ON=2");
const volatile uint32_t lrn_selector=LRN_MODE;
uint32_t lrn_mode,lrn_lines,lrn_eligible,lrn_alias,lrn_runs,lrn_pixels;
void lrn_reset(void){lrn_mode=lrn_selector==2?2:1;lrn_lines=lrn_eligible=lrn_alias=lrn_runs=lrn_pixels=0;}
#endif
