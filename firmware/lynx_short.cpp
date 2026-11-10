/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_short.h"
#include <cstring>
#if R8_LYNX_SHORT
ls_bin ls_bins[2];
uint32_t ls_frame,ls_active;
static uint32_t retry;
void ls_reset(void){memset(ls_bins,0,sizeof ls_bins);ls_frame=ls_active=retry=0;}
void ls_frame_begin(void){ls_active=R8_LYNX_SHORT_MODE==1?1:(ls_frame/240)%2;++ls_frame;}
void ls_frame_done(uint32_t cycles){++ls_bins[ls_active].frames;ls_bins[ls_active].cycles+=cycles;}
int ls_should_probe(void){if(!ls_active)return 0;if(retry){--retry;return 0;}++ls_bins[1].tested;return 1;}
void ls_probe_done(uint32_t hits){ls_bins[1].fast+=hits;retry=hits<128?31:0;}
#endif

#if R8_LYNX_CPU_PROBE
#include <cstdio>
#include "lynx_cost.h"
static uint64_t opcounts[2][256];
void lynx_cpu_count(unsigned opcode,unsigned pc,unsigned fast){
 (void)pc;if(lmc_ready && ls_frame>1200)++opcounts[fast][opcode];
}
void lynx_cpu_report(void){
 for(unsigned op=0;op<256;op++)if(opcounts[0][op]||opcounts[1][op])
 printf("CPU_OP op=%02x original=%llu short=%llu\n",op,(unsigned long long)opcounts[0][op],(unsigned long long)opcounts[1][op]);
}
#endif
