/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_WIDE_H
#define LYNX_WIDE_H
#include <stdint.h>
#if R8_LYNX_WIDE
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint32_t mode,entries,accepted,bytes,groups,max_bytes,cycles; } lwd_stats;
extern const volatile uint32_t lwd_selector;
extern lwd_stats lwd_data;
void lwd_reset(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
