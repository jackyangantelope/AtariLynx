/* SPDX-License-Identifier: BSD-3-Clause
 * LLD028: passive, coarse load timing. Never called from a guest instruction.
 */
#ifndef LYNX_LOAD_DIAG_H
#define LYNX_LOAD_DIAG_H
#include <stdint.h>
#include "lynx_boot_pcm.h"
#include "lynx_boot_cpu.h"
#include "lynx_scan.h"
#include "lynx_wide.h"
#include "lynx_romclk.h"
#if R8_LYNX_LOAD_DIAG
#ifdef __cplusplus
extern "C" {
#endif
enum { LLD_ROM, LLD_BIOS, LLD_SAVE, LLD_BACKUP, LLD_FILES };
enum { LLD_VALIDATE, LLD_ROM_CRC, LLD_PAYLOAD_CRC, LLD_SAVE_CRC,
       LLD_CLEAR, LLD_CONSTRUCT, LLD_BANK_CRC, LLD_APPLY, LLD_BOOT,
       LLD_RESET, LLD_PHASES };
typedef struct {
    uint32_t called,total_us,open_us,read_us,close_us,requested,bytes;
    uint32_t open_rc,read_rc,close_rc,rejected;
} lld_io;
typedef struct {
    uint32_t sequence,start,total_us,core_us,path_us,first_us,first_frame;
    uint32_t ok,present,save_source,rom_crc,save_crc,boot_calls,boot_cycles;
    uint32_t boot_eof,boot_halt,phase_mask,ready;
    lld_io io[LLD_FILES];
    uint32_t phase_us[LLD_PHASES];
#if R8_LYNX_BOOT_PCM
    lbp_stats audio;
#endif
#if R8_LYNX_BOOT_CPU
    lbc_stats cpu;
#if R8_LYNX_SCAN
    lcs_stats scan;
#if R8_LYNX_WIDE
    lwd_stats wide;
#endif
#endif
#endif
} lld_record;
extern uint32_t lld_wait_present,lld_dropped;
uint32_t lld_clock(void);
void lld_begin(void);
void lld_file(unsigned which,const lld_io *io);
#if R8_LYNX_ROMCLK
void lld_rom_clock(const lrc_stats *stats);
const lrc_stats *lld_rom_clock_get(void);
#endif
void lld_save_source(unsigned source);
void lld_core_begin(void);
void lld_phase(unsigned phase);
void lld_core_end(void);
void lld_boot_begin(uint32_t cycles);
void lld_boot_call(void);
void lld_boot_end(uint32_t cycles,uint32_t eof,uint32_t halt);
void lld_path(uint32_t us);
void lld_done(unsigned ok,uint32_t rom_crc,uint32_t save_crc);
void lld_first_present(uint32_t frame);
void lld_cancel_present(void);
const lld_record *lld_pending(void);
void lld_release(void);
#ifdef __cplusplus
}
#endif
#else
#define lld_begin(...) ((void)0)
#define lld_save_source(...) ((void)0)
#define lld_core_begin(...) ((void)0)
#define lld_phase(...) ((void)0)
#define lld_core_end(...) ((void)0)
#define lld_boot_begin(...) ((void)0)
#define lld_boot_call(...) ((void)0)
#define lld_boot_end(...) ((void)0)
#define lld_path(...) ((void)0)
#define lld_done(...) ((void)0)
#define lld_cancel_present(...) ((void)0)
#endif
#endif
