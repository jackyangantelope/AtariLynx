/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_wide.h"
#include <string.h>
#if R8_LYNX_WIDE
#ifndef LWD_MODE
#define LWD_MODE 1
#endif
_Static_assert(LWD_MODE==1 || LWD_MODE==2,"Fixed OFF=1 ON=2");
const volatile uint32_t lwd_selector=LWD_MODE;
lwd_stats lwd_data;
void lwd_reset(void){memset(&lwd_data,0,sizeof lwd_data);lwd_data.mode=lwd_selector==2?2:1;}
#endif
