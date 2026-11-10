/* SPDX-License-Identifier: BSD-3-Clause
 * LBP029: exact constant PCM runs, gated to synchronous cartridge startup.
 * Ready/emit are main-core-only sinks with no service, IRQ or guest callbacks.
 */
#ifndef LYNX_BOOT_PCM_H
#define LYNX_BOOT_PCM_H
#include <stdint.h>
#include <stdbool.h>
#if R8_LYNX_BOOT_PCM
#ifdef __cplusplus
extern "C" {
#endif
enum { LBP_MIN_RAW=4096, LBP_MAX_RAW=0xffffffffu/500u };
typedef bool (*lbp_ready_fn)(void);
typedef void (*lbp_emit_fn)(int16_t,int16_t,uint32_t);
typedef struct {
    uint64_t sound_us,raw_long;
    uint32_t calls,max_raw,max_us,batched,fallback;
    uint32_t raw_total,converted_total,phase,previous;
    uint32_t queued_before,queued_after,enqueued,dropped;
} lbp_stats;
extern const volatile uint32_t lbp_selector;
extern uint32_t lbp_mode,lbp_active;
extern lbp_stats lbp_data;
extern lbp_ready_fn lbp_ready;
extern lbp_emit_fn lbp_emit;
void lbp_set_sink(lbp_ready_fn ready,lbp_emit_fn emit);
void lbp_reset(void);
uint32_t lbp_clock(void);
void lbp_long_end(uint32_t raw,uint32_t start,bool batched);
void lbp_source(uint32_t before,uint32_t after,uint32_t enqueued,uint32_t dropped);
/* Success consumes all raw samples and advances audio-last-cycle exactly;
 * rejection is completely side-effect-free and requires the scalar loop. */
bool n3_lynx_pcm_batch(int16_t left,int16_t right,uint32_t raw);
#ifdef __cplusplus
}
#endif
#endif
#endif
