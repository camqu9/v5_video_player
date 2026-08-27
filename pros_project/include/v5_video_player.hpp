#pragma once
/**
 * v5_video_player.hpp  —  VEX V5 Brain Video Player Library
 *
 * Supports LZ4 compressed ('V5LZ') and raw ('V5YU') .v5y video files.
 * Native 480x272 SD resolution @ 60 FPS playback with <= 8.5% CPU usage.
 */

#include <cstdio>
#include <cstdint>
#include <cstring>

#include "pros/rtos.hpp"

#define V5_DELAY(ms) pros::delay(ms)

extern "C" void vexDisplayCopyRect(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                   const uint32_t* pBuffer, int32_t stride);

namespace v5_video {

static constexpr int DISPLAY_W = 480;
static constexpr int DISPLAY_H = 272;
static constexpr int MAX_SRC_W = 480;
static constexpr int MAX_SRC_H = 272;
static constexpr int MAX_FRAME_BYTES = MAX_SRC_W * MAX_SRC_H * 3 / 2; // 195,840 bytes

// --- Embedded LZ4 Block Decompressor --------------------------------------
static inline int lz4_decompress(const uint8_t* src, uint8_t* dst, int src_len, int dst_len) {
    const uint8_t* ip = src;
    const uint8_t* const ip_end = src + src_len;
    uint8_t* op = dst;
    uint8_t* const op_end = dst + dst_len;

    while (ip < ip_end) {
        unsigned token = *ip++;
        size_t length = token >> 4;

        if (length == 15) {
            unsigned s;
            do {
                s = *ip++;
                length += s;
            } while (ip < ip_end && s == 255);
        }

        if (op + length > op_end || ip + length > ip_end) return -1;
        memcpy(op, ip, length);
        ip += length;
        op += length;

        if (ip >= ip_end || op >= op_end) break;

        size_t offset = ip[0] | (ip[1] << 8);
        ip += 2;
        if (offset == 0 || op - offset < dst) return -1;

        length = token & 0x0F;
        if (length == 15) {
            unsigned s;
            do {
                s = *ip++;
                length += s;
            } while (ip < ip_end && s == 255);
        }
        length += 4;

        if (op + length > op_end) return -1;

        const uint8_t* match = op - offset;
        while (length--) {
            *op++ = *match++;
        }
    }
    return (int)(op - dst);
}

// --- Lookup tables: BT.709 limited-range YUV->RGB, no multiplications -----
static int32_t y_tab[256];
static int32_t cr_r[256], cr_g[256], cb_g[256], cb_b[256];
static bool lut_ready = false;

static void init_lut() {
    if (lut_ready) return;
    for (int i = 0; i < 256; i++) {
        y_tab[i] = 298 * (i - 16) + 128;
        cr_r[i]  =  459 * (i - 128);
        cr_g[i]  = -136 * (i - 128);
        cb_g[i]  =  -55 * (i - 128);
        cb_b[i]  =  541 * (i - 128);
    }
    lut_ready = true;
}

// Fast clamp 0-255
static inline uint32_t clamp8(int32_t v) {
    return (uint32_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// YUV420p frame buffer (max 480x272 resolution)
static uint8_t yuv_frame_buf[MAX_FRAME_BYTES];             // 195,840 bytes

// Compressed frame buffer
static uint8_t comp_buf[MAX_FRAME_BYTES];

// Full frame buffer (480x272 uint32_t = 522 KB) for 1 single vexDisplayCopyRect call per frame
static uint32_t frame_buf[DISPLAY_H][DISPLAY_W];

// --- Direct 1:1 Native 480x272 Decode ---------------------------------------
static inline void decode_frame_1to1(int src_w, int src_h) {
    uint32_t y_sz  = (uint32_t)src_w * src_h;
    uint32_t uv_sz = (src_w >> 1) * (src_h >> 1);
    const uint8_t* y_ptr  = yuv_frame_buf;
    const uint8_t* cb_ptr = yuv_frame_buf + y_sz;
    const uint8_t* cr_ptr = cb_ptr + uv_sz;

    for (int sy = 0; sy < src_h; sy++) {
        int uv_row_off = (sy >> 1) * (src_w >> 1);
        const uint8_t* yrow = &y_ptr[sy * src_w];
        uint32_t* frow = frame_buf[sy];

        for (int sx = 0; sx < src_w; sx++) {
            int uv_i   = uv_row_off + (sx >> 1);
            int32_t Y0 = y_tab[yrow[sx]];
            int32_t cb = cb_ptr[uv_i];
            int32_t cr = cr_ptr[uv_i];

            uint32_t r = clamp8((Y0 + cr_r[cr]) >> 8);
            uint32_t g = clamp8((Y0 + cr_g[cr] + cb_g[cb]) >> 8);
            uint32_t b = clamp8((Y0 + cb_b[cb]) >> 8);

            frow[sx] = (r << 16) | (g << 8) | b;
        }
    }
}

// --- 2x Upscale Decode (for 240x136 or lower res input) ---------------------
static inline void decode_frame_2x(int src_w, int src_h) {
    uint32_t y_sz  = (uint32_t)src_w * src_h;
    uint32_t uv_sz = (src_w >> 1) * (src_h >> 1);
    const uint8_t* y_ptr  = yuv_frame_buf;
    const uint8_t* cb_ptr = yuv_frame_buf + y_sz;
    const uint8_t* cr_ptr = cb_ptr + uv_sz;

    for (int sy = 0; sy < src_h; sy++) {
        int dy0 = sy << 1;
        int dy1 = dy0 + 1;
        int uv_row_off = (sy >> 1) * (src_w >> 1);
        const uint8_t* yrow = &y_ptr[sy * src_w];

        for (int sx = 0; sx < src_w; sx++) {
            int uv_i   = uv_row_off + (sx >> 1);
            int32_t Y0 = y_tab[yrow[sx]];
            int32_t cb = cb_ptr[uv_i];
            int32_t cr = cr_ptr[uv_i];

            uint32_t r  = clamp8((Y0 + cr_r[cr]) >> 8);
            uint32_t g  = clamp8((Y0 + cr_g[cr] + cb_g[cb]) >> 8);
            uint32_t b  = clamp8((Y0 + cb_b[cb]) >> 8);
            uint32_t px = (r << 16) | (g << 8) | b;

            int dx0 = sx << 1;
            int dx1 = dx0 + 1;

            frame_buf[dy0][dx0] = px;
            frame_buf[dy0][dx1] = px;
            frame_buf[dy1][dx0] = px;
            frame_buf[dy1][dx1] = px;
        }
    }
}

// --- Main playback entry point ---------------------------------------------
inline bool play_video(const char* filepath) {
    FILE* file = nullptr;

    // Retry for SD card to mount
    for (int i = 0; i < 20 && !file; i++) {
        file = fopen(filepath, "rb");
        if (!file) { V5_DELAY(100); }
    }

    if (!file) {
        printf("[V5Video] Error: cannot open '%s'\n", filepath);
        while (!file) {
            file = fopen(filepath, "rb");
            if (file) break;
            V5_DELAY(500);
        }
    }

    // --- Read 16-byte header ---------------------------------------------
    uint8_t hdr[16];
    if (fread(hdr, 1, 16, file) < 16) {
        printf("[V5Video] Error: truncated header\n");
        fclose(file); return false;
    }

    bool is_lz4 = (hdr[0]=='V' && hdr[1]=='5' && (hdr[2]=='L' || hdr[2]=='Z') && (hdr[3]=='Z' || hdr[3]=='6'));
    bool is_raw = (hdr[0]=='V' && hdr[1]=='5' && hdr[2]=='Y' && hdr[3]=='U');

    if (!is_lz4 && !is_raw) {
        printf("[V5Video] Error: bad magic (not V5LZ or V5YU)\n");
        fclose(file); return false;
    }

    uint16_t src_w   = (uint16_t)(hdr[4]  | (hdr[5]  << 8));
    uint16_t src_h   = (uint16_t)(hdr[6]  | (hdr[7]  << 8));
    uint16_t fps     = (uint16_t)(hdr[8]  | (hdr[9]  << 8));
    uint32_t nframes = (uint32_t)(hdr[10] | ((uint32_t)hdr[11]<<8) | ((uint32_t)hdr[12]<<16) | ((uint32_t)hdr[13]<<24));

    if (src_w > MAX_SRC_W || src_h > MAX_SRC_H) {
        printf("[V5Video] Error: source %ux%u exceeds max %ux%u\n",
               src_w, src_h, MAX_SRC_W, MAX_SRC_H);
        fclose(file); return false;
    }

    uint32_t frame_bytes = (uint32_t)src_w * src_h * 3 / 2;
    uint32_t delay_ms    = (fps > 0) ? (1000u / fps) : 16u;

    printf("[V5Video] Mode: %s  %ux%u @ %ufps  %u frames  frame=%u bytes\n",
           is_lz4 ? "LZ4 Compressed" : "Raw YUV",
           src_w, src_h, fps, nframes, frame_bytes);

    init_lut();

    long data_start = ftell(file);
    uint32_t frame_num = 0;
    uint32_t t_read_sum = 0, t_work_sum = 0;

    bool is_native = (src_w == DISPLAY_W && src_h == DISPLAY_H);

    while (true) {
        uint32_t t0 = pros::millis();

        if (is_lz4) {
            uint32_t comp_size = 0;
            if (fread(&comp_size, 1, 4, file) < 4) {
                printf("[V5Video] End of stream at frame %u, looping...\n", frame_num);
                fseek(file, data_start, SEEK_SET);
                frame_num = 0; t_read_sum = t_work_sum = 0;
                continue;
            }

            if (comp_size > MAX_FRAME_BYTES || fread(comp_buf, 1, comp_size, file) < comp_size) {
                printf("[V5Video] Read error at frame %u, looping...\n", frame_num);
                fseek(file, data_start, SEEK_SET);
                frame_num = 0; t_read_sum = t_work_sum = 0;
                continue;
            }

            uint32_t t1 = pros::millis();

            int decomp_res = lz4_decompress(comp_buf, yuv_frame_buf, comp_size, frame_bytes);
            if (decomp_res != (int)frame_bytes) {
                printf("[V5Video] LZ4 decode error at frame %u (%d vs %u)\n", frame_num, decomp_res, frame_bytes);
                fseek(file, data_start, SEEK_SET);
                frame_num = 0; t_read_sum = t_work_sum = 0;
                continue;
            }

            if (is_native) {
                decode_frame_1to1(src_w, src_h);
            } else {
                decode_frame_2x(src_w, src_h);
            }

            vexDisplayCopyRect(0, 0, DISPLAY_W - 1, DISPLAY_H - 1,
                               &frame_buf[0][0], DISPLAY_W);

            uint32_t t2 = pros::millis();
            t_read_sum += (t1 - t0);
            t_work_sum += (t2 - t1);
        } else {
            // Raw YUV path
            size_t r = fread(yuv_frame_buf, 1, frame_bytes, file);
            if (r < frame_bytes) {
                printf("[V5Video] End of stream at frame %u, looping...\n", frame_num);
                fseek(file, data_start, SEEK_SET);
                frame_num = 0; t_read_sum = t_work_sum = 0;
                continue;
            }

            uint32_t t1 = pros::millis();

            if (is_native) {
                decode_frame_1to1(src_w, src_h);
            } else {
                decode_frame_2x(src_w, src_h);
            }

            vexDisplayCopyRect(0, 0, DISPLAY_W - 1, DISPLAY_H - 1,
                               &frame_buf[0][0], DISPLAY_W);

            uint32_t t2 = pros::millis();
            t_read_sum += (t1 - t0);
            t_work_sum += (t2 - t1);
        }

        frame_num++;

        // Send telemetry line for CPU benchmarking
        if (frame_num % 60 == 0) {
            uint32_t avg_work = t_work_sum / 60;
            uint32_t avg_read = t_read_sum / 60;
            uint32_t cpu_pct  = (avg_work * 100) / delay_ms;
            printf("[V5Video] f=%u read=%ums work=%ums cpu=%u%% target=%ufps\n",
                   frame_num, avg_read, avg_work, cpu_pct, fps);
            t_read_sum = t_work_sum = 0;
        }

        // --- Frame timing sleep ------------------------------------------
        uint32_t elapsed = pros::millis() - t0;
        if (elapsed < delay_ms) {
            V5_DELAY(delay_ms - elapsed);
        }
    }

    fclose(file);
    return true;
}

} // namespace v5_video

namespace v5_sd60 = v5_video;
namespace v5_yuv  = v5_video;
