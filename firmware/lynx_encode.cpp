/* SPDX-License-Identifier: BSD-3-Clause */
#include "lynx_encode.h"
#if R8_LYNX_ENCODE
#ifndef LE_MODE
#define LE_MODE 1
#endif
static_assert(LE_MODE==1 || LE_MODE==2,"fixed encoder OFF=1 ON=2");
const volatile uint32_t le_selector=LE_MODE;
uint32_t le_mode,le_packets,le_samples,le_pack_us,le_encode_us,le_queue_us,le_max_us;
void le_reset(void){
 le_mode=le_selector==2?2:1;
 le_packets=le_samples=le_pack_us=le_encode_us=le_queue_us=le_max_us=0;
}
#endif
