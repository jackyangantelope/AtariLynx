/* SPDX-License-Identifier: BSD-3-Clause
 * LPH017: fixed diagnostic pixel-scan experiment only. No guest/audio changes.
 */
#include "lynx_hash.h"
#if R8_LYNX_HASH_SWITCH
#ifndef LH_MODE
#define LH_MODE 1
#endif
static_assert(LH_MODE==1 || LH_MODE==2,"OFF1 keeps hashes, ON2 skips pixel scans");
const volatile uint32_t lh_selector=LH_MODE;
uint32_t lh_mode,lh_video_skipped,lh_raw_skipped;
void lh_reset(void){lh_mode=lh_selector==2?2:1;lh_video_skipped=lh_raw_skipped=0;}
#endif
