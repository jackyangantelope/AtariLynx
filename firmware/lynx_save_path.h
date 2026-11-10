/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_SAVE_PATH_H
#define LYNX_SAVE_PATH_H
#include "ff.h"
#include <stdint.h>
enum { LCP_RELATIVE=0,LCP_OFF,LCP_UNPREPARED,LCP_MOUNT_CHANGED,LCP_DIRECTORY_CHANGED,LCP_PATH_CHANGED,LCP_FILESYSTEM };
extern const volatile uint32_t lcp_selector;
extern uint32_t lcp_mode,lcp_reason;
#if R8_LYNX_FS
extern const volatile uint32_t lfx_selector;
extern uint32_t lfx_mode;
const char *lfx_fs_name(unsigned type);
unsigned lfx_depth(const FATFS *fs);
#endif
void lcp_reset(void);
FRESULT lcp_prepare(FATFS *fs,const char *directory,const char *absolute);
const char *lcp_save_name(FATFS *fs,const char *absolute);
#endif
