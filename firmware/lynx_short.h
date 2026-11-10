/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_SHORT_H
#define LYNX_SHORT_H
#include <stdint.h>
#include "lynx_wait.h"
#ifndef R8_LYNX_SHORT_MISS
#define R8_LYNX_SHORT_MISS 63
#endif
#if R8_LYNX_SHORT_MISS != 0 && R8_LYNX_SHORT_MISS != 7 && R8_LYNX_SHORT_MISS != 63
#error Invalid short dispatch miss cooldown
#endif
#ifndef R8_LYNX_SHORT
#define R8_LYNX_SHORT 0
#endif
#ifndef R8_LYNX_SHORT_MODE
#define R8_LYNX_SHORT_MODE 2
#endif
#if R8_LYNX_SHORT
#if R8_LYNX_SHORT_MODE != 1 && R8_LYNX_SHORT_MODE != 2
#error Invalid short dispatch mode
#endif
#if defined(__arm__)
#define LS_FRAME_EXEC __attribute__((section(".time_critical.lynx_short_frame"),noinline,optimize("O3")))
#else
#define LS_FRAME_EXEC __attribute__((noinline,optimize("O3")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t core_us,cycles;uint32_t frames,fast,tested;} ls_bin;
extern ls_bin ls_bins[2];
extern uint32_t ls_frame,ls_active;
void ls_reset(void);
void ls_frame_begin(void);
void ls_frame_done(uint32_t cycles);
int ls_should_probe(void);
void ls_probe_done(uint32_t hits);
#ifdef __cplusplus
}
#endif
#else
#define LS_FRAME_EXEC
#endif
#endif

#if R8_LYNX_CPU_PROBE
void lynx_cpu_count(unsigned opcode,unsigned pc,unsigned fast);
void lynx_cpu_report(void);
#endif

#ifndef R8_LYNX_HOTSET
#define R8_LYNX_HOTSET 0
#endif
