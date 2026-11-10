/* SPDX-License-Identifier: BSD-3-Clause
 * Two cold report extensions only. Original LLD records remain in original SRAM.
 * Called only after the platform's 8MiB-PSRAM guard, never by a guest instruction.
 */
#include "lynx_romclk.h"
#ifndef LRC_MODE
#define LRC_MODE 1
#endif
#if R8_LYNX_ROMCLK
volatile const uint32_t lrc_selector=LRC_MODE;
typedef struct {uint32_t sequence;lrc_stats stats;} lrc_record;
#if defined(__arm__)
_Static_assert(2*sizeof(lrc_record)<=512,"ROM clock report reservation");
/* Names end at11409200; this extension ends before11409400. */
#define records (*(lrc_record (*)[2])0x11409200u)
#else
static lrc_record records[2];
#endif
static const lrc_stats empty;
void lrc_store(unsigned slot,uint32_t sequence,const lrc_stats *stats){
    if(slot>=2)return;
    lrc_record *r=&records[slot];r->stats=*stats;r->sequence=sequence;
}
const lrc_stats *lrc_get(unsigned slot,uint32_t sequence){
    if(slot>=2)return &empty;
    const lrc_record *r=&records[slot];return r->sequence==sequence?&r->stats:&empty;
}
#endif
