/* SPDX-License-Identifier: BSD-3-Clause
 * LCS032: startup-only, exact cartridge byte/checksum blocks. */
#ifndef LYNX_SCAN_H
#define LYNX_SCAN_H
#include <stdint.h>
#if R8_LYNX_SCAN
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint32_t mode,probes,batches,bytes,cycles,guard,boundary; } lcs_stats;
extern const volatile uint32_t lcs_selector;
extern lcs_stats lcs_data;
extern uint32_t lcs_active;
void lcs_reset(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
