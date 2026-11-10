/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_HASH_H
#define LYNX_HASH_H
#include <stdint.h>
#if R8_LYNX_HASH_SWITCH
#ifdef __cplusplus
extern "C" {
#endif
extern const volatile uint32_t lh_selector;
extern uint32_t lh_mode,lh_video_skipped,lh_raw_skipped;
void lh_reset(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
