/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_ROMCLK_H
#define LYNX_ROMCLK_H
#include <stdint.h>
#include <stdbool.h>
#if R8_LYNX_ROMCLK
#ifdef __cplusplus
extern "C" {
#endif
enum { LRC_APPLIED=0,LRC_OFF=1,LRC_PIO=2,LRC_NOT_READY=3,LRC_BUS_BUSY=4,LRC_PERI=5,LRC_ALREADY_FAST=6,LRC_DIRTY_FS=7 };
typedef struct {
    uint32_t mode,reason,hardware,peri_hz,requested_hz,before_hz,read_hz,after_hz,restored;
    uint32_t scope_us,enter_us,exit_us,cr0_before,cpsr_before,cr0_read,cpsr_read,cr1_before,cfg_before,cfg_after;
} lrc_stats;
typedef struct { lrc_stats stats;void *spi;uint32_t snapshot,changed; } lrc_scope;
extern volatile const uint32_t lrc_selector;
void pico_fatfs_romclk_begin(lrc_scope *scope,uint32_t mode);
bool pico_fatfs_romclk_end(lrc_scope *scope);
void lrc_store(unsigned slot,uint32_t sequence,const lrc_stats *stats);
const lrc_stats *lrc_get(unsigned slot,uint32_t sequence);
#ifdef __cplusplus
}
#endif
#endif
#endif
