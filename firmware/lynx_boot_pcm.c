/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_boot_pcm.h"
#include <string.h>
#if R8_LYNX_BOOT_PCM
#ifndef LBP_MODE
#define LBP_MODE 1
#endif
_Static_assert(LBP_MODE==1 || LBP_MODE==2,"fixed OFF=1 ON=2");
const volatile uint32_t lbp_selector=LBP_MODE;
uint32_t lbp_mode,lbp_active;
lbp_stats lbp_data;
lbp_ready_fn lbp_ready;
lbp_emit_fn lbp_emit;
#ifndef LBP_TEST_CLOCK
#include "pico/time.h"
uint32_t lbp_clock(void){return time_us_32();}
#endif
void lbp_set_sink(lbp_ready_fn ready,lbp_emit_fn emit){lbp_ready=ready;lbp_emit=emit;}
void lbp_reset(void){lbp_mode=lbp_selector==2?2:1;lbp_active=0;memset(&lbp_data,0,sizeof lbp_data);}
void lbp_long_end(uint32_t raw,uint32_t start,bool batched){
    uint32_t us=lbp_clock()-start;
    ++lbp_data.calls;lbp_data.raw_long+=raw;lbp_data.sound_us+=us;
    if(raw>lbp_data.max_raw)lbp_data.max_raw=raw;
    if(us>lbp_data.max_us)lbp_data.max_us=us;
    if(batched)++lbp_data.batched;else ++lbp_data.fallback;
}
void lbp_source(uint32_t before,uint32_t after,uint32_t enqueued,uint32_t dropped){
    lbp_data.queued_before=before;lbp_data.queued_after=after;
    lbp_data.enqueued=enqueued;lbp_data.dropped=dropped;
}
#endif
