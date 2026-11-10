/* SPDX-License-Identifier: BSD-3-Clause
 * LU021: main-thread-only passive source-audio observer. No sample/clock hook.
 */
#ifndef LYNX_UNDER_H
#define LYNX_UNDER_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { LU_IDLE=0, LU_CORE=1, LU_RENDER=2, LU_SAVE=3, LU_REPORT=4 };
enum { LU_HISTORY=4, LU_RECORDS=LU_HISTORY+2 };
enum { LU_UNDER=1u, LU_RESYNC=2u, LU_REFILL=4u, LU_STOP=8u };
enum { LU_CLOSE_NONE=0, LU_CLOSE_REFILL=1, LU_CLOSE_RESYNC=2, LU_CLOSE_STOP=3 };
extern const volatile uint32_t lu_selector;
extern uint32_t lu_mode, lu_stage, lu_phase_mask;
static inline bool lu_enabled(void) { return lu_mode==2; }
/* Mark main-thread phases only. Bits describe occupancy, NOT elapsed cost. */
static inline void lu_stage_set(uint32_t stage) {
    if(lu_enabled() && stage<=LU_REPORT) {
        lu_stage=stage;
        lu_phase_mask|=1u<<stage;
    }
}

typedef struct {
    uint32_t enqueued, played, zero, under, resync, guest_cycles;
    uint16_t q;
    uint8_t active, buffering;
} lu_counts;
typedef struct {
    uint64_t scaled_now;       /* Existing time_us_64()*44100 timestamp. */
    uint64_t audio_next;       /* Before the original resync decision. */
    uint32_t frame;
    lu_counts counts;
} lu_snapshot;
typedef struct {
    lu_snapshot before;
    lu_counts after;
    uint32_t phase_mask;       /* Previous audio entry through this entry. */
    uint32_t flags;            /* LU_UNDER / LU_RESYNC / LU_REFILL. */
} lu_record;
typedef struct {
    uint32_t run, sequence, reason, lost_at_trigger;
    uint8_t history_count, record_count, read_index, complete;
    uint8_t aborted, close_reason;
    lu_record records[LU_RECORDS]; /* History oldest first, trigger, refill. */
} lu_event;
typedef struct {
    /* events/lost count fault-bearing services, not individual reason bits. */
    uint32_t run, services, events, lost, cancelled;
} lu_status;

/* Call at load/reset. Pending events are cancelled explicitly; cumulative
 * events/lost/cancelled survive reset, services and history restart per run.
 * OFF still selects identical code/data layout but collects no observations.
 */
void lu_reset(void);
/* Root guards snapshot construction with lu_enabled(). Calls themselves also
 * guard OFF. No division, timer read, producer call or audio mutation occurs.
 */
void lu_service_begin(const lu_snapshot *before);
void lu_service_end(const lu_counts *after);

/* Main-thread deferred reporter. Peek returns immutable storage valid until
 * release/reset. Consume ONLY after accepting that row for UART output.
 * An event waiting for refill may expose its history/trigger rows immediately.
 * Once complete and every row consumed, print END then release. A new fault
 * while pending increments lost; it never overwrites the retained event.
 */
const lu_status *lu_get_status(void);
const lu_event *lu_pending(void);
const lu_record *lu_peek_record(void);
void lu_consume_record(void);
bool lu_release_event(void);

#ifdef __cplusplus
}
#endif
#endif
