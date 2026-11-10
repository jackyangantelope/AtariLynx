/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_load_diag.h"
#include <string.h>
#if R8_LYNX_LOAD_DIAG
#if !defined(LLD_TEST_CLOCK)
#include "pico/time.h"
uint32_t lld_clock(void){return time_us_32();}
#endif
enum { LLD_CAPACITY=2 };
static lld_record records[LLD_CAPACITY];
static lld_record *active,*awaiting;
#if R8_LYNX_ROMCLK
void lld_rom_clock(const lrc_stats *stats){if(active)lrc_store((unsigned)(active-records),active->sequence,stats);}
#endif
static uint32_t head,used,sequence,core_start,phase_start,current_phase=LLD_PHASES;
uint32_t lld_wait_present,lld_dropped;
void lld_cancel_present(void){
    if(awaiting){awaiting->present=2;awaiting->ready=1;awaiting=0;}
    lld_wait_present=0;
}
void lld_begin(void){
    lld_cancel_present();++sequence;
    if(active || used==LLD_CAPACITY){++lld_dropped;return;}
    active=&records[(head+used)%LLD_CAPACITY];++used;
    memset(active,0,sizeof *active);active->sequence=sequence;
    active->start=lld_clock();current_phase=LLD_PHASES;
}
void lld_file(unsigned which,const lld_io *io){if(active && which<LLD_FILES)active->io[which]=*io;}
void lld_save_source(unsigned source){if(active)active->save_source=source;}
void lld_core_begin(void){if(active)core_start=lld_clock();}
void lld_phase(unsigned phase){
    if(!active)return;
    uint32_t now=lld_clock();
    if(current_phase<LLD_PHASES)active->phase_us[current_phase]+=now-phase_start;
    current_phase=phase;phase_start=now;
    if(phase<LLD_PHASES)active->phase_mask|=1u<<phase;
}
void lld_core_end(void){if(active){lld_phase(LLD_PHASES);active->core_us=lld_clock()-core_start;}}
void lld_boot_begin(uint32_t cycles){if(active)active->boot_cycles=cycles;}
void lld_boot_call(void){if(active)++active->boot_calls;}
void lld_boot_end(uint32_t cycles,uint32_t eof,uint32_t halt){
    if(active){active->boot_cycles=cycles-active->boot_cycles;active->boot_eof=eof;active->boot_halt=halt;}
}
void lld_path(uint32_t us){if(active)active->path_us=us;}
void lld_done(unsigned ok,uint32_t rom_crc,uint32_t save_crc){
    if(!active)return;
    active->total_us=lld_clock()-active->start;
    active->ok=ok;active->rom_crc=rom_crc;active->save_crc=save_crc;
#if R8_LYNX_BOOT_PCM
    active->audio=lbp_data;
#endif
#if R8_LYNX_BOOT_CPU
    active->cpu=lbc_data;
#if R8_LYNX_SCAN
    active->scan=lcs_data;
#if R8_LYNX_WIDE
    active->wide=lwd_data;
#endif
#endif
#endif
    if(ok){awaiting=active;lld_wait_present=1;}
    else active->ready=1;
    active=0;
}
void lld_first_present(uint32_t frame){
    if(awaiting){
        awaiting->first_us=lld_clock()-awaiting->start;awaiting->first_frame=frame;
        awaiting->present=1;awaiting->ready=1;awaiting=0;
    }
    lld_wait_present=0;
}
const lld_record *lld_pending(void){return used && records[head].ready?&records[head]:0;}
#if R8_LYNX_ROMCLK
const lrc_stats *lld_rom_clock_get(void){const lld_record *r=lld_pending();return r?lrc_get(head,r->sequence):lrc_get(2,0);}
#endif
void lld_release(void){if(lld_pending()){head=(head+1)%LLD_CAPACITY;--used;}}
#endif
