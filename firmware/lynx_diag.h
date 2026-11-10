/* SPDX-License-Identifier: BSD-3-Clause */
#ifndef LYNX_DIAG_H
#define LYNX_DIAG_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t count, nonzero, changes;
    int16_t minimum, maximum;
} lynx_pcm_stats;
typedef struct {
    lynx_pcm_stats raw, converted;
    uint32_t frame, video_frame, video_samples, raw_hash, rgb_hash, raw_nonzero, rgb_nonzero;
    uint32_t rom_crc, payload_crc, bank_crc, save_crc, save_bytes, save_applied;
    uint16_t buttons;
    uint8_t rotation, pending_rotation;
} lynx_diag_stats;
uint32_t lynx_crc32(const void *data,size_t size);
void lynx_diag_get(lynx_diag_stats *out);
void lynx_diag_interval_reset(void);
void lynx_diag_request_video(void);
int lynx_diag_wants_video(void);
#ifdef __cplusplus
}
#endif
#endif
