/* SPDX-License-Identifier: BSD-3-Clause
 * LRC018: fixed exact output conversion; no guest/audio/presentation changes.
 */
#include "lynx_render.h"
#if R8_LYNX_RENDER_PAIR
#ifndef LR_MODE
#define LR_MODE 1
#endif
static_assert(LR_MODE==1 || LR_MODE==2,"OFF1 old conversion; ON2 paired conversion");
const volatile uint32_t lr_selector=LR_MODE;
uint32_t lr_mode,lr_calls;
void lr_reset(void){lr_mode=lr_selector==2?2:1;lr_calls=0;}
#endif
