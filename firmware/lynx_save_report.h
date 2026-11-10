/* SPDX-License-Identifier: BSD-3-Clause
 * One bounded line after the transaction, only at the existing empty-TX sparse
 * report gate. Failed queue admission keeps the immutable record/cursor.
 */
#ifndef LYNX_SAVE_REPORT_H
#define LYNX_SAVE_REPORT_H
#include "lynx_save_diag.h"
static unsigned lsv_row;
#if R8_LYNX_SAVE_FEED
static bool lfe_reported;
#endif
#if R8_LYNX_PATH
static unsigned lcp_detail;
#endif
static bool lsv_report_one(void){
    const lsv_record *r=lsv_pending();if(!r)return false;
    uint32_t drops=r8_tx_drops;
#if R8_LYNX_PATH
    if(lcp_detail){
        if(lcp_detail==LSV_PHASES+1){
            r8_profile_printf("LYNX_PATH_SAVE run=%lu seq=%lu relative=%lu reason=%lu\n",
                (unsigned long)r->run,(unsigned long)r->sequence,(unsigned long)r->path_relative,(unsigned long)r->path_reason);
        }else{
            unsigned p=lcp_detail-1;const lsv_io *a=&r->phase[p].io[0],*b=&r->phase[p].io[1];
            r8_profile_printf("LYNX_SD_STEP run=%lu seq=%lu p=%u rd_n=%lu rd_req=%lu rd_done=%lu rd_fail=%lu rd_us=%lu rd_max=%lu wr_n=%lu wr_req=%lu wr_done=%lu wr_fail=%lu wr_us=%lu wr_max=%lu\n",
                (unsigned long)r->run,(unsigned long)r->sequence,p,
                (unsigned long)a->calls,(unsigned long)a->requested,(unsigned long)a->completed,(unsigned long)a->failed,(unsigned long)a->us,(unsigned long)a->max_us,
                (unsigned long)b->calls,(unsigned long)b->requested,(unsigned long)b->completed,(unsigned long)b->failed,(unsigned long)b->us,(unsigned long)b->max_us);
        }
        if(drops==r8_tx_drops)lcp_detail=0;
        return true;
    }
#endif
    if(lsv_row==0){
        r8_profile_printf("LYNX_SAVE_BEGIN id=LSV025 run=%lu seq=%lu frame=%lu bytes=%lu queued=%lu written=%lu result=%lu total_us=%lu mask=%lu dropped=%lu\n",
            (unsigned long)r->run,(unsigned long)r->sequence,(unsigned long)r->frame,
            (unsigned long)r->bytes,(unsigned long)r->queued,(unsigned long)r->written,
            (unsigned long)r->result,(unsigned long)r->total_us,(unsigned long)r->mask,(unsigned long)r->dropped);
        if(drops==r8_tx_drops){lsv_row=1;
#if R8_LYNX_PATH
            lcp_detail=LSV_PHASES+1;
#endif
        }
    }else{
        unsigned p=lsv_row-1;while(p<LSV_PHASES && !(r->mask&(1u<<p)))++p;
        if(p<LSV_PHASES){
            const lsv_phase *s=&r->phase[p];
            r8_profile_printf("LYNX_SAVE_STEP run=%lu seq=%lu p=%u us=%lu rc=%ld ready_n=%lu ready_us=%lu ready_max=%lu ready_fail=%lu token_n=%lu token_us=%lu token_max=%lu token_fail=%lu cmd_n=%lu cmd_us=%lu cmd_max=%lu cmd_fail=%lu\n",
                (unsigned long)r->run,(unsigned long)r->sequence,p,(unsigned long)s->us,(long)s->result,
                (unsigned long)s->wait[0].calls,(unsigned long)s->wait[0].us,(unsigned long)s->wait[0].max_us,(unsigned long)s->wait[0].failed,
                (unsigned long)s->wait[1].calls,(unsigned long)s->wait[1].us,(unsigned long)s->wait[1].max_us,(unsigned long)s->wait[1].failed,
                (unsigned long)s->wait[2].calls,(unsigned long)s->wait[2].us,(unsigned long)s->wait[2].max_us,(unsigned long)s->wait[2].failed);
            if(drops==r8_tx_drops){lsv_row=p+2;
#if R8_LYNX_PATH
                lcp_detail=p+1;
#endif
            }
        }else{
#if R8_LYNX_SAVE_FEED
            if(!lfe_reported){
                const lfe_stats *a=&r->feed;
                r8_profile_printf("LYNX_FEED id=LFE030 run=%lu seq=%lu mode=%lu boundaries=%lu pumps=%lu us=%lu max_gap_us=%lu q0=%lu q1=%lu di0=%lu di1=%lu u0=%lu u1=%lu z0=%lu z1=%lu empty0=%lu empty1=%lu limited0=%lu limited1=%lu\n",
                    (unsigned long)r->run,(unsigned long)r->sequence,(unsigned long)lfe_mode,
                    (unsigned long)a->boundaries,(unsigned long)a->pumps,(unsigned long)a->service_us,(unsigned long)a->max_gap_us,
                    (unsigned long)a->q0,(unsigned long)a->q1,(unsigned long)a->di0,(unsigned long)a->di1,
                    (unsigned long)a->u0,(unsigned long)a->u1,(unsigned long)a->z0,(unsigned long)a->z1,
                    (unsigned long)a->empty0,(unsigned long)a->empty1,(unsigned long)a->limited0,(unsigned long)a->limited1);
                if(drops==r8_tx_drops)lfe_reported=true;
                return true;
            }
#endif
            r8_profile_printf("LYNX_SAVE_END run=%lu seq=%lu transactions=%lu dropped=%lu\n",(unsigned long)r->run,(unsigned long)r->sequence,(unsigned long)lsv_transactions,(unsigned long)lsv_dropped);
            if(drops==r8_tx_drops){lsv_release();lsv_row=0;
#if R8_LYNX_SAVE_FEED
                lfe_reported=false;
#endif
            }
        }
    }
    return true;
}
#endif
