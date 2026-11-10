/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_BOUND_H
#define LYNX_BOUND_H
#include <stdint.h>
#if R8_LYNX_BOUND
#ifdef __cplusplus
extern "C" {
#endif
extern const volatile uint32_t lb_selector;
extern uint32_t lb_mode,lb_devices,lb_accepted,lb_loops,lb_short;
void lb_reset(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
