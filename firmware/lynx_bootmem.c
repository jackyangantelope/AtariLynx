/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_bootmem.h"
#ifndef LBM_MODE
#define LBM_MODE 1
#endif
#if R8_LYNX_BOOTMEM
volatile const uint32_t lbm_selector=LBM_MODE;
#endif
