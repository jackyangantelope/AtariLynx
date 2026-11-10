/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#if R8_LYNX_HOT_CPU || R8_LYNX_FAST_LFSR
#include <unistd.h>
extern char __HeapLimit;
#endif
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/psram.h"
#include "hardware/sync.h"
#include "ff.h"
#include "tf_card.h"
#include "hstx.h"
#include "hdmi_diag.h"
#include "lynx_diag.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/dma.h"
#include "retro8.h"
#include "jack_input_devices.h"
#include "jack_tinyusb_input.h"
#include "jack_nespad_input.h"
#include "font_8x8.h"
#if R8_LYNX_REPORT
#define R8_PROFILE_TX_CAPACITY 5376
#elif R8_LYNX_ADMIT
#define R8_PROFILE_TX_CAPACITY 4864
#elif R8_LYNX_SCHED
#define R8_PROFILE_TX_CAPACITY 4608
#elif R8_LYNX_RUN
#define R8_PROFILE_TX_CAPACITY 4352
#else
#define R8_PROFILE_TX_CAPACITY 4096
#endif
#if R8_QUEUE_DIAG
#define R8_PROFILE_TX_CAPACITY 3072
#endif
#include "retro8_profile.h"
#include "lynx_load_report.h"
#include "lynx_gap_report.h"
#include "lynx_crc.h"
#include "lynx_crcmerge.h"
#include "lynx_wide.h"
#if R8_LYNX_BOOTMEM
#include "lynx_bootmem.h"
#include <malloc.h>
#endif
#if R8_LYNX_SAVE_DIAG
#include "lynx_save_report.h"
#if R8_LYNX_PATH
#include "lynx_save_path.h"
#endif
#endif
#include "lynx_short_platform.h"
#include "lynx_sprite_platform.h"
#include "lynx_cost_platform.h"
#include "lynx_fd.h"
#include "lynx_encode.h"
#include "lynx_run.h"
#include "lynx_bound.h"
#include "lynx_hash.h"
#include "lynx_render.h"
#if R8_LYNX_REPORT
#include "lynx_report_gate.h"
#endif
#if R8_LYNX_ADMIT
#include "lynx_admit.h"
#endif
#if R8_LYNX_SCHED
#include "lynx_sched.h"
static uint64_t lp_next;
static uint32_t lp_clock;
#endif
#if R8_LYNX_UNDER
#include "lynx_under.h"
#include "lynx_under_report.h"
extern uint32_t gSystemCycleCount;
#define LU_STAGE(s) lu_stage_set(s)
#else
#define LU_STAGE(s) ((void)0)
#endif
#if R8_QUEUE_DIAG
#include "retro8_queue_diag.h"
static r8_queue_diag queue_diag;
#endif
#if R8_DEFERRED_PRESENT
#include "retro8_present.h"
#endif
#if R8_ZX_SPLIT_PROFILE
uint32_t r8_zx_profile_now(void){return time_us_32();}
#endif
static FATFS fs;
static jack_input_queue queue;
static jack_input_devices devices;
#if R8_LYNX_BOOTMEM

#define names (*(char (*)[48][96])0x11408000u)
#else
static char names[48][96];
#endif
static int16_t samples[4096][2];
static unsigned rd,wr,count,names_count,selected;
static bool usb_ok,pad_ok,playing,buffering,memory_ok;
static uint64_t audio_next;
static uint32_t under,over,resync,slow,frames,max_frame_us;

static uint32_t source_enqueued,source_played,source_zero;
static uint32_t probe_render_us,q_observed_min=127,q_observed_max;
static uint32_t hdmi_frames_before,hdmi_irqs_before;
static uint64_t hdmi_report_before;
#if R8_LYNX_SAVE_FEED
#if !R8_PROFILE || R8_QUEUE_DIAG
#error LFE030 requires existing profile clocks and no legacy queue observer
#endif
static void lfe_begin(void);
static void lfe_boundary(void);
static void lfe_finish(void);
#else
#define lfe_begin() ((void)0)
#define lfe_boundary() ((void)0)
#define lfe_finish() ((void)0)
#endif
static void lynx_report(void){
    lynx_diag_stats d;lynx_diag_get(&d);
    uint64_t now=time_us_64();uint32_t vf=hstx_getframecounter(),irq=hstx_diag_irq_count();
    r8_profile_printf("LYNX_VIDEO frame=%lu sample_frame=%lu samples=%lu raw_fnv=%08lx rgb_fnv=%08lx raw_nz=%lu rgb_nz=%lu rot=%u pending_rot=%u probe_render_us=%lu\n",
        (unsigned long)d.frame,(unsigned long)d.video_frame,(unsigned long)d.video_samples,
        (unsigned long)d.raw_hash,(unsigned long)d.rgb_hash,(unsigned long)d.raw_nonzero,(unsigned long)d.rgb_nonzero,
        d.rotation,d.pending_rotation,(unsigned long)probe_render_us);
    r8_profile_printf("LYNX_PCM detail=0 raw_n=%lu out_n=%lu\n",(unsigned long)d.raw.count,(unsigned long)d.converted.count);
    r8_profile_printf("LYNX_SOURCE enqueued_total=%lu played_total=%lu zero_total=%lu queued=%u buffering=%u under=%lu over=%lu\n",
        (unsigned long)source_enqueued,(unsigned long)source_played,(unsigned long)source_zero,count,buffering,(unsigned long)under,(unsigned long)over);
    r8_profile_printf("LYNX_HDMI playing=%u interval_us=%llu frames=%lu irqs=%lu resync_total=%lu stall_total=%lu rate_total=%lu di_level=%lu q_obs_min=%lu q_obs_max=%lu di_under_total=%lu di_pushed_total=%lu di_used_total=%lu di_full_total=%lu di_limited_total=%lu fifo_stat=%08lx hstx_csr=%08lx dma0=%08lx dma1=%08lx sys_hz=%lu hstx_hz=%lu\n",
        playing,(unsigned long long)(now-hdmi_report_before),(unsigned long)(vf-hdmi_frames_before),(unsigned long)(irq-hdmi_irqs_before),
        (unsigned long)get_video_output_resync_count(),(unsigned long)hstx_diag_stalls(),(unsigned long)hstx_diag_overrate(),
        (unsigned long)hstx_di_queue_get_level(),(unsigned long)q_observed_min,(unsigned long)q_observed_max,
        (unsigned long)hstx_di_queue_get_underrun_count(),(unsigned long)hstx_diag_pushed(),(unsigned long)hstx_diag_consumed(),
        (unsigned long)hstx_diag_full(),(unsigned long)hstx_diag_limited(),(unsigned long)hstx_fifo_hw->stat,
        (unsigned long)hstx_ctrl_hw->csr,(unsigned long)dma_hw->ch[0].ctrl_trig,(unsigned long)dma_hw->ch[1].ctrl_trig,
        (unsigned long)clock_get_hz(clk_sys),(unsigned long)clock_get_hz(clk_hstx));
    r8_profile_printf("LYNX_INPUT pad0=%04x pad1=%04x buttons=%04x keys=%02x,%02x,%02x,%02x,%02x,%02x modifiers=%02x tx_drop=%lu\n",
        devices.pads[0],devices.pads[1],d.buttons,devices.keyboard_keys[0],devices.keyboard_keys[1],devices.keyboard_keys[2],
        devices.keyboard_keys[3],devices.keyboard_keys[4],devices.keyboard_keys[5],devices.keyboard_modifier,(unsigned long)r8_tx_drops);
#if R8_LYNX_WAIT
    r8_profile_printf("LYNX_WAIT id=LW013 mode=%u p0=%lu b0=%lu n0=%lu c0=%llu p1=%lu b1=%lu n1=%lu c1=%llu f=%lu ih=%08lx sf=%lu rh=%08lx\n",lw_mode,
        (unsigned long)lw_bins[0].probes,(unsigned long)lw_bins[0].batches,(unsigned long)lw_bins[0].loops,(unsigned long long)lw_bins[0].cycles,
        (unsigned long)lw_bins[1].probes,(unsigned long)lw_bins[1].batches,(unsigned long)lw_bins[1].loops,(unsigned long long)lw_bins[1].cycles,
        (unsigned long)lw_frames,(unsigned long)lw_input_hash,(unsigned long)lw_sample_frame,(unsigned long)lw_raw_hash);
#endif
#if R8_LYNX_ENCODE
    r8_profile_printf("LYNX_ENC m=%u n=%lu s=%lu p=%lu e=%lu q=%lu x=%lu\n",le_mode,
        (unsigned long)le_packets,(unsigned long)le_samples,(unsigned long)le_pack_us,
        (unsigned long)le_encode_us,(unsigned long)le_queue_us,(unsigned long)le_max_us);
#endif
#if R8_LYNX_RUN
    r8_profile_printf("LYNX_RUN m=%u l=%lu e=%lu a=%lu r=%lu p=%lu c=%lu\n",lrn_mode,
        (unsigned long)lrn_lines,(unsigned long)lrn_eligible,(unsigned long)lrn_alias,
        (unsigned long)lrn_runs,(unsigned long)lrn_pixels,(unsigned long)(lrn_pixels*6u));
#endif
#if R8_LYNX_BOUND
    r8_profile_printf("LYNX_BOUND m=%u d=%lu a=%lu n=%lu s=%lu\n",lb_mode,
        (unsigned long)lb_devices,(unsigned long)lb_accepted,(unsigned long)lb_loops,(unsigned long)lb_short);
#endif
#if R8_LYNX_HASH_SWITCH
    r8_profile_printf("LYNX_HASH m=%u v=%lu r=%lu\n",lh_mode,(unsigned long)lh_video_skipped,(unsigned long)lh_raw_skipped);
#endif
#if R8_LYNX_RENDER_PAIR
#if R8_LYNX_UNDER
    r8_profile_printf("LYNX_RENDER m=%u n=%lu ud=%u\n",lr_mode,(unsigned long)lr_calls,(unsigned)lu_mode);
#else
    r8_profile_printf("LYNX_RENDER m=%u n=%lu\n",lr_mode,(unsigned long)lr_calls);
#endif
#endif
#if R8_LYNX_SCHED
    r8_profile_printf("LYNX_SCHED m=%u f=%lu tries=%lu allow=%lu due=%lu slack=%lu source=%lu over_budget=%lu\n",
        (unsigned)lp_mode,(unsigned long)frames,(unsigned long)lp_stats.tries,
        (unsigned long)lp_stats.allow,(unsigned long)lp_stats.due,
        (unsigned long)lp_stats.slack,(unsigned long)lp_stats.source,
        (unsigned long)lp_stats.over_budget);
#endif
#if R8_LYNX_ADMIT
    r8_profile_printf("LYNX_ADMIT m=%u f=%lu pending=%u first=%lu latest=%lu age=%lu max_age=%lu gap=%lu max_gap=%lu\n",
        (unsigned)la_mode,(unsigned long)frames,(unsigned)la_stats.pending,
        (unsigned long)la_stats.first,(unsigned long)la_stats.latest,
        (unsigned long)la_age(frames),(unsigned long)la_stats.max_age,
        (unsigned long)la_gap(frames),(unsigned long)la_stats.max_gap);
#endif
#if R8_LYNX_REPORT
    r8_profile_printf("LYNX_REPORT m=%u f=%lu tries=%lu allow=%lu source=%lu tx=%lu bypass=%lu pending=%u wait_us=%llu max_wait_us=%llu max_cost_us=%llu over_budget=%lu\n",
        (unsigned)lq_mode,(unsigned long)frames,(unsigned long)lq_stats.tries,
        (unsigned long)lq_stats.allow,(unsigned long)lq_stats.source,(unsigned long)lq_stats.tx,
        (unsigned long)lq_stats.bypass,(unsigned)lq_stats.pending,
        (unsigned long long)lq_stats.wait_us,(unsigned long long)lq_stats.max_wait_us,
        (unsigned long long)lq_stats.max_cost_us,(unsigned long)lq_stats.over_budget);
#endif
#if R8_LYNX_SAVE_FEED
    lgap_report();
#endif
    hdmi_frames_before=vf;hdmi_irqs_before=irq;hdmi_report_before=now;
    q_observed_min=127;q_observed_max=0;lynx_diag_interval_reset();lynx_diag_request_video();
}
#if N3_SAVE
static uint8_t saved_bytes[2048],save_scratch[2048];
static size_t saved_size;
static bool save_loaded;
static char save_path[160];
#if R8_LYNX_SAVE_DIAG
static void flush_save(void){
    if(!save_loaded)return;
    uint32_t save_begin=lsv_clock();
    size_t n=r8_export_save(save_scratch,sizeof save_scratch);
    uint32_t export_us=lsv_clock()-save_begin;
    if(!n||(n==saved_size&&!memcmp(save_scratch,saved_bytes,n)))return;
    lsv_begin(lu_get_status()->run,frames,n,count,save_begin,export_us);
#if R8_LYNX_UNDER
    uint32_t save_stage=lu_stage;LU_STAGE(LU_SAVE);
#endif
    lfe_begin();
    lsv_phase_begin(LSV_PREP);
#if R8_LYNX_PATH
    const char *save_target=lcp_save_name(&fs,save_path);
    lsv_set_path(save_target!=save_path,lcp_reason);
#else
    const char *save_target=save_path;
#endif
    char temporary[168],backup[168];
    snprintf(temporary,sizeof temporary,"%s.tmp",save_target);
    snprintf(backup,sizeof backup,"%s.bak",save_target);
    lsv_phase_end(0);
    lfe_boundary();
    FIL f;UINT written=0;
    lsv_phase_begin(LSV_OPEN);
    FRESULT result=f_open(&f,temporary,FA_WRITE|FA_CREATE_ALWAYS);
    lsv_phase_end(result);lfe_boundary();
    if(result==FR_OK){
        lsv_phase_begin(LSV_WRITE);
        result=f_write(&f,save_scratch,n,&written);
        lsv_phase_end(result);lfe_boundary();
        if(result==FR_OK&&written!=n)result=FR_DISK_ERR;
        if(result==FR_OK){
            lsv_phase_begin(LSV_SYNC);result=f_sync(&f);lsv_phase_end(result);lfe_boundary();
        }
        lsv_phase_begin(LSV_CLOSE);
        FRESULT closed=f_close(&f);lsv_phase_end(closed);lfe_boundary();
        if(result==FR_OK)result=closed;
    }
    if(result==FR_OK){
        FILINFO info;
        lsv_phase_begin(LSV_STAT);
        FRESULT stat_result=f_stat(save_target,&info);lsv_phase_end(stat_result);lfe_boundary();
        bool existed=stat_result==FR_OK;
        if(existed){
            lsv_phase_begin(LSV_UNLINK);
            FRESULT removed=f_unlink(backup);lsv_phase_end(removed);lfe_boundary();
            lsv_phase_begin(LSV_BACKUP);
            result=f_rename(save_target,backup);lsv_phase_end(result);lfe_boundary();
        }
        if(result==FR_OK){
            lsv_phase_begin(LSV_COMMIT);
            result=f_rename(temporary,save_target);lsv_phase_end(result);lfe_boundary();
            if(result!=FR_OK&&existed){
                lsv_phase_begin(LSV_RESTORE);
                FRESULT restored=f_rename(backup,save_target);lsv_phase_end(restored);lfe_boundary();
            }
        }
    }
    if(result==FR_OK){
        lsv_phase_begin(LSV_CACHE);
        memcpy(saved_bytes,save_scratch,n);saved_size=n;
        lsv_phase_end(0);
    }
    lsv_phase_begin(LSV_LOG);
    r8_profile_printf("N3_SAVE bytes=%lu result=%u\n",(unsigned long)n,result);
    lsv_phase_end(0);lfe_finish();lsv_end(result,written);
#if R8_LYNX_UNDER
    LU_STAGE(save_stage);
#endif
}
#else
static void flush_save(void){
    if(!save_loaded)return;
    size_t n=r8_export_save(save_scratch,sizeof save_scratch);
    if(!n||(n==saved_size&&!memcmp(save_scratch,saved_bytes,n)))return;
#if R8_LYNX_UNDER
    uint32_t save_stage=lu_stage;LU_STAGE(LU_SAVE);
#endif
    char temporary[168],backup[168];
    snprintf(temporary,sizeof temporary,"%s.tmp",save_path);
    snprintf(backup,sizeof backup,"%s.bak",save_path);
    FIL f;UINT written=0;FRESULT result=f_open(&f,temporary,FA_WRITE|FA_CREATE_ALWAYS);
    if(result==FR_OK){
        result=f_write(&f,save_scratch,n,&written);
        if(result==FR_OK&&written!=n)result=FR_DISK_ERR;
        if(result==FR_OK)result=f_sync(&f);
        FRESULT closed=f_close(&f);if(result==FR_OK)result=closed;
    }
    if(result==FR_OK){
        FILINFO info;bool existed=f_stat(save_path,&info)==FR_OK;
        if(existed){(void)f_unlink(backup);result=f_rename(save_path,backup);}
        if(result==FR_OK){
            result=f_rename(temporary,save_path);
            if(result!=FR_OK&&existed)(void)f_rename(backup,save_path);
        }
    }
    if(result==FR_OK){memcpy(saved_bytes,save_scratch,n);saved_size=n;}
    r8_profile_printf("N3_SAVE bytes=%lu result=%u\n",(unsigned long)n,result);
#if R8_LYNX_UNDER
    LU_STAGE(save_stage);
#endif
}
#endif
#endif
#if R8_DEFERRED_PRESENT
static r8_present_state presentation;
static uint32_t last_frame_work_us,render_max_us;

static bool try_present(bool idle) {
    if(!presentation.pending)return false;
    if(!hstx_framebuffer_available()){
        if(!idle)presentation.blocked++;
        return false;
    }
    uint64_t begin=time_us_64();
#if R8_LYNX_SCHED
    if(!lp_allow(idle,begin,lp_next,lp_clock,audio_next,count))return false;
#endif
    bool probing=lynx_diag_wants_video();
#if R8_LYNX_UNDER
    uint32_t saved_stage=lu_stage;LU_STAGE(LU_RENDER);
#endif
    r8_render((uint16_t *)hstx_getframebuffer());
#if R8_LYNX_UNDER
    LU_STAGE(saved_stage);
#endif
    bool accepted=hstx_present_frame();
#if R8_LYNX_LOAD_DIAG
    if(accepted && lld_wait_present)lld_first_present(presentation.produced);
#endif
    uint32_t elapsed=(uint32_t)(time_us_64()-begin);
#if R8_LYNX_SCHED
    if(elapsed>LP_RENDER_US)++lp_stats.over_budget;
#endif
    if(probing)probe_render_us=elapsed;
    r8_present_finish(&presentation,accepted,idle);
#if R8_LYNX_ADMIT
    la_finish(la_stats.latest,accepted);
#endif
    r8_profile.render_us+=elapsed;
    if(accepted)r8_profile.shown++;
    if(elapsed>render_max_us)render_max_us=elapsed;
    if(idle){

        r8_profile.work_us+=elapsed;last_frame_work_us+=elapsed;
        if(last_frame_work_us>max_frame_us)max_frame_us=last_frame_work_us;
    }
    return true;
}
#endif

static uint8_t *const bulk=(uint8_t *)0x11000000u;
static uint8_t *const rom=(uint8_t *)0x11200000u;
static uint8_t *const bios=(uint8_t *)0x11400000u;
static bool key(unsigned k){for(unsigned i=0;i<6;i++)if(devices.keyboard_keys[i]==k)return true;return false;}
#if R8_LYNX_SAVE_FEED
static uint32_t lfe_last;
static void lfe_audio_at(uint64_t now){
#include "lynx_audio_consumer.inc"
}
static void lfe_begin(void){
    memset(&lfe_data,0,sizeof lfe_data);lfe_last=time_us_32();
    lfe_data.q0=count;lfe_data.di0=hstx_di_queue_get_level();
    lfe_data.u0=under;lfe_data.z0=source_zero;
    lfe_data.empty0=hstx_di_queue_get_underrun_count();lfe_data.limited0=hstx_diag_limited();
}
static void lfe_boundary(void){
    uint32_t start=time_us_32(),gap=start-lfe_last;
    ++lfe_data.boundaries;if(gap>lfe_data.max_gap_us)lfe_data.max_gap_us=gap;
    if(lfe_mode!=2 || !playing)return;
    uint32_t q=count,u=under,mask=lu_phase_mask;
    uint64_t audio=time_us_64();lfe_audio_at(audio*44100u);
    uint32_t end=time_us_32();++lfe_data.pumps;lfe_data.service_us+=end-start;
    r8_profile.audio_us+=end-start;r8_profile.service_us+=end-start;
    lgap_note(start,start,start,(uint32_t)audio,end,frames,gSystemCycleCount,mask,q,count,under-u,1);
    lfe_last=end;
}
static void lfe_finish(void){
    uint32_t gap=time_us_32()-lfe_last;if(gap>lfe_data.max_gap_us)lfe_data.max_gap_us=gap;
    lfe_data.q1=count;lfe_data.di1=hstx_di_queue_get_level();
    lfe_data.u1=under;lfe_data.z1=source_zero;
    lfe_data.empty1=hstx_di_queue_get_underrun_count();lfe_data.limited1=hstx_diag_limited();
}
#endif
static void service(void){
#if R8_PROFILE
    uint64_t begin=time_us_64(),mark=begin;
#endif
    if(usb_ok)jack_tinyusb_input_poll();
#if R8_PROFILE
    uint64_t stamp=time_us_64();r8_profile.usb_us+=stamp-mark;mark=stamp;
#if R8_LYNX_SAVE_FEED
    uint64_t feed_usb=stamp;
#endif
#endif
#if R8_ASYNC_PAD
    if(pad_ok)(void)jack_nespad_input_service(time_us_32());
#else
    if(pad_ok)(void)jack_nespad_input_poll();
#endif
#if R8_PROFILE
    stamp=time_us_64();r8_profile.pad_us+=stamp-mark;
#if R8_LYNX_SAVE_FEED
    uint64_t feed_pad=stamp;
#endif
#endif
    jack_input_event e;while(jack_input_pop(&queue,&e)){}
#if R8_PROFILE
    mark=time_us_64();
#endif
#if R8_QUEUE_DIAG
    uint64_t audio_stamp=time_us_64();
    r8_queue_service_begin(&queue_diag,audio_stamp);
    uint64_t now=audio_stamp*44100u;
#elif R8_LYNX_SAVE_FEED
    uint64_t audio_stamp=time_us_64(),now=audio_stamp*44100u;
#else
    uint64_t now=time_us_64()*44100u;
#endif
#if R8_LYNX_SAVE_FEED
    uint32_t feed_q=count,feed_u=under,feed_mask=lu_phase_mask;
    lfe_audio_at(now);
#else
#if R8_LYNX_UNDER
    if(lu_enabled()) {
        lu_snapshot before={now,audio_next,frames,
            {source_enqueued,source_played,source_zero,under,resync,gSystemCycleCount,
             (uint16_t)count,playing,buffering}};
        lu_service_begin(&before);
    }
#endif
    if(now>audio_next+1000000ull*2048){audio_next=now;resync++;}
#if R8_QUEUE_DIAG
    uint64_t due_start=audio_next;
    unsigned queue_before=count;
#endif
    if(buffering&&count>=1764){
        buffering=false;
#if R8_QUEUE_DIAG
        if(queue_diag.active)queue_diag.w.leave++;
#endif
    }
    while(audio_next<=now){
        int l=0,r=0;
        if(playing&&!buffering){
            if(count){l=samples[rd][0];r=samples[rd][1];rd=(rd+1)&4095;count--;source_played++;}
            else{
                under++;buffering=true;source_zero++;
#if R8_QUEUE_DIAG
                queue_diag.w.enter++;
                if(!queue_diag.w.first_under.valid)
                    r8_queue_capture(&queue_diag,false,time_us_64(),frames,count,r8_pc(),buffering);
#endif
            }
        }else if(playing)source_zero++;
        hstx_push_audio_sample(l,r);audio_next+=1000000;
    }
#if R8_LYNX_UNDER
    if(lu_enabled()) {
        lu_counts after={source_enqueued,source_played,source_zero,under,resync,gSystemCycleCount,
                         (uint16_t)count,playing,buffering};
        lu_service_end(&after);
    }
#endif
    unsigned qlevel=hstx_di_queue_get_level();
    if(qlevel<q_observed_min)q_observed_min=qlevel;
    if(qlevel>q_observed_max)q_observed_max=qlevel;
#if R8_QUEUE_DIAG
    r8_queue_service_end(&queue_diag,queue_before,count,(uint32_t)((audio_next-due_start)/1000000u));
#endif
#endif
#if R8_PROFILE
    r8_profile.audio_us+=time_us_64()-mark;r8_profile_pump();
#if R8_LYNX_SAVE_FEED
    uint64_t feed_end=time_us_64();r8_profile.service_us+=feed_end-begin;
    if(playing)lgap_note((uint32_t)begin,(uint32_t)feed_usb,(uint32_t)feed_pad,(uint32_t)audio_stamp,
        (uint32_t)feed_end,frames,gSystemCycleCount,feed_mask,feed_q,count,under-feed_u,0);
    else lgap_valid=0;
#else
    r8_profile.service_us+=time_us_64()-begin;
#endif
    r8_profile.service_calls++;
#endif
}
static void pcm(int16_t l,int16_t r){
#if R8_QUEUE_DIAG
    queue_diag.w.offered++;
#endif
    if(count==4096){
        over++;
#if R8_QUEUE_DIAG
        queue_diag.w.dropped++;
        if(!queue_diag.w.first_over.valid)
            r8_queue_capture(&queue_diag,true,time_us_64(),frames,count,r8_pc(),buffering);
#endif
        return;
    }
    samples[wr][0]=l;samples[wr][1]=r;wr=(wr+1)&4095;count++;source_enqueued++;
#if R8_QUEUE_DIAG
    queue_diag.w.enqueued++;
    if(count>queue_diag.w.qmax)queue_diag.w.qmax=count;
#endif
}
#if R8_LYNX_BOOT_PCM

static bool pcm_repeat_ready(void){
#if R8_QUEUE_DIAG
    return false;
#else
    return !playing && count<=4096 && wr<4096 && rd<4096;
#endif
}
static void pcm_repeat(int16_t l,int16_t r,uint32_t n){
    uint32_t accepted=n<4096-count?n:4096-count;
    for(uint32_t i=0;i<accepted;i++)pcm(l,r);
    over+=n-accepted;
}
#endif
static void text(uint16_t *p,unsigned x,unsigned y,const char *s,uint16_t color){
    for(;*s&&x+8<=320;s++,x+=8){
        unsigned c=(unsigned char)*s;if(c<32||c>126)c='?';
        for(unsigned r=0;r<8;r++)for(unsigned b=0;b<8;b++)
            if(font_8x8[r*95+c-32]&(1u<<b))p[(y+r)*320+x+b]=color;
    }
}
static void menu(const char *message){
    if(!hstx_framebuffer_available())return;
    uint16_t *p=(uint16_t *)hstx_getframebuffer();memset(p,0,320*240*2);
    text(p,8,12,R8_TITLE " " R8_BUILD_VERSION,0x7fff);
    unsigned top=selected/9*9;
    for(unsigned n=top;n<names_count&&n<top+9;n++){
        text(p,8,36+(n-top)*18,n==selected?">":" ",0x7fff);
        text(p,24,36+(n-top)*18,names[n],n==selected?0x7fe0:0x5294);
    }
    text(p,8,202,message,0x7fff);
    text(p,8,220,"ESC / SELECT+START: menu",0x5294);hstx_present_frame();
}
static FRESULT scan(void){
#if R8_LYNX_BOOTMEM
    if(!memory_ok){names_count=selected=0;return FR_NOT_READY;}
#endif
    DIR d;FILINFO f;FRESULT r=f_opendir(&d,R8_DIRECTORY);names_count=0;selected=0;
    if(r!=FR_OK)return r;
    while((r=f_readdir(&d,&f))==FR_OK&&f.fname[0]){
        const char *ext=strrchr(f.fname,'.');
        if(!(f.fattrib&AM_DIR)&&ext&&!strcasecmp(ext,R8_EXTENSION)&&strlen(f.fname)<96&&names_count<48)
            strcpy(names[names_count++],f.fname);
    }
    f_closedir(&d);
    for(unsigned i=1;i<names_count;i++){
        char tmp[96];strcpy(tmp,names[i]);unsigned j=i;
        while(j&&strcmp(names[j-1],tmp)>0){strcpy(names[j],names[j-1]);j--;}
        strcpy(names[j],tmp);
    }
    return r;
}
#if R8_LYNX_LOAD_DIAG
static size_t load_read_file(unsigned which,const char *path,uint8_t *dest,size_t capacity){
    lld_io io={0};io.called=1;uint32_t begin=lld_clock(),mark=begin;
    FIL f;UINT got=0;FRESULT opened=f_open(&f,path,FA_READ);
    io.open_us=lld_clock()-mark;io.open_rc=opened;
    if(opened!=FR_OK){io.total_us=lld_clock()-begin;lld_file(which,&io);return 0;}
    FSIZE_t len=f_size(&f);bool ok=len>0&&len<=capacity;
    if(ok){
        io.requested=(uint32_t)len;mark=lld_clock();
        FRESULT result=f_read(&f,dest,(UINT)len,&got);
        io.read_us=lld_clock()-mark;io.read_rc=result;io.bytes=got;
        ok=result==FR_OK&&got==len;
    }else io.rejected=1;
    mark=lld_clock();FRESULT closed=f_close(&f);
    io.close_us=lld_clock()-mark;io.close_rc=closed;
    io.total_us=lld_clock()-begin;lld_file(which,&io);

    return ok?(size_t)len:0;
}
#else
static size_t read_file(const char *path,uint8_t *dest,size_t capacity){
    FIL f;UINT got;if(f_open(&f,path,FA_READ)!=FR_OK)return 0;
    FSIZE_t len=f_size(&f);bool ok=len>0&&len<=capacity;
    if(ok)ok=f_read(&f,dest,(UINT)len,&got)==FR_OK&&got==len;
    f_close(&f);return ok?(size_t)len:0;
}
#define load_read_file(which,path,dest,capacity) read_file(path,dest,capacity)
#endif
#if R8_LYNX_ROMCLK

static __attribute__((noinline)) size_t load_read_rom(const char *path,uint8_t *dest,size_t capacity){
    lrc_scope scope;uint32_t start=lld_clock();

    uint32_t mode=lrc_selector;bool dirty=fs.wflag!=0||(fs.fsi_flag&1u)!=0;
    pico_fatfs_romclk_begin(&scope,dirty?1:mode);
    scope.stats.mode=mode;if(dirty&&mode==2)scope.stats.reason=LRC_DIRTY_FS;
    uint32_t entered=lld_clock();
    size_t result=load_read_file(LLD_ROM,path,dest,capacity);
    uint32_t leaving=lld_clock();
    bool restored=pico_fatfs_romclk_end(&scope);
    uint32_t finish=lld_clock();
    scope.stats.scope_us=finish-start;scope.stats.enter_us=entered-start;scope.stats.exit_us=finish-leaving;
    lld_rom_clock(&scope.stats);
    return restored?result:0;
}
#else
#define load_read_rom(path,dest,capacity) load_read_file(LLD_ROM,path,dest,capacity)
#endif
static bool load(void){
    lld_begin();
    char path[160];snprintf(path,sizeof path,R8_DIRECTORY "/%s",names[selected]);
    size_t size=load_read_rom(path,rom,1048640),bios_size=0;
    if(R8_BIOS[0])bios_size=load_read_file(LLD_BIOS,R8_DIRECTORY "/" R8_BIOS,bios,32768);
#if N3_SAVE
    snprintf(save_path,sizeof save_path,R8_DIRECTORY "/%s.sav",names[selected]);
    saved_size=load_read_file(LLD_SAVE,save_path,saved_bytes,sizeof saved_bytes);
    lld_save_source(saved_size?1:0);
    if(!saved_size){char backup[168];snprintf(backup,sizeof backup,"%s.bak",save_path);saved_size=load_read_file(LLD_BACKUP,backup,saved_bytes,sizeof saved_bytes);lld_save_source(saved_size?2:0);}
    r8_restore_save(saved_bytes,saved_size);
#endif
#if R8_LYNX_BOOT_PCM
    uint32_t queue_before=count,enqueued_before=source_enqueued,dropped_before=over;
#endif
    lld_core_begin();
    bool ok=r8_load(rom,size,bios,bios_size,names[selected]);
    lld_core_end();
#if R8_LYNX_BOOT_PCM
    lbp_source(queue_before,count,source_enqueued-enqueued_before,over-dropped_before);
#endif
#if N3_SAVE
    save_loaded=ok;
#if R8_LYNX_PATH
    if(ok){
        uint32_t before=time_us_32();
        FRESULT path_result=lcp_prepare(&fs,R8_DIRECTORY,save_path);
        lld_path(time_us_32()-before);
        r8_profile_printf("LYNX_PATH_READY id=LCP026 mode=%lu rc=%u reason=%lu us=%lu\n",
            (unsigned long)lcp_mode,path_result,(unsigned long)lcp_reason,(unsigned long)(time_us_32()-before));
#if R8_LYNX_FS
        r8_profile_printf("LYNX_FS_READY id=LFX027 mode=%lu fs_type=%u fs_name=%s mount=%u cdir=%lu depth=%u relative=%u reason=%lu rc=%u\n",
            (unsigned long)lfx_mode,fs.fs_type,lfx_fs_name(fs.fs_type),fs.id,(unsigned long)fs.cdir,
            lfx_depth(&fs),lcp_reason==LCP_RELATIVE,(unsigned long)lcp_reason,path_result);
#endif
    }
#endif
#endif
    r8_profile_printf("R8_LOAD file=%s bytes=%lu bios=%lu ok=%u error=%s\n",names[selected],(unsigned long)size,(unsigned long)bios_size,ok,r8_error());
    lynx_diag_stats identity;lynx_diag_get(&identity);
    r8_profile_printf("LYNX_ID rom_crc=%08lx payload_crc=%08lx bank0_crc=%08lx save_bytes=%lu save_crc=%08lx save_applied=%lu rotation=%u pending_rotation=%u\n",
        (unsigned long)identity.rom_crc,(unsigned long)identity.payload_crc,(unsigned long)identity.bank_crc,
        (unsigned long)identity.save_bytes,(unsigned long)identity.save_crc,(unsigned long)identity.save_applied,identity.rotation,identity.pending_rotation);
#if R8_LYNX_HOT_CPU || R8_LYNX_FAST_LFSR
    uintptr_t heap_now=(uintptr_t)sbrk(0),heap_limit=(uintptr_t)&__HeapLimit;
#if R8_LYNX_BOOTMEM
    struct mallinfo info=mallinfo();
    r8_profile_printf("LYNX_HEAP id=LHM035 tail=%lu arena=%lu used=%lu free=%lu top=%lu blocks=%lu names_psram=4608\n",
        (unsigned long)(heap_now<heap_limit?heap_limit-heap_now:0),
        (unsigned long)info.arena,(unsigned long)info.uordblks,(unsigned long)info.fordblks,
        (unsigned long)info.keepcost,(unsigned long)info.ordblks);
#endif
#if R8_LYNX_CPU_FLASH || R8_LYNX_HOT_MIKIE
    r8_profile_printf("LYNX_MEM heap_tail=%lu cpu_sram=%u lfsr_exact=%u mikie_sram=%u\n",(unsigned long)(heap_now<heap_limit?heap_limit-heap_now:0),R8_LYNX_HOT_CPU&&!R8_LYNX_CPU_FLASH,R8_LYNX_FAST_LFSR,R8_LYNX_HOT_MIKIE);
#else
    r8_profile_printf("LYNX_MEM heap_tail=%lu cpu_sram=%u lfsr_exact=%u\n",(unsigned long)(heap_now<heap_limit?heap_limit-heap_now:0),R8_LYNX_HOT_CPU,R8_LYNX_FAST_LFSR);
#endif
#endif
    lld_done(ok,ok?identity.rom_crc:0,ok?identity.save_crc:0);
    return ok;
}
int main(void){
#if R8_LYNX_SAVE_FEED
    lfe_reset();
#endif
#if R8_LYNX_UNDER
    lu_reset();
#endif
    vreg_set_voltage(VREG_VOLTAGE_1_30);sleep_ms(10);set_sys_clock_khz(252000,true);stdio_init_all();
    bool present=psram_is_available();
    int params=present?psram_configure_params(PICO_DEFAULT_PSRAM_MAX_FREQ,PICO_DEFAULT_PSRAM_MAX_SELECT,PICO_DEFAULT_PSRAM_MIN_DESELECT):PICO_ERROR_NOT_FOUND;
    uint32_t irq=save_and_disable_interrupts();
    int reinit=params==PICO_OK?psram_reinitialize():params;restore_interrupts(irq);
    memory_ok=present&&reinit==PICO_OK&&psram_get_size()>=8*1024*1024;
    pico_fatfs_spi_config_t cfg={SDCARD_SPI,CLK_SLOW_DEFAULT,CLK_FAST_DEFAULT_PIO,SDCARD_PIN_MISO,SDCARD_PIN_CS,SDCARD_PIN_SCK,SDCARD_PIN_MOSI,true};
    if(!pico_fatfs_set_config(&cfg))panic("SD pins");
    FRESULT sd=f_mount(&fs,"",1);hstx_init(false);
    audio_next=time_us_64()*44100u;
    jack_input_init(&queue);jack_input_devices_init(&devices,&queue);
    usb_ok=jack_tinyusb_input_init(&devices);pad_ok=jack_nespad_input_init(&devices,252000);
    if(memory_ok)memory_ok=r8_init(bulk,2*1024*1024,pcm,service);
#if R8_LYNX_BOOT_PCM
    lbp_set_sink(pcm_repeat_ready,pcm_repeat);
#endif
#if R8_LYNX_SHORT
#if R8_LYNX_HOTSET
#if R8_LYNX_SHORT_MISS == 63
    printf("LYNX_SHORT_CFG id=LS016 mode=%u epoch=240 opcodes=10,20,38,48,4c,60,65,68,80,85,86,8a,8d,91,92,98,a0,a2,a5,a8,a9,aa,ad,b0,b1,b2,bd,d0,e0,e6,e9,f0 miss_cooldown=63 retry_frames=31 min_hits=128 skip_cycles=0 original_mmio=1\n",R8_LYNX_SHORT_MODE);
#else
    printf("LYNX_SHORT_CFG id=LS016 mode=%u epoch=240 opcodes=10,20,38,48,4c,60,65,68,80,85,86,8a,8d,91,92,98,a0,a2,a5,a8,a9,aa,ad,b0,b1,b2,bd,d0,e0,e6,e9,f0 miss_cooldown=%u retry_frames=31 min_hits=128 skip_cycles=0 original_mmio=1\n",R8_LYNX_SHORT_MODE,R8_LYNX_SHORT_MISS);
#endif
    printf("LYNX_CPU_CFG id=LHC011 hotset=32 original_bodies=1 original_cycles=1 original_mmio=1\n");
#if R8_LYNX_SHORT_MISS != 63
#if R8_LYNX_WAIT
#if R8_LYNX_ENCODE
#if R8_LYNX_RUN
#if R8_LYNX_BOUND
#if R8_LYNX_HASH_SWITCH
#if R8_LYNX_RENDER_PAIR
    printf("LYNX_RENDER_CFG id=LRC018 fixed=%lu hash_fixed=2 scope=paired_rgb565_rgb555 original_pixels=1 original_counters=1 original_events=1 original_pcm=1\n",(unsigned long)lr_selector);
#endif
    printf("LYNX_HASH_CFG id=LPH017 fixed=%lu scope=video_and_wait_raw input_hash=1 bound_fixed=2 zero_fields=unavailable_when_on original_counters=1 original_events=1 original_pcm=1\n",(unsigned long)lh_selector);
#endif
    printf("LYNX_BOUND_CFG id=LWB016 fixed=%lu run_fixed=2 enc_fixed=2 fd_fixed=2 original_counters=1 original_events=1 original_pcm=1\n",(unsigned long)lb_selector);
#endif
    printf("LYNX_RUN_CFG id=LSR015 fixed=%lu enc_fixed=2 fd_fixed=2 scope=shadow_packed original_cycles=1 original_pcm=1\n",(unsigned long)lrn_selector);
#endif
    printf("LYNX_ENC_CFG id=LE014 fixed=%lu fd_fixed=2 sample_stride=257 observer_bin=1 original_pcm=1 original_packet=1\n",(unsigned long)le_selector);
#endif
    printf("LYNX_WAIT_CFG id=LW013 fixed=%lu boundary=event_free original_counters=1 original_events=1 original_pcm=1\n",(unsigned long)lw_selector);
#endif
    printf("LYNX_RETRY_CFG id=LR012 miss_cooldown=%u retry_frames=31 min_hits=128 skip_cycles=0 original_events=1 original_pcm=1\n",R8_LYNX_SHORT_MISS);
#endif
#else
    printf("LYNX_SHORT_CFG id=LS016 mode=%u epoch=240 opcodes=ad,d0 miss_cooldown=63 retry_frames=31 min_hits=128 skip_cycles=0 original_mmio=1\n",R8_LYNX_SHORT_MODE);
#endif
#endif
#if R8_LYNX_SPRITE
    lsp_calibrate();
#if R8_LYNX_COST
    lmc_calibrate();
#if R8_LYNX_FD_FAST
    printf("LYNX_COST_CFG id=LMC019 epoch=240 clock=us32 all_calls=1 origins=event,fd0a,other_timer nested=0 original_update=0 equivalent_update=1 fd0a_fast=1 original_cycles=1 cal_n=256 cal_sum_us=%lu cal_max_us=%lu\n",(unsigned long)lmc_cal_sum,(unsigned long)lmc_cal_max);
    printf("LYNX_FD_CFG id=LFD010 mode=fixed_on guard=timers_0_2_7_no_rollover original_audio=1 original_cycles=1 crlf=1\n");
#else
    printf("LYNX_COST_CFG id=LMC019 epoch=240 clock=us32 all_calls=1 origins=event,fd0a,other_timer nested=0 original_update=1 original_cycles=1 cal_n=256 cal_sum_us=%lu cal_max_us=%lu\n",(unsigned long)lmc_cal_sum,(unsigned long)lmc_cal_max);
#endif
#endif
#if R8_LYNX_LINECACHE
    printf("LYNX_SPRITE_CFG id=LSP017 epoch=240 all_calls=1 clock=us32 irq_included=1 short_mode=1 cal_n=256 cal_sum_us=%lu cal_max_us=%lu original_draw=0 line_cache=1 original_cycles=1\n",(unsigned long)lsp_cal_sum,(unsigned long)lsp_cal_max);
    printf("LYNX_LINE_CFG id=LL018 cache=1 scope=destination_line fields=8 original_cycles=1 original_ram_writes=1\n");
#else
    printf("LYNX_SPRITE_CFG id=LSP017 epoch=240 all_calls=1 clock=us32 irq_included=1 short_mode=1 cal_n=256 cal_sum_us=%lu cal_max_us=%lu original_draw=1 original_cycles=1\n",(unsigned long)lsp_cal_sum,(unsigned long)lsp_cal_max);
#endif
#endif
    FRESULT listing=sd==FR_OK?scan():sd;
    printf("R8_BOOT " R8_TITLE " " R8_BUILD_VERSION " FruitJam psram=%u size=%lu sd=%d count=%u usb=%u pad=%u\n",memory_ok,(unsigned long)psram_get_size(),listing,names_count,usb_ok,pad_ok);
    printf("LYNX_DIAG version=" R8_BUILD_VERSION " signed_resample=1 initialized_rotation=1 di_capacity=128 di_high=120 interval_ms=2000 hash=u16_fnv1a audio=original pcm_detail=0 render_lut=1 border_clear=1 hot_sram=1\n");
#if R8_LYNX_HOT_CPU || R8_LYNX_FAST_LFSR
#if R8_LYNX_CPU_FLASH || R8_LYNX_HOT_MIKIE
    printf("LYNX_EXEC cpu_sram=%u lfsr_exact=%u mikie_sram=%u original_clock=1 original_pcm=1\n",R8_LYNX_HOT_CPU&&!R8_LYNX_CPU_FLASH,R8_LYNX_FAST_LFSR,R8_LYNX_HOT_MIKIE);
#else
    printf("LYNX_EXEC cpu_sram=%u lfsr_exact=%u original_clock=1 original_pcm=1\n",R8_LYNX_HOT_CPU,R8_LYNX_FAST_LFSR);
#endif
#endif
    hdmi_report_before=time_us_64();hdmi_frames_before=hstx_getframecounter();hdmi_irqs_before=hstx_diag_irq_count();
#if R8_PROFILE || R8_SKIP_48K_AY || R8_ASYNC_PAD || R8_SRAM_HOT
    printf("R8_CFG variant=" R8_VARIANT " version=" R8_BUILD_VERSION " profile=%u async_pad=%u\n",R8_PROFILE,R8_ASYNC_PAD);
#if R8_LYNX_SAVE_FEED
    printf("LYNX_FEED_CFG id=LFE030 fixed=%lu baseline=LBP029_ON original_pcm=1 save_order=1 audio_only=1 gap_q=512 gap_us=1000\n",(unsigned long)lfe_selector);
#if R8_LYNX_BOOT_CPU
#if R8_LYNX_SCAN
    printf("LYNX_SCAN_CFG id=LCS032 fixed=%lu baseline=LBC031_ON scope=startup original_pcm=1 original_cycles=1 original_events=1\n",(unsigned long)lcs_selector);
#if R8_LYNX_CRC_TABLE
    printf("LYNX_CRC_CFG id=LCRC033 fixed=%lu baseline=LCS032_ON table=256 table_flash=1 polynomial=edb88320 original_pcm=1 original_cycles=1 original_events=1\n",(unsigned long)lcrc_selector);
#if R8_LYNX_WIDE
    printf("LYNX_WIDE_CFG id=LWD034 fixed=%lu baseline=LCRC033_ON scope=startup original_pcm=1 original_cycles=1 original_events=1 original_scan_counters=1\n",(unsigned long)lwd_selector);
#if R8_LYNX_BOOTMEM
#if R8_LYNX_ROMCLK
#if R8_LYNX_CRC_MERGE
    printf("LYNX_CRCMERGE_CFG id=LCM039 fixed=%lu baseline=LRC038_ON header=64 bank_scan=1 original_pcm=1\n",(unsigned long)lcm_selector);
#endif
    printf("LYNX_ROMCLK_CFG id=LRC038 fixed=%lu baseline=LBM035_ON req=24000000 bounce=0 exit_bench=0 original_pcm=1\n",(unsigned long)lrc_selector);
#endif
    printf("LYNX_BOOTMEM_CFG id=LBM035 fixed=%lu baseline=LWD034_ON names_psram=4608 exact_boot_math=1 original_pcm=1\n",(unsigned long)lbm_selector);
#endif
#endif
#endif
#endif
    printf("LYNX_BOOT_CPU_CFG id=LBC031 fixed=%lu baseline=LFE030_ON scope=startup wait_batch=0 bins=1,2-16,17-32,33-48,49-64 original_pcm=1 original_cycles=1 original_events=1\n",(unsigned long)lbc_selector);
#endif
#endif
#if R8_LYNX_BOOT_PCM
    printf("LYNX_BOOT_PCM_CFG id=LBP029 fixed=%lu baseline=LLD028_ON threshold=%u scope=startup_long_constant original_pcm=1 original_cycles=1 original_events=1 source_drops=exact\n",(unsigned long)lbp_selector,LBP_MIN_RAW);
#endif
#if R8_LYNX_LOAD_DIAG
    printf("LYNX_LOAD_CFG id=LLD028 fixed=2 baseline=LFX027_ON boundary_clock=us32 records=2 original_pcm=1 first=submit\n");
#endif
#if R8_DEFERRED_PRESENT
    printf("R8_PRESENT_CFG deferred=1 latest_only=1 blocking_wait=0\n");
#endif
#if R8_FUSED_TICK
    printf("R8_EXEC_CFG fused_tick=1 original_beeper=1\n");
#endif
#if R8_Z80_INLINE
    printf("R8_Z80_CFG inline_tick=1 all_states=1\n");
#if R8_HALT_BATCH
    printf("R8_HALT_CFG batch=1 event_ticks=original filter=exact\n");
#endif
#endif
#endif
#if R8_ZX_PACKED_RENDER
    printf("R8_RENDER_CFG packed=1 crop=320x240 pair_table=1024 original_palette=1\n");
#endif
#if R8_QUEUE_DIAG
    printf("R8_QUEUE_CFG diag=1 capacity=4096 refill=1764 uart_queue=3072 sample_changes=0\n");
#endif
#if R8_ZX_SPLIT_PROFILE
    printf("R8_ZX_CFG split=1 clock=us32 row_timing=1\n");
#endif
#if R8_BEEPER_STEADY
    printf("R8_BEEPER_CFG steady=1 window=128 exact_sum=1\n");
#endif
#if R8_LYNX_UNDER
    printf("LYNX_UNDER_CFG id=LU021 fixed=%u history=4 original_pcm=1 clock_reads=0\n",(unsigned)lu_mode);
#if !R8_LYNX_ADMIT
    lu_report_reset();
#endif
#endif
#if R8_LYNX_SCHED
    lp_reset();
    printf("LYNX_SCHED_CFG id=LP022 fixed=%lu ud=2 render_us=%u core_gap_us=%u idle_slack=1 post_slack=0 original_pcm=1\n",
        (unsigned long)lp_selector,LP_RENDER_US,LP_CORE_GAP_US);
#endif
#if R8_LYNX_ADMIT
    la_reset();
    printf("LYNX_ADMIT_CFG id=LA023 fixed=%lu lp=2 ud=2 render_us=2500 core_gap_us=5000 off=LP022_ON on=reserve_only age_unit=frames clock_reads=0 original_pcm=1\n",
        (unsigned long)la_selector);
#if R8_LYNX_REPORT
    lq_reset();
    printf("LYNX_REPORT_CFG id=LQ024 fixed=%lu la=2 lp=2 ud=2 report_us=3500 core_gap_us=5000 force=0 save_period_us=2000000 original_pcm=1\n",(unsigned long)lq_selector);
#endif
#if R8_LYNX_SAVE_DIAG
    lsv_reset();
    printf("LYNX_SAVE_CFG id=LSV025 fixed=%lu baseline=LQ024_ON phases=13 records=2 clock=us32 loop_body_unchanged=1 original_pcm=1 save_period_us=2000000\n",(unsigned long)lsv_selector);
#endif
#if R8_LYNX_PATH
    lcp_reset();
    printf("LYNX_PATH_CFG id=LCP026 fixed=%lu baseline=LSV025_ON sector_bytes=512 records=2 original_pcm=1 absolute_fallback=1\n",(unsigned long)lcp_selector);
#if R8_LYNX_FS
    printf("LYNX_FS_CFG id=LFX027 fixed=%lu baseline=LCP026_ON path_depth=%u original_pcm=1 save_order=1\n",(unsigned long)lfx_selector,FF_PATH_DEPTH);
    printf("LYNX_FS_MOUNT id=LFX027 fs_type=%u fs_name=%s mount=%u cdir=%lu depth=%u rc=%u\n",
        fs.fs_type,lfx_fs_name(fs.fs_type),fs.id,(unsigned long)fs.cdir,lfx_depth(&fs),sd);
#endif
#endif

    lu_report_reset();
#endif
    unsigned old=0;const char *message="UP/DOWN + ENTER/A: play";
    uint64_t clock=r8_clock(),step=(uint64_t)r8_frame_cycles()*1000000;
    uint64_t next=time_us_64()*clock,report=time_us_64()+2000000;
#if R8_LYNX_REPORT
    uint64_t save_report=report,lq_last_status=0,lq_last_full=report-2000000;
#endif
#if R8_PROFILE
    r8_profile_reset(time_us_64());
#endif
#if R8_LYNX_UNDER
    uint64_t lu_last_status=0;
#endif
    while(true){
        LU_STAGE(LU_IDLE);
        service();unsigned pad=devices.pads[0];
#if R8_DEFERRED_PRESENT
#if R8_LYNX_SCHED

        if(lp_mode==1 && playing&&try_present(true))service();
#else
        if(playing&&try_present(true))service();
#endif
#endif
        unsigned buttons=(key(0x52)||(pad&R8_UP)?1:0)|(key(0x51)||(pad&R8_DOWN)?2:0)|
            (key(0x28)||(pad&R8_A)?4:0)|(key(0x29)||((pad&(R8_SELECT|R8_START))==(R8_SELECT|R8_START))?8:0);
        unsigned edge=buttons&~old;old=buttons;
        if(playing&&(edge&8)){
            lld_cancel_present();
#if N3_SAVE
            flush_save();
#endif
#if R8_QUEUE_DIAG
            r8_queue_flush(&queue_diag,count);
#endif
            playing=false;rd=wr=count=0;message="UP/DOWN + ENTER/A: play";
#if R8_DEFERRED_PRESENT
            if(presentation.pending)presentation.replaced++;
            presentation.pending=false;
#if R8_LYNX_ADMIT
            la_cancel();
#endif
#endif
        }
        if(!playing){
            if(!memory_ok)message="PSRAM unavailable (8 MiB required)";
            else if(names_count){
                if(edge&1)selected=(selected+names_count-1)%names_count;
                if(edge&2)selected=(selected+1)%names_count;
                if(edge&4){
                    playing=load();message=playing?"":r8_error();rd=wr=count=0;buffering=playing;
                    audio_next=time_us_64()*44100u;next=time_us_64()*clock;
                    under=over=resync=slow=frames=max_frame_us=0;
                    source_enqueued=source_played=source_zero=0;
#if R8_LYNX_UNDER
                    lu_reset();
#if R8_LYNX_SAVE_FEED
                    lgap_reset();
#endif
                    lu_report_reset();
#if R8_LYNX_SCHED
                    lp_reset();
#endif
#endif
#if R8_QUEUE_DIAG
                    r8_queue_reset(&queue_diag,time_us_64(),count,playing,buffering);
#endif
#if R8_DEFERRED_PRESENT
                    memset(&presentation,0,sizeof presentation);
                    last_frame_work_us=render_max_us=0;
#if R8_LYNX_ADMIT
                    la_reset();
#endif
#endif
#if R8_PROFILE
                    r8_profile_reset(time_us_64());report=time_us_64()+2000000;
#if R8_LYNX_REPORT
                    lq_reset();save_report=report;lq_last_status=0;lq_last_full=report-2000000;
#endif
                    hdmi_report_before=time_us_64();hdmi_frames_before=hstx_getframecounter();hdmi_irqs_before=hstx_diag_irq_count();
                    lynx_diag_interval_reset();q_observed_min=127;q_observed_max=0;
#endif
                }
            }else message="No " R8_EXTENSION " files in " R8_DIRECTORY;
        }
        uint64_t now=time_us_64();
        if(now*clock>=next){
#if R8_QUEUE_DIAG
            r8_queue_due(&queue_diag,now,next,step,clock);
#endif
            next+=step;
            if(now*clock>next+step*2){
                next=now*clock;slow++;
#if R8_QUEUE_DIAG
                if(queue_diag.active)queue_diag.w.resets++;
#endif
            }
            if(playing){
#if R8_QUEUE_DIAG
                queue_diag.in_frame=true;
#endif
                r8_input state={0};memcpy(state.pads,devices.pads,sizeof state.pads);
                memcpy(state.keys,devices.keyboard_keys,sizeof state.keys);state.modifiers=devices.keyboard_modifier;
                r8_controls(&state);uint64_t begin=time_us_64();
#if R8_PROFILE
                uint64_t service_before=r8_profile.service_us;
#endif
                LU_STAGE(LU_CORE);
                r8_frame();
                LU_STAGE(LU_IDLE);
                if(r8_error()[0]){lld_cancel_present();playing=false;message=r8_error();rd=wr=count=0;}
                uint64_t new_step=(uint64_t)r8_frame_cycles()*1000000;
                next=next-step+new_step;step=new_step;
#if R8_PROFILE
#if R8_LYNX_SHORT
                uint32_t core_us=(uint32_t)(time_us_64()-begin-(r8_profile.service_us-service_before));
                r8_profile.emu_us+=core_us;ls_bins[ls_active].core_us+=core_us;
#if R8_LYNX_SPRITE
                lsp_bins[lsp_active].core_us+=core_us;
#if R8_LYNX_COST
        lmc_bins[lsp_active].core_us+=core_us;
#endif
#endif
#else
                r8_profile.emu_us+=time_us_64()-begin-(r8_profile.service_us-service_before);
#endif
#endif
                service();
#if R8_DEFERRED_PRESENT
#if R8_LYNX_SCHED
                lp_next=next;lp_clock=(uint32_t)clock;
#endif
#if R8_LYNX_ADMIT
                la_offer(frames+1,presentation.pending);
#endif
                r8_present_offer(&presentation);(void)try_present(false);
#else
                if(hstx_framebuffer_available()){
#if R8_PROFILE
                    uint64_t render_begin=time_us_64();
#endif
                    r8_render((uint16_t *)hstx_getframebuffer());hstx_present_frame();
#if R8_PROFILE
                    r8_profile.render_us+=time_us_64()-render_begin;r8_profile.shown++;
#endif
                }
#endif
                service();uint32_t elapsed=(uint32_t)(time_us_64()-begin);
#if R8_DEFERRED_PRESENT
                last_frame_work_us=elapsed;
#endif
                if(elapsed>max_frame_us)max_frame_us=elapsed;
                frames++;
#if R8_QUEUE_DIAG
                queue_diag.in_frame=false;
#endif
#if R8_PROFILE
                r8_profile.work_us+=elapsed;r8_profile.completed++;
#endif
            }else menu(message);
        }
#if R8_LYNX_SCHED
        else if(lp_mode==2 && playing){
            lp_next=next;lp_clock=(uint32_t)clock;
            if(try_present(true))service();
        }
#endif
#if R8_QUEUE_DIAG
        else queue_diag.due_run=0;
#endif
#if R8_LYNX_REPORT

        bool lq_serviced=false;
        if(now>=save_report){
            LU_STAGE(LU_REPORT);
#if N3_SAVE
            if(playing)flush_save();
#endif
            service();lq_serviced=true;save_report=now+2000000;
            LU_STAGE(LU_IDLE);
        }
#endif
        if(now>=report){
            LU_STAGE(LU_REPORT);
#if R8_LYNX_REPORT
            if(!lq_serviced)service();

            uint64_t lq_begin=time_us_64();
            if(lq_allow(lq_begin,report,audio_next,count,playing,r8_tx_pos==r8_tx_size)){
#else
#if N3_SAVE
            if(playing)flush_save();
#endif
            service();
#endif
#if R8_PROFILE
            r8_profile_report(frames,max_frame_us,slow,count,under,over,resync,playing?r8_pc():0);
#if R8_COLECO_HALT_BATCH
            uint64_t halt_ticks;uint32_t halt_spans;
            r8_coleco_halt_get(&halt_ticks,&halt_spans);
            r8_profile_printf("R8_CV_HALT skipped_ticks=%llu spans=%lu\n",
                (unsigned long long)halt_ticks,(unsigned long)halt_spans);
#endif
#if R8_DEFERRED_PRESENT
            r8_profile_printf("R8_PRESENT produced=%lu submitted=%lu blocked=%lu replaced=%lu deferred=%lu failed=%lu pending=%u render_max_us=%lu\n",
                (unsigned long)presentation.produced,(unsigned long)presentation.submitted,
                (unsigned long)presentation.blocked,(unsigned long)presentation.replaced,
                (unsigned long)presentation.deferred,(unsigned long)presentation.failed,
                (unsigned)presentation.pending,(unsigned long)render_max_us);
#endif
#if R8_QUEUE_DIAG
            r8_queue_report(&queue_diag,count,frames);
#endif
#if R8_SG_QUEUE_DIAG
            uint64_t sg_halt_ticks;uint32_t sg_halt_spans;
            r8_coleco_halt_get(&sg_halt_ticks,&sg_halt_spans);
            r8_profile_printf("R8_SG_HALT skipped_ticks=%llu spans=%lu\n",
                (unsigned long long)sg_halt_ticks,(unsigned long)sg_halt_spans);
#endif
#else
            printf("R8_PERF frames=%lu max_us=%lu slow=%lu audio_q=%u under=%lu over=%lu resync=%lu pc=%04x\n",(unsigned long)frames,(unsigned long)max_frame_us,(unsigned long)slow,count,(unsigned long)under,(unsigned long)over,(unsigned long)resync,playing?r8_pc():0);
#endif
            lynx_report();
#if R8_LYNX_SHORT
            ls_report();
#endif
#if R8_LYNX_SPRITE
            lsp_report();
#if R8_LYNX_COST
        lmc_report();
#endif
#endif
#if R8_LYNX_REPORT
            lq_finish(time_us_64()-lq_begin);lq_last_full=lq_begin;
#endif
            service();report=now+2000000;
#if R8_LYNX_REPORT
            }
#endif
            LU_STAGE(LU_IDLE);
        }
#if R8_LYNX_UNDER

#if R8_LYNX_REPORT

        if(r8_tx_pos==r8_tx_size &&
           ((report>now && report-now>500000) ||
            (lq_stats.pending && now>=lq_last_full && now-lq_last_full>500000)) &&
           (lu_report_ready() || now-lu_last_status>=2000000 ||
#if R8_LYNX_SAVE_DIAG
            lsv_pending() ||
#endif
#if R8_LYNX_LOAD_DIAG
            lld_pending() ||
#endif
            (lq_stats.pending && now-lq_last_status>=2000000))) {
#else
        if(r8_tx_pos==r8_tx_size && report>now && report-now>500000 &&
           (lu_report_ready() || now-lu_last_status>=2000000)) {
#endif
            LU_STAGE(LU_REPORT);
            if(!lu_report_one()
#if R8_LYNX_SAVE_DIAG
               && !lsv_report_one()
#endif
#if R8_LYNX_LOAD_DIAG
               && !lld_report_one()
#endif
            ) {
                if(now-lu_last_status>=2000000){
                    lu_report_status();lu_last_status=now;
                }
#if R8_LYNX_REPORT
                else if(lq_stats.pending && now-lq_last_status>=2000000){
                    uint64_t status_now=time_us_64();
                    r8_profile_printf("LYNX_REPORT_STATUS m=%u f=%lu pending=%u wait_us=%llu tries=%lu allow=%lu source=%lu tx=%lu under=%lu over=%lu zero=%lu resync=%lu\n",
                        (unsigned)lq_mode,(unsigned long)frames,(unsigned)lq_stats.pending,
                        (unsigned long long)(status_now>=report?status_now-report:0),
                        (unsigned long)lq_stats.tries,(unsigned long)lq_stats.allow,
                        (unsigned long)lq_stats.source,(unsigned long)lq_stats.tx,
                        (unsigned long)under,(unsigned long)over,(unsigned long)source_zero,(unsigned long)resync);
                    lq_last_status=status_now;
                }
#endif
            }
            LU_STAGE(LU_IDLE);
        }
#endif
        sleep_us(50);
    }
}
