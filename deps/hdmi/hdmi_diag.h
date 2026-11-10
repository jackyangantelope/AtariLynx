/* SPDX-License-Identifier: BSD-3-Clause. Cumulative single-writer counters. */
#ifndef LYNX_HDMI_DIAG_H
#define LYNX_HDMI_DIAG_H
#include <stdint.h>
uint32_t hstx_diag_irq_count(void);
uint32_t hstx_diag_stalls(void);
uint32_t hstx_diag_overrate(void);
uint32_t hstx_diag_limited(void);
uint32_t hstx_diag_pushed(void);
uint32_t hstx_diag_full(void);
uint32_t hstx_diag_consumed(void);
#endif
