/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_CRCMERGE_H
#define LYNX_CRCMERGE_H
#include <stddef.h>
#include <stdint.h>
#if R8_LYNX_CRC_MERGE
#ifdef __cplusplus
extern "C" {
#endif
extern const volatile uint32_t lcm_selector;
/* Finalized reflected CRC32 (poly0xedb88320, initial/final XOR0xffffffff).
 * Return CRC(A || B) from CRC(A), CRC(B), and the length of B in bytes.
 * Empty B returns first. No memory scan, allocation or persistent state. */
uint32_t lcm_combine(uint32_t first,uint32_t second,size_t second_bytes);
#ifdef __cplusplus
}
#endif
#endif
#endif
