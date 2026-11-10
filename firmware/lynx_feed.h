/* SPDX-License-Identifier: BSD-3-Clause
 * LFE030 feeds only existing PCM at completed file-operation boundaries.
 * No file, guest, input or resampler callback is allowed in the feed path.
 */
#ifndef LYNX_FEED_H
#define LYNX_FEED_H
#include <stdint.h>
#if R8_LYNX_SAVE_FEED
typedef struct {
    uint32_t boundaries,pumps,service_us,max_gap_us;
    uint32_t q0,q1,di0,di1,u0,u1,z0,z1,empty0,empty1,limited0,limited1;
} lfe_stats;
extern const volatile uint32_t lfe_selector;
extern uint32_t lfe_mode;
extern lfe_stats lfe_data;
void lfe_reset(void);
typedef struct {
    uint32_t valid,frame,cycle,prev_cycle,mask,kind;
    uint32_t span_us,prev_tail_us,gap_us,usb_us,pad_us,poll_us,body_us;
    uint32_t qprev,q0,q1,under;
} lgap_record;
extern lgap_record lgap_worst;
extern uint32_t lgap_count,lgap_valid,lgap_audio,lgap_end,lgap_q,lgap_cycle;
void lgap_reset(void);
/* Uses already-read service timestamps. Unsigned subtraction keeps us32 wrap.
 * Healthy queue path only advances five scalars; retain one worst low-queue
 * audio-entry interval per admitted ordinary report, not a per-sample log. */
static inline void lgap_note(uint32_t begin,uint32_t usb,uint32_t pad,uint32_t audio,
                            uint32_t end,uint32_t frame,uint32_t cycle,uint32_t mask,
                            uint32_t q0,uint32_t q1,uint32_t under,uint32_t kind){
    if(lgap_valid && (lgap_q<512 || q0<512 || q1<512)){
        uint32_t span=audio-lgap_audio;
        if(span>=1000){
            ++lgap_count;
            if(span>lgap_worst.span_us){
                lgap_worst=(lgap_record){1,frame,cycle,lgap_cycle,mask,kind,
                    span,lgap_end-lgap_audio,begin-lgap_end,usb-begin,pad-usb,
                    audio-pad,end-audio,lgap_q,q0,q1,under};
            }
        }
    }
    lgap_valid=1;lgap_audio=audio;lgap_end=end;lgap_q=q1;lgap_cycle=cycle;
}
#endif
#endif
