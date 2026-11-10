/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_boot_cpu.h"
#include "lynx_load_diag.h"
#include <string.h>
#if R8_LYNX_BOOT_CPU
#ifndef LBC_MODE
#define LBC_MODE 1
#endif
_Static_assert(LBC_MODE==1 || LBC_MODE==2,"fixed OFF=1, ON=2");
const volatile uint32_t lbc_selector=LBC_MODE;
lbc_stats lbc_data;
void lbc_reset(void){memset(&lbc_data,0,sizeof lbc_data);lbc_data.mode=lbc_selector==2?2:1;}
void lbc_call_end(uint32_t start,uint32_t cycles,uint32_t hits){
    uint32_t us=lld_clock()-start,n=lbc_data.calls++;
    /* Call1, calls2..16,17..32,33..48,49..64; indexes never depend on time. */
    unsigned i=n==0?0:n<16?1:n<32?2:n<48?3:4;
    lbc_bin *b=&lbc_data.bin[i];++b->calls;b->us+=us;
    if(us>b->max_us)b->max_us=us;
    b->cycles+=cycles;b->short_hits+=hits;lbc_data.short_hits+=hits;
}
#endif
