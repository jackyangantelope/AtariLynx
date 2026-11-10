/* SPDX-License-Identifier: BSD-3-Clause
 * Main-thread diagnostic admission only. Does not advance guest time, call
 * service, consume/produce PCM, change presentation or reset core statistics.
 */
#include "lynx_report_gate.h"
#include <string.h>
#ifndef LQ_MODE
#define LQ_MODE 2
#endif
_Static_assert(LQ_MODE==1 || LQ_MODE==2,"LQ_MODE must be fixed OFF1 or ON2");
const volatile uint32_t lq_selector=LQ_MODE;
uint32_t lq_mode;
lq_counts lq_stats;
void lq_reset(void){memset(&lq_stats,0,sizeof lq_stats);lq_mode=lq_selector==2?2u:1u;}
bool lq_allow(uint64_t now,uint64_t deadline,uint64_t audio_next,
              unsigned queued,bool active,bool tx_empty){
    ++lq_stats.tries;
    lq_stats.wait_us=now>=deadline?now-deadline:0;
    if(lq_stats.wait_us>lq_stats.max_wait_us)lq_stats.max_wait_us=lq_stats.wait_us;
    lq_stats.pending=true;
    /* Sparse observer lines may be queued while a full report is pending.
     * Never append the full burst to one of those lines. */
    if(!tx_empty){++lq_stats.tx;return false;}
    if(lq_mode==2 && active){
        /* Playback debt and inclusive <= consumer are both accounted for.
         * These are experimental margins, not a physical WCET guarantee. */
        uint64_t horizon=(now+LQ_REPORT_US+LQ_CORE_GAP_US)*44100u;
        if(audio_next+(uint64_t)queued*1000000u<=horizon){
            ++lq_stats.source;return false;
        }
    }else ++lq_stats.bypass;
    ++lq_stats.allow;lq_stats.pending=false;return true;
}
void lq_finish(uint64_t cost){
    if(cost>lq_stats.max_cost_us)lq_stats.max_cost_us=cost;
    if(cost>LQ_REPORT_US)++lq_stats.over_budget;
}
