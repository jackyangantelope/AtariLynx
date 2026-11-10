/* SPDX-License-Identifier: BSD-3-Clause
 * Presentation admission only. Never advances guest time, consumes/produces
 * audio, changes input, blocks, or touches framebuffer ownership.
 */
#include "lynx_sched.h"
#if R8_LYNX_ADMIT
#include "lynx_admit.h"
#endif
#include <string.h>
#ifndef LP_MODE
#define LP_MODE 1
#endif
_Static_assert(LP_MODE==1 || LP_MODE==2,"LP_MODE must be fixed OFF1 or ON2");
const volatile uint32_t lp_selector=LP_MODE;
uint32_t lp_mode;
lp_counts lp_stats;
void lp_reset(void){
    memset(&lp_stats,0,sizeof(lp_stats));
    lp_mode=lp_selector==2 ? 2u : 1u;
}
bool lp_allow(bool idle,uint64_t now_us,uint64_t next,uint32_t clock,
              uint64_t audio_next,unsigned queued){
    ++lp_stats.tries;
    if(lp_mode==2){
#if R8_LYNX_ADMIT
        /* Both variants retain the due-core-first outer loop. ON removes
         * only deadline/slack vetoes on already completed frame conversion. */
        if(la_mode!=2){
#endif
        uint64_t scaled=now_us*clock;
        if(scaled>=next){++lp_stats.due;return false;}
        /* Only idle retries need a complete conversion window. Applying
         * this to the normal post-frame site can starve all presentation
         * when core work leaves slightly less than the worst-case budget. */
        if(idle && next-scaled<((uint64_t)LP_RENDER_US*clock)){
            ++lp_stats.slack;return false;
        }
#if R8_LYNX_ADMIT
        }
#endif
        /* Next sample time and queue count are in 1/44100 microsecond units.
         * This includes already accrued playback debt, unlike a raw low-water
         * count. Strict > covers the inclusive audio_next<=now consumer.
         * All operands fit uint64 throughout the platform clock's lifetime
         * supported by the existing now_us*16MHz frame scheduler. */
        uint64_t horizon=(now_us+LP_RENDER_US+LP_CORE_GAP_US)*44100u;
        if(audio_next+(uint64_t)queued*1000000u<=horizon){
            ++lp_stats.source;return false;
        }
    }
    ++lp_stats.allow;return true;
}
