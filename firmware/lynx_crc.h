/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_CRC_H
#define LYNX_CRC_H
#include <stdint.h>
#if R8_LYNX_CRC_TABLE
#ifdef __cplusplus
extern "C" {
#endif
extern const volatile uint32_t lcrc_selector;
extern const uint32_t lcrc_table[256];
#ifdef __cplusplus
}
#endif
#endif
#endif
