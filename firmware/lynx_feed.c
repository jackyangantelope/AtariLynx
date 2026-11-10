/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_feed.h"
#include <string.h>
#if R8_LYNX_SAVE_FEED
#ifndef LFE_MODE
#define LFE_MODE 1
#endif
_Static_assert(LFE_MODE==1 || LFE_MODE==2,"Fixed OFF=1 ON=2");
const volatile uint32_t lfe_selector=LFE_MODE;
uint32_t lfe_mode;
lfe_stats lfe_data;
lgap_record lgap_worst;
uint32_t lgap_count,lgap_valid,lgap_audio,lgap_end,lgap_q,lgap_cycle;
void lgap_reset(void){memset(&lgap_worst,0,sizeof lgap_worst);lgap_count=lgap_valid=0;}
void lfe_reset(void){lfe_mode=lfe_selector==2?2:1;memset(&lfe_data,0,sizeof lfe_data);lgap_reset();}
#endif
