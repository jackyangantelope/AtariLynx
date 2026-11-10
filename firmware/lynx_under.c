/* SPDX-License-Identifier: BSD-3-Clause
 * LU021 only copies existing main-thread observations. In particular, it does
 * not read clocks, divide timestamps, generate PCM or change queue decisions.
 */
#include "lynx_under.h"
#include <stddef.h>
#include <string.h>

#ifndef LU_MODE
#define LU_MODE 1
#endif
_Static_assert(LU_MODE==1 || LU_MODE==2, "LU_MODE must be fixed OFF1 or ON2");
const volatile uint32_t lu_selector=LU_MODE;
uint32_t lu_mode, lu_stage, lu_phase_mask;

typedef struct {
    lu_status status;
    lu_event event;
    lu_record history[LU_HISTORY], current;
    uint8_t history_count, history_next;
    bool pending, open;
} lu_state;
static lu_state state;
_Static_assert(sizeof(lu_state)+3*sizeof(uint32_t)<=1536,
               "LU021 observer state exceeds its 1.5 KiB budget");

void lu_reset(void) {
    lu_status old=state.status;
    if(state.pending)++old.cancelled;
    lu_mode=1;
    memset(&state,0,sizeof(state));
    state.status.run=old.run+1u;
    state.status.events=old.events;
    state.status.lost=old.lost;
    state.status.cancelled=old.cancelled;
    lu_stage=LU_IDLE;
    lu_phase_mask=1u<<LU_IDLE;
    lu_mode=lu_selector==2 ? 2u : 1u;
}

void lu_service_begin(const lu_snapshot *before) {
    if(!lu_enabled())return;
    state.current.before=*before;
    state.current.phase_mask=lu_phase_mask;
    state.current.flags=0;
    lu_phase_mask=1u<<lu_stage;
    state.open=true;
}

static void freeze_event(void) {
    lu_event *event=&state.event;
    memset(event,0,offsetof(lu_event,records));
    event->run=state.status.run;
    event->sequence=state.status.events;
    event->reason=state.current.flags&(LU_UNDER|LU_RESYNC);
    event->lost_at_trigger=state.status.lost;
    event->history_count=state.history_count;
    unsigned start=(state.history_next+LU_HISTORY-state.history_count)%LU_HISTORY;
    for(unsigned i=0;i<state.history_count;++i)
        event->records[i]=state.history[(start+i)%LU_HISTORY];
    event->records[event->history_count]=state.current;
    event->record_count=event->history_count+1u;
    if(!state.current.after.active) {
        event->complete=1;
        event->aborted=1;
        event->close_reason=LU_CLOSE_STOP;
    } else if(!(event->reason&LU_UNDER)) {
        event->complete=1;
        event->close_reason=LU_CLOSE_RESYNC;
    } else if(!state.current.after.buffering) {
        event->complete=1;
        event->close_reason=LU_CLOSE_REFILL;
    }
    state.pending=true;
}

void lu_service_end(const lu_counts *after) {
    if(!lu_enabled() || !state.open)return;
    state.open=false;
    ++state.status.services;
    lu_record *record=&state.current;
    record->after=*after;
    const lu_counts *before=&record->before.counts;
    bool active=before->active || after->active;
    bool under=active && after->under!=before->under;
    bool resync=active && after->resync!=before->resync;
    /* An under while entering buffered implies that the original service
     * resumed consumption and then exhausted it again within this call. */
    bool refill=before->buffering && (!after->buffering || under);
    record->flags=(under?LU_UNDER:0u)|(resync?LU_RESYNC:0u)|
                  (refill?LU_REFILL:0u)|(!after->active?LU_STOP:0u);
    bool had_pending=state.pending;
    if(under || resync) {
        ++state.status.events;
        if(state.pending)++state.status.lost;
        else freeze_event();
    }
    if(had_pending && !state.event.complete && (refill || !after->active)) {
        lu_event *event=&state.event;
        event->records[event->record_count++]=*record;
        event->complete=1;
        event->aborted=!after->active;
        event->close_reason=after->active?LU_CLOSE_REFILL:LU_CLOSE_STOP;
    }
    state.history[state.history_next]=*record;
    state.history_next=(state.history_next+1u)%LU_HISTORY;
    if(state.history_count<LU_HISTORY)++state.history_count;
}

const lu_status *lu_get_status(void) { return &state.status; }
const lu_event *lu_pending(void) { return state.pending?&state.event:NULL; }
const lu_record *lu_peek_record(void) {
    if(!state.pending || state.event.read_index>=state.event.record_count)return NULL;
    return &state.event.records[state.event.read_index];
}
void lu_consume_record(void) {
    if(lu_peek_record())++state.event.read_index;
}
bool lu_release_event(void) {
    if(!state.pending || !state.event.complete ||
       state.event.read_index!=state.event.record_count)return false;
    state.pending=false;
    return true;
}
