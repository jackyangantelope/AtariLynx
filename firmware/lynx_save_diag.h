/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_SAVE_DIAG_H
#define LYNX_SAVE_DIAG_H
#include <stdint.h>
#include <stdbool.h>
#include "lynx_feed.h"
enum { LSV_EXPORT,LSV_PREP,LSV_OPEN,LSV_WRITE,LSV_SYNC,LSV_CLOSE,LSV_STAT,
       LSV_UNLINK,LSV_BACKUP,LSV_COMMIT,LSV_RESTORE,LSV_CACHE,LSV_LOG,LSV_PHASES };
enum { LSV_READY,LSV_TOKEN,LSV_COMMAND,LSV_WAITS };
enum { LSV_CAPACITY=2 };
typedef struct { uint32_t calls,us,max_us,failed; } lsv_wait;
#if R8_LYNX_PATH
enum { LSV_READ,LSV_WRITE_IO,LSV_IOS };
/* completed writes are accepted data blocks, NOT proof of durable storage. */
typedef struct { uint32_t calls,requested,completed,failed,us,max_us; } lsv_io;
#endif
typedef struct { uint32_t us; int32_t result; lsv_wait wait[LSV_WAITS];
#if R8_LYNX_PATH
    lsv_io io[LSV_IOS];
#endif
} lsv_phase;
typedef struct {
    uint32_t run,sequence,frame,bytes,queued,written,result,total_us,mask,dropped;
    lsv_phase phase[LSV_PHASES];
#if R8_LYNX_PATH
    uint32_t path_relative,path_reason;
#endif
#if R8_LYNX_SAVE_FEED
    lfe_stats feed;
#endif
} lsv_record;
extern const volatile uint32_t lsv_selector;
extern uint32_t lsv_mode,lsv_transactions,lsv_dropped;
void lsv_reset(void); /* Startup only. Completed records survive game reload. */
uint32_t lsv_clock(void);
void lsv_begin(uint32_t run,uint32_t frame,uint32_t bytes,uint32_t queued,
               uint32_t start_us,uint32_t export_us);
void lsv_phase_begin(unsigned phase);
void lsv_phase_end(int32_t result);
uint32_t lsv_wait_begin(void);
void lsv_wait_end(unsigned kind,uint32_t start,bool failed);
void lsv_end(uint32_t result,uint32_t written);
const lsv_record *lsv_pending(void);
void lsv_release(void);
#if R8_LYNX_PATH
void lsv_set_path(bool relative,uint32_t reason);
void lsv_io_end(unsigned kind,uint32_t start,uint32_t requested,uint32_t completed,bool failed);
#endif
#endif
