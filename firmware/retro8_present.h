/* SPDX-License-Identifier: BSD-3-Clause
 * One latest indexed image, retried without waiting for HDMI buffer ownership.
 * The emulator overwrites that image only when the next frame is produced.
 */
#ifndef JACK_RETRO8_PRESENT_H
#define JACK_RETRO8_PRESENT_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t produced, submitted, blocked, replaced, deferred, failed;
    bool pending;
} r8_present_state;
static inline void r8_present_offer(r8_present_state *s) {
    s->produced++;
    if(s->pending)s->replaced++;
    s->pending=true;
}
static inline void r8_present_finish(r8_present_state *s,bool accepted,bool idle) {
    if(!accepted){s->failed++;return;}
    s->pending=false;s->submitted++;
    if(idle)s->deferred++;
}
#endif
