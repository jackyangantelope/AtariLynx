#include "hstx_packet.h"

#include <string.h>
#include "pico.h"

// The per-audio-packet encode path (hstx_packet_set_audio_samples +
// hstx_encode_data_island and their tables) is kept out of flash: it runs
// ~46 times per frame on core0, and a flash fetch can stall behind QMI
// traffic (CHD hunk decompress hammering PSRAM), delaying the audio push
// long enough to drain the HDMI data-island queue.

// ============================================================================
// TERC4 Symbol Table (4-bit to 10-bit encoding)
// ============================================================================

static const uint16_t __not_in_flash("hstx_packet") ter_c4[16] = {
    0b1010011100, // 0
    0b1001100011, // 1
    0b1011100100, // 2
    0b1011100010, // 3
    0b0101110001, // 4
    0b0100011110, // 5
    0b0110001110, // 6
    0b0100111100, // 7
    0b1011001100, // 8
    0b0100111001, // 9
    0b0110011100, // 10
    0b1011000110, // 11
    0b1010001110, // 12
    0b1001110001, // 13
    0b0101100011, // 14
    0b1011000011, // 15
};

#define GUARD_BAND_SYMBOL 0x133u // 0b0100110011

// ============================================================================
// BCH Encoding
// ============================================================================

static const uint8_t __not_in_flash("hstx_packet") bch_table[256] = {
    0x00, 0xd9, 0xb5, 0x6c, 0x6d, 0xb4, 0xd8, 0x01, 0xda, 0x03, 0x6f, 0xb6, 0xb7, 0x6e, 0x02, 0xdb, 0xb3, 0x6a, 0x06,
    0xdf, 0xde, 0x07, 0x6b, 0xb2, 0x69, 0xb0, 0xdc, 0x05, 0x04, 0xdd, 0xb1, 0x68, 0x61, 0xb8, 0xd4, 0x0d, 0x0c, 0xd5,
    0xb9, 0x60, 0xbb, 0x62, 0x0e, 0xd7, 0xd6, 0x0f, 0x63, 0xba, 0xd2, 0x0b, 0x67, 0xbe, 0xbf, 0x66, 0x0a, 0xd3, 0x08,
    0xd1, 0xbd, 0x64, 0x65, 0xbc, 0xd0, 0x09, 0xc2, 0x1b, 0x77, 0xae, 0xaf, 0x76, 0x1a, 0xc3, 0x18, 0xc1, 0xad, 0x74,
    0x75, 0xac, 0xc0, 0x19, 0x71, 0xa8, 0xc4, 0x1d, 0x1c, 0xc5, 0xa9, 0x70, 0xab, 0x72, 0x1e, 0xc7, 0xc6, 0x1f, 0x73,
    0xaa, 0xa3, 0x7a, 0x16, 0xcf, 0xce, 0x17, 0x7b, 0xa2, 0x79, 0xa0, 0xcc, 0x15, 0x14, 0xcd, 0xa1, 0x78, 0x10, 0xc9,
    0xa5, 0x7c, 0x7d, 0xa4, 0xc8, 0x11, 0xca, 0x13, 0x7f, 0xa6, 0xa7, 0x7e, 0x12, 0xcb, 0x83, 0x5a, 0x36, 0xef, 0xee,
    0x37, 0x5b, 0x82, 0x59, 0x80, 0xec, 0x35, 0x34, 0xed, 0x81, 0x58, 0x30, 0xe9, 0x85, 0x5c, 0x5d, 0x84, 0xe8, 0x31,
    0xea, 0x33, 0x5f, 0x86, 0x87, 0x5e, 0x32, 0xeb, 0xe2, 0x3b, 0x57, 0x8e, 0x8f, 0x56, 0x3a, 0xe3, 0x38, 0xe1, 0x8d,
    0x54, 0x55, 0x8c, 0xe0, 0x39, 0x51, 0x88, 0xe4, 0x3d, 0x3c, 0xe5, 0x89, 0x50, 0x8b, 0x52, 0x3e, 0xe7, 0xe6, 0x3f,
    0x53, 0x8a, 0x41, 0x98, 0xf4, 0x2d, 0x2c, 0xf5, 0x99, 0x40, 0x9b, 0x42, 0x2e, 0xf7, 0xf6, 0x2f, 0x43, 0x9a, 0xf2,
    0x2b, 0x47, 0x9e, 0x9f, 0x46, 0x2a, 0xf3, 0x28, 0xf1, 0x9d, 0x44, 0x45, 0x9c, 0xf0, 0x29, 0x20, 0xf9, 0x95, 0x4c,
    0x4d, 0x94, 0xf8, 0x21, 0xfa, 0x23, 0x4f, 0x96, 0x97, 0x4e, 0x22, 0xfb, 0x93, 0x4a, 0x26, 0xff, 0xfe, 0x27, 0x4b,
    0x92, 0x49, 0x90, 0xfc, 0x25, 0x24, 0xfd, 0x91, 0x48,
};

static const uint8_t __not_in_flash("hstx_packet") parity_table[32] = {0x96, 0x69, 0x69, 0x96, 0x69, 0x96, 0x96, 0x69, 0x69, 0x96, 0x96,
                                         0x69, 0x96, 0x69, 0x69, 0x96, 0x69, 0x96, 0x96, 0x69, 0x96, 0x69,
                                         0x69, 0x96, 0x96, 0x69, 0x69, 0x96, 0x69, 0x96, 0x96, 0x69};

static inline bool compute_parity(uint8_t v)
{
    return (parity_table[v / 8] >> (v % 8)) & 1;
}

static inline bool compute_parity3(uint8_t a, uint8_t b, uint8_t c)
{
    return compute_parity(a) ^ compute_parity(b) ^ compute_parity(c);
}

static uint8_t __not_in_flash_func(encode_bch_3)(const uint8_t *p)
{
    uint8_t v = bch_table[p[0]];
    v = bch_table[p[1] ^ v];
    v = bch_table[p[2] ^ v];
    return v;
}

static uint8_t __not_in_flash_func(encode_bch_7)(const uint8_t *p)
{
    uint8_t v = bch_table[p[0]];
    v = bch_table[p[1] ^ v];
    v = bch_table[p[2] ^ v];
    v = bch_table[p[3] ^ v];
    v = bch_table[p[4] ^ v];
    v = bch_table[p[5] ^ v];
    v = bch_table[p[6] ^ v];
    return v;
}

static void __not_in_flash_func(compute_header_parity)(hstx_packet_t *p)
{
    p->header[3] = encode_bch_3(p->header);
}

static void __not_in_flash_func(compute_subpacket_parity)(hstx_packet_t *p, int idx)
{
    p->subpacket[idx][7] = encode_bch_7(p->subpacket[idx]);
}

static void compute_all_parity(hstx_packet_t *p)
{
    compute_header_parity(p);
    for (int i = 0; i < 4; i++) {
        compute_subpacket_parity(p, i);
    }
}

static void compute_infoframe_checksum(hstx_packet_t *p)
{
    int sum = 0;
    for (int i = 0; i < 3; i++) {
        sum += p->header[i];
    }
    int len = p->header[2] + 1;
    for (int j = 0; j < 4 && len > 0; j++) {
        for (int i = 0; i < 7 && len > 0; i++, len--) {
            sum += p->subpacket[j][i];
        }
    }
    p->subpacket[0][0] = (uint8_t)(-sum);
}

// ============================================================================
// Public API
// ============================================================================

void __not_in_flash_func(hstx_packet_init)(hstx_packet_t *packet)
{
    // Volatile byte stores instead of memset: keeps the zeroing inline so
    // this SRAM function never calls the flash-resident libc memset.
    volatile uint8_t *p = (volatile uint8_t *)packet;
    for (size_t i = 0; i < sizeof(hstx_packet_t); i++)
        p[i] = 0;
}

void hstx_packet_set_null(hstx_packet_t *packet)
{
    hstx_packet_init(packet);
    compute_all_parity(packet);
}

void hstx_packet_set_acr(hstx_packet_t *packet, uint32_t n, uint32_t cts)
{
    hstx_packet_init(packet);
    packet->header[0] = 0x01;
    packet->header[1] = 0x00;
    packet->header[2] = 0x00;
    compute_header_parity(packet);

    packet->subpacket[0][0] = 0;
    packet->subpacket[0][1] = (cts >> 16) & 0x0F;
    packet->subpacket[0][2] = (cts >> 8) & 0xFF;
    packet->subpacket[0][3] = cts & 0xFF;
    packet->subpacket[0][4] = (n >> 16) & 0x0F;
    packet->subpacket[0][5] = (n >> 8) & 0xFF;
    packet->subpacket[0][6] = n & 0xFF;
    compute_subpacket_parity(packet, 0);

    memcpy(packet->subpacket[1], packet->subpacket[0], 8);
    memcpy(packet->subpacket[2], packet->subpacket[0], 8);
    memcpy(packet->subpacket[3], packet->subpacket[0], 8);
}

void hstx_packet_set_audio_infoframe(hstx_packet_t *packet, uint32_t sample_rate, uint8_t channels,
                                     uint8_t bits_per_sample)
{
    hstx_packet_init(packet);
    packet->header[0] = 0x84;
    packet->header[1] = 0x01;
    packet->header[2] = 0x0A;

    uint8_t cc = (channels - 1) & 0x07;
    uint8_t ct = 0x01;
    uint8_t ss =
        (bits_per_sample == 16) ? 0x01 : (bits_per_sample == 20 ? 0x02 : (bits_per_sample == 24 ? 0x03 : 0x00));
    uint8_t sf = (sample_rate == 32000) ? 0x01 : (sample_rate == 44100 ? 0x02 : (sample_rate == 48000 ? 0x03 : 0x00));

    packet->subpacket[0][1] = cc | (ct << 4);
    packet->subpacket[0][2] = ss | (sf << 2);
    packet->subpacket[0][3] = 0x00;
    packet->subpacket[0][4] = 0x00;
    packet->subpacket[0][5] = 0x00;

    compute_infoframe_checksum(packet);
    compute_all_parity(packet);
}

void hstx_packet_set_avi_infoframe(hstx_packet_t *packet, uint8_t vic, uint8_t pixel_repetition)
{
    hstx_packet_init(packet);
    packet->header[0] = 0x82;
    packet->header[1] = 0x02;
    packet->header[2] = 0x0D;

    packet->subpacket[0][1] = 0x00;
    packet->subpacket[0][2] = 0x08;
    packet->subpacket[0][3] = 0x00;
    packet->subpacket[0][4] = vic;
    packet->subpacket[0][5] = pixel_repetition & 0x0F;

    compute_infoframe_checksum(packet);
    compute_all_parity(packet);
}

// IEC 60958-3 channel status bytes, one bit per audio frame across the
// 192-frame block: byte 0 = 0x04 (consumer, L-PCM, copy permitted), byte 3 =
// sample-frequency code (bits 24-27). Bytes 5..23 are zero, which IEC 60958
// treats as valid consumer defaults, so only the first 5 bytes are stored.
// Mutable so hstx_packet_set_cs_sample_rate can retarget the rate code; lives
// in .data (RAM), safe to read from the not-in-flash encode path.
static uint8_t cs_bytes[5] = {0x04, 0x00, 0x00, 0x02 /* 48 kHz */, 0x00};

void hstx_packet_set_cs_sample_rate(uint32_t sample_rate)
{
    // Sample-frequency code, IEC 60958-3 bits 24-27 (LSB-first in byte 3):
    // 44.1 kHz = 0000, 48 kHz = 0100, 32 kHz = 1100.
    switch (sample_rate) {
        case 44100: cs_bytes[3] = 0x00; break;
        case 32000: cs_bytes[3] = 0x03; break;
        case 48000:
        default:    cs_bytes[3] = 0x02; break;
    }
}

static inline int channel_status_bit(int frame)
{
    return frame < 40 ? (cs_bytes[frame >> 3] >> (frame & 7)) & 1 : 0;
}

int __not_in_flash_func(hstx_packet_set_audio_samples_cs)(hstx_packet_t *packet,
                                                          const audio_sample_t *samples,
                                                          int num_samples,
                                                          int frame_count)
{
    hstx_packet_init(packet);
    if (num_samples < 1) num_samples = 1;
    if (num_samples > 4) num_samples = 4;

    uint8_t sample_present = (1 << num_samples) - 1;
    uint8_t b_flags = 0;
    int temp_frame_count = frame_count;
    for (int i = 0; i < num_samples; i++) {
        if (temp_frame_count == 0) b_flags |= (1 << i);
        temp_frame_count = (temp_frame_count + 1) % 192;
    }

    packet->header[0] = 0x02;
    packet->header[1] = sample_present;
    packet->header[2] = b_flags << 4;
    compute_header_parity(packet);

    int fc = frame_count;
    for (int i = 0; i < num_samples; i++) {
        uint8_t *d = packet->subpacket[i];
        int16_t left = samples[i].left;
        int16_t right = samples[i].right;
        int c = channel_status_bit(fc);
        fc = (fc + 1) % 192;

        d[0] = 0x00;
        d[1] = left & 0xFF;
        d[2] = (left >> 8) & 0xFF;
        d[3] = 0x00;
        d[4] = right & 0xFF;
        d[5] = (right >> 8) & 0xFF;

        // Parity covers the 24-bit sample plus V, U, C bits (V=U=0 here).
        int p_left = (int)compute_parity3(d[1], d[2], 0) ^ c;
        int p_right = (int)compute_parity3(d[4], d[5], 0) ^ c;

        // Byte 6 layout: VL UL CL PL VR UR CR PR (bits 0..7).
        d[6] = (uint8_t)((c << 2) | (p_left << 3) | (c << 6) | (p_right << 7));
        compute_subpacket_parity(packet, i);
    }

    return temp_frame_count;
}

int __not_in_flash_func(hstx_packet_set_audio_samples)(hstx_packet_t *packet, const audio_sample_t *samples, int num_samples,
                                  int frame_count)
{
    hstx_packet_init(packet);
    if (num_samples < 1)
        num_samples = 1;
    if (num_samples > 4)
        num_samples = 4;

    uint8_t sample_present = (1 << num_samples) - 1;
    uint8_t b_flags = 0;

    int temp_frame_count = frame_count;
    for (int i = 0; i < num_samples; i++) {
        if (temp_frame_count == 0)
            b_flags |= (1 << i);
        temp_frame_count = (temp_frame_count + 1) % 192;
    }

    packet->header[0] = 0x02;
    packet->header[1] = sample_present;
    packet->header[2] = b_flags << 4;
    compute_header_parity(packet);

    for (int i = 0; i < num_samples; i++) {
        uint8_t *d = packet->subpacket[i];
        int16_t left = samples[i].left;
        int16_t right = samples[i].right;

        d[0] = 0x00;
        d[1] = left & 0xFF;
        d[2] = (left >> 8) & 0xFF;
        d[3] = 0x00;
        d[4] = right & 0xFF;
        d[5] = (right >> 8) & 0xFF;

        bool p_left = compute_parity3(d[1], d[2], 0);
        bool p_right = compute_parity3(d[4], d[5], 0);

        d[6] = (p_left << 3) | (p_right << 7);
        compute_subpacket_parity(packet, i);
    }

    return temp_frame_count;
}

static inline uint32_t make_hstx_word(uint16_t lane0, uint16_t lane1, uint16_t lane2)
{
    return (lane0 & 0x3FF) | ((lane1 & 0x3FF) << 10) | ((lane2 & 0x3FF) << 20);
}

static void __not_in_flash_func(encode_header_to_lane0)(const hstx_packet_t *packet, uint16_t *lane0, int hv, bool first_packet)
{
    int hv1 = hv | 0x08;
    if (!first_packet)
        hv = hv1;

    int idx = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t h = packet->header[i];
        lane0[idx++] = ter_c4[((h << 2) & 4) | hv];
        hv = hv1;
        lane0[idx++] = ter_c4[((h << 1) & 4) | hv];
        lane0[idx++] = ter_c4[(h & 4) | hv];
        lane0[idx++] = ter_c4[((h >> 1) & 4) | hv];
        lane0[idx++] = ter_c4[((h >> 2) & 4) | hv];
        lane0[idx++] = ter_c4[((h >> 3) & 4) | hv];
        lane0[idx++] = ter_c4[((h >> 4) & 4) | hv];
        lane0[idx++] = ter_c4[((h >> 5) & 4) | hv];
    }
}

static void __not_in_flash_func(encode_subpackets_to_lanes)(const hstx_packet_t *packet, uint16_t *lane1, uint16_t *lane2)
{
    for (int i = 0; i < 8; i++) {
        uint32_t v = (packet->subpacket[0][i] << 0) | (packet->subpacket[1][i] << 8) | (packet->subpacket[2][i] << 16) |
                     (packet->subpacket[3][i] << 24);
        uint32_t t = (v ^ (v >> 7)) & 0x00aa00aa;
        v = v ^ t ^ (t << 7);
        t = (v ^ (v >> 14)) & 0x0000cccc;
        v = v ^ t ^ (t << 14);

        lane1[(i * 4) + 0] = ter_c4[(v >> 0) & 0xF];
        lane1[(i * 4) + 1] = ter_c4[(v >> 16) & 0xF];
        lane1[(i * 4) + 2] = ter_c4[(v >> 4) & 0xF];
        lane1[(i * 4) + 3] = ter_c4[(v >> 20) & 0xF];

        lane2[(i * 4) + 0] = ter_c4[(v >> 8) & 0xF];
        lane2[(i * 4) + 1] = ter_c4[(v >> 24) & 0xF];
        lane2[(i * 4) + 2] = ter_c4[(v >> 12) & 0xF];
        lane2[(i * 4) + 3] = ter_c4[(v >> 28) & 0xF];
    }
}

void __not_in_flash_func(hstx_encode_data_island)(hstx_data_island_t *out, const hstx_packet_t *packet, bool vsync_active, bool hsync_active)
{
    // REVERTED: vsync_active/hsync_active indicate pulse region, not signal level
    // For 640x480 (negative polarity), pulse region means signal=0
    int hv = (vsync_active ? 0 : 2) | (hsync_active ? 0 : 1);
    uint16_t lane0[32];
    uint16_t lane1[32];
    uint16_t lane2[32];

    encode_header_to_lane0(packet, lane0, hv, true);
    encode_subpackets_to_lanes(packet, lane1, lane2);

    uint16_t gb_lane0 = ter_c4[0xC | hv];
    uint32_t guard_word = make_hstx_word(gb_lane0, GUARD_BAND_SYMBOL, GUARD_BAND_SYMBOL);

    out->words[0] = guard_word;
    out->words[1] = guard_word;
    for (int i = 0; i < 32; i++)
        out->words[i + 2] = make_hstx_word(lane0[i], lane1[i], lane2[i]);
    out->words[34] = guard_word;
    out->words[35] = guard_word;
}

static hstx_data_island_t null_islands[4];
static bool null_islands_initialized = false;

static void init_null_islands(void)
{
    if (null_islands_initialized)
        return;
    hstx_packet_t null_packet;
    hstx_packet_set_null(&null_packet);
    for (int vsync = 0; vsync < 2; vsync++) {
        for (int hsync = 0; hsync < 2; hsync++) {
            hstx_encode_data_island(&null_islands[(vsync * 2) + hsync], &null_packet, vsync, hsync);
        }
    }
    null_islands_initialized = true;
}

const uint32_t *hstx_get_null_data_island(bool vsync, bool hsync)
{
    init_null_islands();
    return null_islands[(vsync ? 2 : 0) | (hsync ? 1 : 0)].words;
}

// RP2350 LE014 alteration: fixed four-pair audio packets, bit-identical output.
// Original generic encoder remains the fallback; no queue/PCM/rate changes.
#if R8_LYNX_ENCODE
static const uint32_t __not_in_flash("le014") le_lanes[256]={
    0x29ca7000u,0x29c98c00u,0x29cb9000u,0x29cb8800u,0x29c5c400u,0x29c47800u,0x29c63800u,0x29c4f000u,
    0x29cb3000u,0x29c4e400u,0x29c67000u,0x29cb1800u,0x29ca3800u,0x29c9c400u,0x29c58c00u,0x29cb0c00u,
    0x263a7000u,0x26398c00u,0x263b9000u,0x263b8800u,0x2635c400u,0x26347800u,0x26363800u,0x2634f000u,
    0x263b3000u,0x2634e400u,0x26367000u,0x263b1800u,0x263a3800u,0x2639c400u,0x26358c00u,0x263b0c00u,
    0x2e4a7000u,0x2e498c00u,0x2e4b9000u,0x2e4b8800u,0x2e45c400u,0x2e447800u,0x2e463800u,0x2e44f000u,
    0x2e4b3000u,0x2e44e400u,0x2e467000u,0x2e4b1800u,0x2e4a3800u,0x2e49c400u,0x2e458c00u,0x2e4b0c00u,
    0x2e2a7000u,0x2e298c00u,0x2e2b9000u,0x2e2b8800u,0x2e25c400u,0x2e247800u,0x2e263800u,0x2e24f000u,
    0x2e2b3000u,0x2e24e400u,0x2e267000u,0x2e2b1800u,0x2e2a3800u,0x2e29c400u,0x2e258c00u,0x2e2b0c00u,
    0x171a7000u,0x17198c00u,0x171b9000u,0x171b8800u,0x1715c400u,0x17147800u,0x17163800u,0x1714f000u,
    0x171b3000u,0x1714e400u,0x17167000u,0x171b1800u,0x171a3800u,0x1719c400u,0x17158c00u,0x171b0c00u,
    0x11ea7000u,0x11e98c00u,0x11eb9000u,0x11eb8800u,0x11e5c400u,0x11e47800u,0x11e63800u,0x11e4f000u,
    0x11eb3000u,0x11e4e400u,0x11e67000u,0x11eb1800u,0x11ea3800u,0x11e9c400u,0x11e58c00u,0x11eb0c00u,
    0x18ea7000u,0x18e98c00u,0x18eb9000u,0x18eb8800u,0x18e5c400u,0x18e47800u,0x18e63800u,0x18e4f000u,
    0x18eb3000u,0x18e4e400u,0x18e67000u,0x18eb1800u,0x18ea3800u,0x18e9c400u,0x18e58c00u,0x18eb0c00u,
    0x13ca7000u,0x13c98c00u,0x13cb9000u,0x13cb8800u,0x13c5c400u,0x13c47800u,0x13c63800u,0x13c4f000u,
    0x13cb3000u,0x13c4e400u,0x13c67000u,0x13cb1800u,0x13ca3800u,0x13c9c400u,0x13c58c00u,0x13cb0c00u,
    0x2cca7000u,0x2cc98c00u,0x2ccb9000u,0x2ccb8800u,0x2cc5c400u,0x2cc47800u,0x2cc63800u,0x2cc4f000u,
    0x2ccb3000u,0x2cc4e400u,0x2cc67000u,0x2ccb1800u,0x2cca3800u,0x2cc9c400u,0x2cc58c00u,0x2ccb0c00u,
    0x139a7000u,0x13998c00u,0x139b9000u,0x139b8800u,0x1395c400u,0x13947800u,0x13963800u,0x1394f000u,
    0x139b3000u,0x1394e400u,0x13967000u,0x139b1800u,0x139a3800u,0x1399c400u,0x13958c00u,0x139b0c00u,
    0x19ca7000u,0x19c98c00u,0x19cb9000u,0x19cb8800u,0x19c5c400u,0x19c47800u,0x19c63800u,0x19c4f000u,
    0x19cb3000u,0x19c4e400u,0x19c67000u,0x19cb1800u,0x19ca3800u,0x19c9c400u,0x19c58c00u,0x19cb0c00u,
    0x2c6a7000u,0x2c698c00u,0x2c6b9000u,0x2c6b8800u,0x2c65c400u,0x2c647800u,0x2c663800u,0x2c64f000u,
    0x2c6b3000u,0x2c64e400u,0x2c667000u,0x2c6b1800u,0x2c6a3800u,0x2c69c400u,0x2c658c00u,0x2c6b0c00u,
    0x28ea7000u,0x28e98c00u,0x28eb9000u,0x28eb8800u,0x28e5c400u,0x28e47800u,0x28e63800u,0x28e4f000u,
    0x28eb3000u,0x28e4e400u,0x28e67000u,0x28eb1800u,0x28ea3800u,0x28e9c400u,0x28e58c00u,0x28eb0c00u,
    0x271a7000u,0x27198c00u,0x271b9000u,0x271b8800u,0x2715c400u,0x27147800u,0x27163800u,0x2714f000u,
    0x271b3000u,0x2714e400u,0x27167000u,0x271b1800u,0x271a3800u,0x2719c400u,0x27158c00u,0x271b0c00u,
    0x163a7000u,0x16398c00u,0x163b9000u,0x163b8800u,0x1635c400u,0x16347800u,0x16363800u,0x1634f000u,
    0x163b3000u,0x1634e400u,0x16367000u,0x163b1800u,0x163a3800u,0x1639c400u,0x16358c00u,0x163b0c00u,
    0x2c3a7000u,0x2c398c00u,0x2c3b9000u,0x2c3b8800u,0x2c35c400u,0x2c347800u,0x2c363800u,0x2c34f000u,
    0x2c3b3000u,0x2c34e400u,0x2c367000u,0x2c3b1800u,0x2c3a3800u,0x2c39c400u,0x2c358c00u,0x2c3b0c00u
};
static const uint32_t __not_in_flash("le014") le_headers[5][32]={
{
    0x000002e4u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x00000163u,0x00000163u,0x00000163u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x00000163u,0x0000019cu,0x00000163u,0x00000163u
},
{
    0x000002e4u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x00000163u,0x00000163u,0x00000163u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,
    0x00000163u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x00000163u,0x00000163u,0x0000019cu
},
{
    0x000002e4u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x00000163u,0x00000163u,0x00000163u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x00000163u,0x0000019cu,0x0000019cu,
    0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x00000163u,0x00000163u,0x0000019cu,0x00000163u
},
{
    0x000002e4u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x00000163u,0x00000163u,0x00000163u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x00000163u,0x0000019cu,
    0x0000019cu,0x00000163u,0x0000019cu,0x0000019cu,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu
},
{
    0x000002e4u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x00000163u,0x00000163u,0x00000163u,0x00000163u,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,
    0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x0000019cu,0x00000163u,
    0x00000163u,0x00000163u,0x0000019cu,0x0000019cu,0x00000163u,0x0000019cu,0x00000163u,0x0000019cu
}
};
static const uint8_t __not_in_flash("le014") le_parities[5]={208,99,177,18,83};
void __not_in_flash_func(hstx_encode_audio4_fused)(hstx_data_island_t *out,const hstx_packet_t *packet)
{
    unsigned h;
    switch(packet->header[2]){
        case 0:h=0;break;case 16:h=1;break;case 32:h=2;break;
        case 64:h=3;break;case 128:h=4;break;default:goto fallback;
    }
    if(packet->header[0]!=2 || packet->header[1]!=15 || packet->header[3]!=le_parities[h])goto fallback;
    const uint32_t *header=le_headers[h];
    out->words[0]=out->words[1]=out->words[34]=out->words[35]=0x1334cd63u;
    for(unsigned i=0;i<8;++i){
        uint32_t v=(uint32_t)packet->subpacket[0][i]|((uint32_t)packet->subpacket[1][i]<<8)|
            ((uint32_t)packet->subpacket[2][i]<<16)|((uint32_t)packet->subpacket[3][i]<<24);
        uint32_t t=(v^(v>>7))&0x00aa00aa;v^=t^(t<<7);
        t=(v^(v>>14))&0x0000cccc;v^=t^(t<<14);
        uint32_t lo=(v&0x000f000f)|((v>>4)&0x00f000f0);
        uint32_t hi=((v>>4)&0x000f000f)|((v>>8)&0x00f000f0);
        out->words[i*4+2]=header[i*4]+le_lanes[lo&255];
        out->words[i*4+3]=header[i*4+1]+le_lanes[lo>>16];
        out->words[i*4+4]=header[i*4+2]+le_lanes[hi&255];
        out->words[i*4+5]=header[i*4+3]+le_lanes[hi>>16];
    }
    return;
fallback:
    hstx_encode_data_island(out,packet,false,true);
}
#endif
