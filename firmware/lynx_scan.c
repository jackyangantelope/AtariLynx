/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_scan.h"
#include <string.h>
#if R8_LYNX_SCAN
#ifndef LCS_MODE
#define LCS_MODE 1
#endif
_Static_assert(LCS_MODE==1 || LCS_MODE==2,"fixed OFF=1 ON=2");
const volatile uint32_t lcs_selector=LCS_MODE;
lcs_stats lcs_data;
uint32_t lcs_active;
void lcs_reset(void){memset(&lcs_data,0,sizeof lcs_data);lcs_active=0;lcs_data.mode=lcs_selector==2?2:1;}
#endif
