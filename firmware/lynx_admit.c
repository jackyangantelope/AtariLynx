/* SPDX-License-Identifier: BSD-3-Clause
 * Passive frame-count observations. No extra clock reads or pixel/audio access.
 */
#include "lynx_admit.h"
#include <string.h>
#ifndef LA_MODE
#define LA_MODE 1
#endif
_Static_assert(LA_MODE==1 || LA_MODE==2,"LA_MODE must be fixed OFF1 or ON2");
const volatile uint32_t la_selector=LA_MODE;
uint32_t la_mode;
la_counts la_stats;
uint32_t la_age(uint32_t frame){return la_stats.pending ? frame-la_stats.first : 0;}
uint32_t la_gap(uint32_t frame){return frame-la_stats.last_submit;}
static void observe(uint32_t frame){
    uint32_t age=la_age(frame),gap=la_gap(frame);
    if(age>la_stats.max_age)la_stats.max_age=age;
    if(gap>la_stats.max_gap)la_stats.max_gap=gap;
}
void la_reset(void){
    memset(&la_stats,0,sizeof(la_stats));
    la_mode=la_selector==2 ? 2u : 1u;
}
void la_offer(uint32_t frame,bool was_pending){
    if(!was_pending || !la_stats.pending)la_stats.first=frame;
    la_stats.latest=frame;la_stats.pending=true;observe(frame);
}
void la_cancel(void){la_stats.pending=false;la_stats.first=la_stats.latest=0;}
void la_finish(uint32_t frame,bool accepted){
    observe(frame);
    if(accepted){la_stats.last_submit=frame;la_cancel();}
}
