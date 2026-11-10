/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_GAP_REPORT_H
#define LYNX_GAP_REPORT_H
#include "lynx_feed.h"
#if R8_LYNX_SAVE_FEED
static void lgap_report(void){
    const lgap_record *r=&lgap_worst;uint32_t drops=r8_tx_drops;
    r8_profile_printf("LYNX_GAP id=LFE030 n=%lu valid=%lu frame=%lu cycle=%lu prev_cycle=%lu mask=%lu kind=%lu span_us=%lu prev_tail_us=%lu gap_us=%lu usb_us=%lu pad_us=%lu poll_us=%lu body_us=%lu qp=%lu q0=%lu q1=%lu under=%lu\n",
        (unsigned long)lgap_count,(unsigned long)r->valid,(unsigned long)r->frame,
        (unsigned long)r->cycle,(unsigned long)r->prev_cycle,(unsigned long)r->mask,(unsigned long)r->kind,
        (unsigned long)r->span_us,(unsigned long)r->prev_tail_us,(unsigned long)r->gap_us,
        (unsigned long)r->usb_us,(unsigned long)r->pad_us,(unsigned long)r->poll_us,(unsigned long)r->body_us,
        (unsigned long)r->qprev,(unsigned long)r->q0,(unsigned long)r->q1,(unsigned long)r->under);
    if(drops==r8_tx_drops){memset(&lgap_worst,0,sizeof lgap_worst);lgap_count=0;}
}
#endif
#endif
