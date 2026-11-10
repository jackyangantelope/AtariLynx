/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_ENCODE_H
#define LYNX_ENCODE_H
#include <stdint.h>
#ifndef R8_LYNX_ENCODE
#define R8_LYNX_ENCODE 0
#endif
#if R8_LYNX_ENCODE
#ifdef __cplusplus
extern "C" {
#endif
extern const volatile uint32_t le_selector;
extern uint32_t le_mode,le_packets,le_samples,le_pack_us,le_encode_us,le_queue_us,le_max_us;
void le_reset(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
