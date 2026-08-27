#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "pros/rtos.hpp"

extern "C" void vexDisplayCopyRect(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                   const uint32_t* pBuffer, int32_t stride);

namespace v5_video {

static constexpr int DISPLAY_W = 480;
static constexpr int DISPLAY_H = 272;
static constexpr int MAX_FRAME_BYTES = DISPLAY_W * DISPLAY_H * 3 / 2;

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
    return static_cast<int>(op - dst);
}

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

static inline uint32_t clamp8(int32_t v) {
    return static_cast<uint32_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static uint8_t yuv_buf[MAX_FRAME_BYTES];
static uint8_t comp_buf[MAX_FRAME_BYTES];
static uint32_t frame_buf[DISPLAY_H][DISPLAY_W];

static inline void decode_frame_1to1(int w, int h) {
    uint32_t y_sz  = static_cast<uint32_t>(w * h);
    uint32_t uv_sz = (w >> 1) * (h >> 1);
    const uint8_t* y_ptr  = yuv_buf;
    const uint8_t* cb_ptr = yuv_buf + y_sz;
    const uint8_t* cr_ptr = cb_ptr + uv_sz;

    for (int sy = 0; sy < h; sy++) {
        int uv_row_off = (sy >> 1) * (w >> 1);
        const uint8_t* yrow = &y_ptr[sy * w];
        uint32_t* frow = frame_buf[sy];

        for (int sx = 0; sx < w; sx++) {
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

static inline void decode_frame_2x(int w, int h) {
    uint32_t y_sz  = static_cast<uint32_t>(w * h);
    uint32_t uv_sz = (w >> 1) * (h >> 1);
    const uint8_t* y_ptr  = yuv_buf;
    const uint8_t* cb_ptr = yuv_buf + y_sz;
    const uint8_t* cr_ptr = cb_ptr + uv_sz;

    for (int sy = 0; sy < h; sy++) {
        int dy0 = sy << 1;
        int dy1 = dy0 + 1;
        int uv_row_off = (sy >> 1) * (w >> 1);
        const uint8_t* yrow = &y_ptr[sy * w];

        for (int sx = 0; sx < w; sx++) {
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

inline bool play_video(const char* filepath) {
    FILE* file = nullptr;

    for (int i = 0; i < 20 && !file; i++) {
        file = fopen(filepath, "rb");
        if (!file) pros::delay(100);
    }

    if (!file) {
        printf("Error: cannot open video file '%s'\n", filepath);
        return false;
    }

    uint8_t hdr[16];
    if (fread(hdr, 1, 16, file) < 16) {
        printf("Error: invalid video header\n");
        fclose(file);
        return false;
    }

    bool is_lz4 = (hdr[0] == 'V' && hdr[1] == '5' && (hdr[2] == 'L' || hdr[2] == 'Z') && (hdr[3] == 'Z' || hdr[3] == '6'));
    bool is_raw = (hdr[0] == 'V' && hdr[1] == '5' && hdr[2] == 'Y' && hdr[3] == 'U');

    if (!is_lz4 && !is_raw) {
        printf("Error: unsupported format magic\n");
        fclose(file);
        return false;
    }

    uint16_t src_w   = static_cast<uint16_t>(hdr[4] | (hdr[5] << 8));
    uint16_t src_h   = static_cast<uint16_t>(hdr[6] | (hdr[7] << 8));
    uint16_t fps     = static_cast<uint16_t>(hdr[8] | (hdr[9] << 8));
    uint32_t frame_bytes = static_cast<uint32_t>(src_w * src_h * 3 / 2);
    uint32_t delay_ms    = (fps > 0) ? (1000u / fps) : 16u;

    init_lut();
    long data_start = ftell(file);
    bool is_native = (src_w == DISPLAY_W && src_h == DISPLAY_H);

    while (true) {
        uint32_t t0 = pros::millis();

        if (is_lz4) {
            uint32_t comp_size = 0;
            if (fread(&comp_size, 1, 4, file) < 4 || comp_size > MAX_FRAME_BYTES ||
                fread(comp_buf, 1, comp_size, file) < comp_size) {
                fseek(file, data_start, SEEK_SET);
                continue;
            }

            if (lz4_decompress(comp_buf, yuv_buf, comp_size, frame_bytes) != static_cast<int>(frame_bytes)) {
                fseek(file, data_start, SEEK_SET);
                continue;
            }
        } else {
            if (fread(yuv_buf, 1, frame_bytes, file) < frame_bytes) {
                fseek(file, data_start, SEEK_SET);
                continue;
            }
        }

        if (is_native) {
            decode_frame_1to1(src_w, src_h);
        } else {
            decode_frame_2x(src_w, src_h);
        }

        vexDisplayCopyRect(0, 0, DISPLAY_W - 1, DISPLAY_H - 1, &frame_buf[0][0], DISPLAY_W);

        uint32_t elapsed = pros::millis() - t0;
        if (elapsed < delay_ms) {
            pros::delay(delay_ms - elapsed);
        }
    }

    fclose(file);
    return true;
}

} // namespace v5_video
