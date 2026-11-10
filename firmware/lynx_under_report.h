/* SPDX-License-Identifier: BSD-3-Clause
 * Deferred main-thread reporter. Caller reserves an empty TX queue and a
 * 500ms margin from the normal burst. Never call from service or PCM.
 */
#ifndef LYNX_UNDER_REPORT_H
#define LYNX_UNDER_REPORT_H
static uint32_t lu_print_run,lu_print_seq;
static bool lu_print_started;
static bool lu_report_ready(void) {
    const lu_event *e=lu_pending();
    return e && (!lu_print_started || lu_print_run!=e->run ||
                 lu_print_seq!=e->sequence || lu_peek_record() || e->complete);
}
static void lu_report_reset(void) {
    const lu_status *s=lu_get_status();
    lu_print_started=false;
    r8_profile_printf("LYNX_UNDER_RESET run=%lu cancelled=%lu lost=%lu\n",
        (unsigned long)s->run,(unsigned long)s->cancelled,(unsigned long)s->lost);
}
static void lu_report_status(void) {
    const lu_status *s=lu_get_status();
    r8_profile_printf("LYNX_UNDER_STATUS run=%lu mode=%lu svc=%lu events=%lu lost=%lu cancelled=%lu pending=%u\n",
        (unsigned long)s->run,(unsigned long)lu_mode,(unsigned long)s->services,
        (unsigned long)s->events,(unsigned long)s->lost,(unsigned long)s->cancelled,
        lu_pending()!=0);
}
static bool lu_report_one(void) {
    const lu_event *e=lu_pending();
    if(!e)return false;
    uint32_t drops=r8_tx_drops;
    if(!lu_print_started || lu_print_run!=e->run || lu_print_seq!=e->sequence) {
        r8_profile_printf("LYNX_UNDER_BEGIN run=%lu seq=%lu reason=%lu history=%u lost=%lu\n",
            (unsigned long)e->run,(unsigned long)e->sequence,(unsigned long)e->reason,
            e->history_count,(unsigned long)e->lost_at_trigger);
        if(drops==r8_tx_drops) {
            lu_print_run=e->run;lu_print_seq=e->sequence;lu_print_started=true;
        }
        return true;
    }
    const lu_record *r=lu_peek_record();
    if(r) {
        const lu_counts *a=&r->before.counts,*b=&r->after;
        r8_profile_printf("LYNX_UNDER_ROW run=%lu seq=%lu idx=%u t=%llu next=%llu frame=%lu mask=%lu flags=%lu "
            "g0=%lu e0=%lu p0=%lu z0=%lu u0=%lu r0=%lu q0=%u a0=%u b0=%u "
            "g1=%lu e1=%lu p1=%lu z1=%lu u1=%lu r1=%lu q1=%u a1=%u b1=%u\n",
            (unsigned long)e->run,(unsigned long)e->sequence,e->read_index,
            (unsigned long long)r->before.scaled_now,(unsigned long long)r->before.audio_next,
            (unsigned long)r->before.frame,(unsigned long)r->phase_mask,(unsigned long)r->flags,
            (unsigned long)a->guest_cycles,(unsigned long)a->enqueued,(unsigned long)a->played,
            (unsigned long)a->zero,(unsigned long)a->under,(unsigned long)a->resync,a->q,a->active,a->buffering,
            (unsigned long)b->guest_cycles,(unsigned long)b->enqueued,(unsigned long)b->played,
            (unsigned long)b->zero,(unsigned long)b->under,(unsigned long)b->resync,b->q,b->active,b->buffering);
        if(drops==r8_tx_drops)lu_consume_record();
        return true;
    }
    if(e->complete) {
        r8_profile_printf("LYNX_UNDER_END run=%lu seq=%lu rows=%u close=%u aborted=%u\n",
            (unsigned long)e->run,(unsigned long)e->sequence,e->record_count,e->close_reason,e->aborted);
        if(drops==r8_tx_drops) { (void)lu_release_event();lu_print_started=false; }
        return true;
    }
    return false; /* Waiting for recovery: STATUS must still be available. */
}
#endif
