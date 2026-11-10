/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#if R8_LYNX_BOOTMEM
extern volatile const uint32_t lbm_selector;
int lbm_decrypt_block(int accumulator,unsigned char *result,const unsigned char *encrypted,const unsigned char *modulus);
#endif
#ifdef __cplusplus
}
#endif
