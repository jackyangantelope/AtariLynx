/* SPDX-License-Identifier: BSD-3-Clause
 * Main thread only; fixed storage, no I/O or guest/audio access. Wall-clock
 * intervals include interrupts. SD byte loops are timed only at their edges.
 */
#include "lynx_save_diag.h"
#include "pico/stdlib.h"
#include <string.h>
#ifndef LSV_MODE
#define LSV_MODE 2
#endif
_Static_assert(LSV_MODE==1 || LSV_MODE==2,"Fixed LSV mode required");
const volatile uint32_t lsv_selector=LSV_MODE;
uint32_t lsv_mode,lsv_transactions,lsv_dropped;
static lsv_record records[LSV_CAPACITY];
static unsigned head,used,phase_index;
static uint32_t transaction_start,phase_start;
static lsv_record *active;
void lsv_reset(void){
    memset(records,0,sizeof records);head=used=0;active=0;
    lsv_transactions=lsv_dropped=0;phase_index=LSV_PHASES;
    lsv_mode=lsv_selector==2?2u:1u;
}
uint32_t lsv_clock(void){return lsv_mode==2?time_us_32():0;}
void lsv_begin(uint32_t run,uint32_t frame,uint32_t bytes,uint32_t queued,
               uint32_t start_us,uint32_t export_us){
    if(lsv_mode!=2)return;
    ++lsv_transactions;
    if(used==LSV_CAPACITY || active){++lsv_dropped;return;}
    active=&records[(head+used)%LSV_CAPACITY];memset(active,0,sizeof *active);
    active->run=run;active->sequence=lsv_transactions;active->frame=frame;
    active->bytes=bytes;active->queued=queued;active->dropped=lsv_dropped;
    active->mask=1u<<LSV_EXPORT;active->phase[LSV_EXPORT].us=export_us;
    transaction_start=start_us;phase_index=LSV_PHASES;
}
void lsv_phase_begin(unsigned phase){
    if(!active || phase>=LSV_PHASES)return;
    phase_index=phase;active->mask|=1u<<phase;phase_start=time_us_32();
}
void lsv_phase_end(int32_t result){
    if(!active || phase_index>=LSV_PHASES)return;
    active->phase[phase_index].us=time_us_32()-phase_start;
    active->phase[phase_index].result=result;phase_index=LSV_PHASES;
}
uint32_t lsv_wait_begin(void){return active && phase_index<LSV_PHASES?time_us_32():0;}
void lsv_wait_end(unsigned kind,uint32_t start,bool failed){
    if(!active || phase_index>=LSV_PHASES || kind>=LSV_WAITS)return;
    uint32_t elapsed=time_us_32()-start;
    lsv_wait *w=&active->phase[phase_index].wait[kind];
    ++w->calls;w->us+=elapsed;if(elapsed>w->max_us)w->max_us=elapsed;
    if(failed)++w->failed;
}
void lsv_end(uint32_t result,uint32_t written){
    if(!active)return;
    active->total_us=time_us_32()-transaction_start;
    active->result=result;active->written=written;
#if R8_LYNX_SAVE_FEED
    active->feed=lfe_data;
#endif
    active=0;phase_index=LSV_PHASES;++used;
}
const lsv_record *lsv_pending(void){return used?&records[head]:0;}
void lsv_release(void){if(used){head=(head+1)%LSV_CAPACITY;--used;}}

#if R8_LYNX_PATH
void lsv_set_path(bool relative,uint32_t reason){
    if(active){active->path_relative=relative;active->path_reason=reason;}
}
void lsv_io_end(unsigned kind,uint32_t start,uint32_t requested,uint32_t completed,bool failed){
    if(!active || phase_index>=LSV_PHASES || kind>=LSV_IOS)return;
    uint32_t elapsed=time_us_32()-start;
    lsv_io *io=&active->phase[phase_index].io[kind];
    ++io->calls;io->requested+=requested;io->completed+=completed;
    if(failed)++io->failed;
    io->us+=elapsed;if(elapsed>io->max_us)io->max_us=elapsed;
}
#endif
