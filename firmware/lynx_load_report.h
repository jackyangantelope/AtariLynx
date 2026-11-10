/* SPDX-License-Identifier: BSD-3-Clause
 * One bounded line per empty shared UART queue, after load/first submission.
 * Keep the cursor immutable if queue admission failed.
 */
#ifndef LYNX_LOAD_REPORT_H
#define LYNX_LOAD_REPORT_H
#include "lynx_load_diag.h"
#if R8_LYNX_LOAD_DIAG
#if R8_LYNX_BOOT_CPU
#define LLD_CPU_END (LLD_FILES+4+LBC_BINS)
#if R8_LYNX_SCAN
#if R8_LYNX_WIDE
#define LLD_LAST_LINE (LLD_CPU_END+2)
#else
#define LLD_LAST_LINE (LLD_CPU_END+1)
#endif
#else
#define LLD_LAST_LINE LLD_CPU_END
#endif
#elif R8_LYNX_BOOT_PCM
#define LLD_LAST_LINE (LLD_FILES+3)
#else
#define LLD_LAST_LINE (LLD_FILES+2)
#endif
#if R8_LYNX_ROMCLK
#undef LLD_LAST_LINE
#define LLD_LAST_LINE (LLD_CPU_END+4)
#endif
static unsigned lld_report_cursor;
static bool lld_report_one(void){
    const lld_record *r=lld_pending();if(!r)return false;
    uint32_t drops=r8_tx_drops;
    if(lld_report_cursor==0){
        r8_profile_printf("LYNX_LOAD_BEGIN id=LLD028 seq=%lu ok=%lu total_us=%lu core_us=%lu path_us=%lu first_us=%lu present=%lu frame=%lu source=%lu rom_crc=%08lx save_crc=%08lx dropped=%lu\n",
            (unsigned long)r->sequence,(unsigned long)r->ok,(unsigned long)r->total_us,
            (unsigned long)r->core_us,(unsigned long)r->path_us,(unsigned long)r->first_us,
            (unsigned long)r->present,(unsigned long)r->first_frame,(unsigned long)r->save_source,
            (unsigned long)r->rom_crc,(unsigned long)r->save_crc,(unsigned long)lld_dropped);
    }else if(lld_report_cursor<=LLD_FILES){
        unsigned f=lld_report_cursor-1;const lld_io *i=&r->io[f];
        r8_profile_printf("LYNX_LOAD_IO seq=%lu file=%u called=%lu total_us=%lu open_us=%lu read_us=%lu close_us=%lu requested=%lu bytes=%lu open_rc=%lu read_rc=%lu close_rc=%lu rejected=%lu\n",
            (unsigned long)r->sequence,f,(unsigned long)i->called,(unsigned long)i->total_us,
            (unsigned long)i->open_us,(unsigned long)i->read_us,(unsigned long)i->close_us,
            (unsigned long)i->requested,(unsigned long)i->bytes,(unsigned long)i->open_rc,(unsigned long)i->read_rc,
            (unsigned long)i->close_rc,(unsigned long)i->rejected);
    }else if(lld_report_cursor==LLD_FILES+1){
        r8_profile_printf("LYNX_LOAD_CORE seq=%lu mask=%lu validate_us=%lu rom_crc_us=%lu payload_crc_us=%lu save_crc_us=%lu clear_us=%lu construct_us=%lu bank_crc_us=%lu apply_us=%lu boot_us=%lu reset_us=%lu calls=%lu cycles=%lu eof=%lu halt=%lu\n",
            (unsigned long)r->sequence,(unsigned long)r->phase_mask,
            (unsigned long)r->phase_us[LLD_VALIDATE],(unsigned long)r->phase_us[LLD_ROM_CRC],
            (unsigned long)r->phase_us[LLD_PAYLOAD_CRC],(unsigned long)r->phase_us[LLD_SAVE_CRC],
            (unsigned long)r->phase_us[LLD_CLEAR],(unsigned long)r->phase_us[LLD_CONSTRUCT],
            (unsigned long)r->phase_us[LLD_BANK_CRC],(unsigned long)r->phase_us[LLD_APPLY],
            (unsigned long)r->phase_us[LLD_BOOT],(unsigned long)r->phase_us[LLD_RESET],
            (unsigned long)r->boot_calls,(unsigned long)r->boot_cycles,
            (unsigned long)r->boot_eof,(unsigned long)r->boot_halt);
    }
#if R8_LYNX_BOOT_PCM
    else if(lld_report_cursor==LLD_FILES+2){
        const lbp_stats *a=&r->audio;
        r8_profile_printf("LYNX_BOOT_PCM id=LBP029 seq=%lu mode=%lu sound_us=%llu calls=%lu raw_long=%llu max_raw=%lu max_us=%lu batched=%lu fallback=%lu raw=%lu out=%lu phase=%lu prev=%08lx q0=%lu q1=%lu enq=%lu drop=%lu\n",
            (unsigned long)r->sequence,(unsigned long)lbp_mode,(unsigned long long)a->sound_us,
            (unsigned long)a->calls,(unsigned long long)a->raw_long,(unsigned long)a->max_raw,
            (unsigned long)a->max_us,(unsigned long)a->batched,(unsigned long)a->fallback,
            (unsigned long)a->raw_total,(unsigned long)a->converted_total,(unsigned long)a->phase,
            (unsigned long)a->previous,(unsigned long)a->queued_before,(unsigned long)a->queued_after,
            (unsigned long)a->enqueued,(unsigned long)a->dropped);
    }
#endif
#if R8_LYNX_BOOT_CPU
    else if(lld_report_cursor==LLD_FILES+3){
        const lbc_stats *c=&r->cpu;
        r8_profile_printf("LYNX_BOOT_CPU id=LBC031 seq=%lu mode=%lu calls=%lu short_hits=%lu wait_batch=0 bins=%u\n",
            (unsigned long)r->sequence,(unsigned long)c->mode,(unsigned long)c->calls,
            (unsigned long)c->short_hits,LBC_BINS);
    }else if(lld_report_cursor>=LLD_FILES+4 && lld_report_cursor<LLD_CPU_END){
        unsigned i=lld_report_cursor-(LLD_FILES+4);const lbc_bin *b=&r->cpu.bin[i];
        r8_profile_printf("LYNX_BOOT_CPU_BIN seq=%lu bin=%u calls=%lu us=%lu max_us=%lu cycles=%lu short_hits=%lu\n",
            (unsigned long)r->sequence,i,(unsigned long)b->calls,(unsigned long)b->us,
            (unsigned long)b->max_us,(unsigned long)b->cycles,(unsigned long)b->short_hits);
    }
#endif
#if R8_LYNX_SCAN
    else if(lld_report_cursor==LLD_CPU_END){
        const lcs_stats *s=&r->scan;
        r8_profile_printf("LYNX_BOOT_SCAN id=LCS032 seq=%lu mode=%lu probes=%lu batches=%lu bytes=%lu cycles=%lu guard=%lu boundary=%lu\n",
            (unsigned long)r->sequence,(unsigned long)s->mode,(unsigned long)s->probes,
            (unsigned long)s->batches,(unsigned long)s->bytes,(unsigned long)s->cycles,
            (unsigned long)s->guard,(unsigned long)s->boundary);
    }
#endif
#if R8_LYNX_WIDE
    else if(lld_report_cursor==LLD_CPU_END+1){
        const lwd_stats *w=&r->wide;
        r8_profile_printf("LYNX_BOOT_WIDE id=LWD034 seq=%lu mode=%lu entries=%lu accepted=%lu bytes=%lu groups=%lu max_bytes=%lu cycles=%lu\n",
            (unsigned long)r->sequence,(unsigned long)w->mode,(unsigned long)w->entries,
            (unsigned long)w->accepted,(unsigned long)w->bytes,(unsigned long)w->groups,
            (unsigned long)w->max_bytes,(unsigned long)w->cycles);
    }
#endif
#if R8_LYNX_ROMCLK
    else if(lld_report_cursor==LLD_CPU_END+2){
        const lrc_stats *s=lld_rom_clock_get();
        r8_profile_printf("LYNX_ROMCLK id=LRC038 seq=%lu mode=%lu reason=%lu hw=%lu peri=%lu req=%lu before=%lu read=%lu after=%lu restored=%lu scope_us=%lu enter_us=%lu exit_us=%lu\n",
            (unsigned long)r->sequence,(unsigned long)s->mode,(unsigned long)s->reason,(unsigned long)s->hardware,
            (unsigned long)s->peri_hz,(unsigned long)s->requested_hz,(unsigned long)s->before_hz,
            (unsigned long)s->read_hz,(unsigned long)s->after_hz,(unsigned long)s->restored,
            (unsigned long)s->scope_us,(unsigned long)s->enter_us,(unsigned long)s->exit_us);
    }else if(lld_report_cursor==LLD_CPU_END+3){
        const lrc_stats *s=lld_rom_clock_get();
        r8_profile_printf("LYNX_ROMCLK_REG seq=%lu cr0_before=%08lx cpsr_before=%lu cr0_read=%08lx cpsr_read=%lu cr1_before=%08lx cfg_before=%lu cfg_after=%lu\n",
            (unsigned long)r->sequence,(unsigned long)s->cr0_before,(unsigned long)s->cpsr_before,
            (unsigned long)s->cr0_read,(unsigned long)s->cpsr_read,(unsigned long)s->cr1_before,
            (unsigned long)s->cfg_before,(unsigned long)s->cfg_after);
    }
#endif
    else{
        r8_profile_printf("LYNX_LOAD_END id=LLD028 seq=%lu dropped=%lu\n",(unsigned long)r->sequence,(unsigned long)lld_dropped);
    }
    if(drops==r8_tx_drops){
        if(lld_report_cursor==LLD_LAST_LINE){lld_release();lld_report_cursor=0;}
        else ++lld_report_cursor;
    }
    return true;
}
#endif
#endif
