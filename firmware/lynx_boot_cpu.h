/* SPDX-License-Identifier: BSD-3-Clause
 * LBC031: fixed startup dispatch; passive timing at UpdateFrame boundaries.
 * No guest instruction, event, PCM sample or file operation is skipped.
 */
#ifndef LYNX_BOOT_CPU_H
#define LYNX_BOOT_CPU_H
#include <stdint.h>
#if R8_LYNX_BOOT_CPU
#ifdef __cplusplus
extern "C" {
#endif
enum { LBC_BINS=5 };
typedef struct {uint32_t calls,us,max_us,cycles,short_hits;} lbc_bin;
typedef struct {uint32_t mode,calls,short_hits; lbc_bin bin[LBC_BINS];} lbc_stats;
extern const volatile uint32_t lbc_selector;
extern lbc_stats lbc_data;
void lbc_reset(void);
void lbc_call_end(uint32_t start,uint32_t cycles,uint32_t hits);
#ifdef __cplusplus
}
#endif
#endif
#endif
