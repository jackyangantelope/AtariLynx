/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_RENDER_H
#define LYNX_RENDER_H
#include <stdint.h>
#if R8_LYNX_RENDER_PAIR
#ifdef __cplusplus
extern "C" {
#endif
extern const volatile uint32_t lr_selector;
extern uint32_t lr_mode,lr_calls;
void lr_reset(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
