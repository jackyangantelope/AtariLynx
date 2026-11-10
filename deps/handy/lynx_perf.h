/* SPDX-License-Identifier: BSD-3-Clause
 * Platform placement and exact arithmetic optimizations. Disabled by default.
 */
#ifndef LYNX_PERF_H
#define LYNX_PERF_H
#include "lynx_fd.h"
#ifndef R8_LYNX_HOT_CPU
#define R8_LYNX_HOT_CPU 0
#endif
#ifndef R8_LYNX_FAST_LFSR
#define R8_LYNX_FAST_LFSR 0
#endif
#ifndef R8_LYNX_CPU_OPT
#define R8_LYNX_CPU_OPT "O3"
#endif
#ifndef R8_LYNX_CPU_FLASH
#define R8_LYNX_CPU_FLASH 0
#endif
#ifndef R8_LYNX_HOT_MIKIE
#define R8_LYNX_HOT_MIKIE 0
#endif
#if R8_LYNX_HOT_MIKIE && defined(__arm__)
#define LYNX_MIKIE_EXEC(name) __attribute__((section(".time_critical." #name)))
#else
#define LYNX_MIKIE_EXEC(name)
#endif
#if R8_LYNX_HOT_CPU && defined(__GNUC__)
#if defined(__arm__)
#if R8_LYNX_CPU_FLASH
#define LYNX_CPU_EXEC __attribute__((noinline,optimize(R8_LYNX_CPU_OPT)))
#else
#define LYNX_CPU_EXEC __attribute__((section(".time_critical.lynx_cpu"),noinline,optimize(R8_LYNX_CPU_OPT)))
#endif
#define LYNX_IO_EXEC __attribute__((section(".time_critical.lynx_io"),noinline))
#else
#define LYNX_CPU_EXEC __attribute__((noinline,optimize(R8_LYNX_CPU_OPT)))
#define LYNX_IO_EXEC __attribute__((noinline))
#endif
#else
#define LYNX_CPU_EXEC
#define LYNX_IO_EXEC
#endif
static inline unsigned int lynx_lfsr_exact(unsigned int current) {
    unsigned int switches=current>>12;
    unsigned int taps=((switches&1u)<<7)|((switches&0x7eu)>>1)|((switches&0x180u)<<3);
    unsigned int parity=current&taps;
    parity^=parity>>8;parity^=parity>>4;parity^=parity>>2;parity^=parity>>1;
    return (current&0xfffff000u)|((current<<1)&0xffeu)|((parity^1u)&1u);
}
#endif
