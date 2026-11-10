/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_FD_H
#define LYNX_FD_H
#include <stdint.h>
#ifndef R8_LYNX_FD_FAST
#define R8_LYNX_FD_FAST 0
#endif
#ifndef R8_LYNX_FD_ORACLE
#define R8_LYNX_FD_ORACLE 0
#endif
#if R8_LYNX_FD_FAST
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t lfd_fast[2],lfd_fallback[2];
#if R8_LYNX_FD_ORACLE
int lfd_oracle_pcm(int16_t l,int16_t r,uint32_t cycle,uint32_t audio_cycle);
#endif
#ifdef __cplusplus
}
#endif
#endif
#endif
