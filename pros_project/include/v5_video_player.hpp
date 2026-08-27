#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include "pros/rtos.hpp"

extern "C" void vexDisplayCopyRect(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                   const uint32_t* pBuffer, int32_t stride);

namespace v5_video {

const int DISPLAY_WIDTH  = 480;
const int DISPLAY_HEIGHT = 272;
const int MAX_FRAME_BYTES = DISPLAY_WIDTH * DISPLAY_HEIGHT * 3 / 2;

static inline int decompress_lz4(const uint8_t* src, uint8_t* dst, int src_len, int dst_len) {
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

static int32_t y_table[256];
static int32_t cr_r[256], cr_g[256], cb_g[256], cb_b[256];
static bool tables_initialized = false;

static void init_color_tables() {
    if (tables_initialized) return;
    for (int i = 0; i < 256; i++) {
        y_table[i] = 298 * (i - 16) + 128;
        cr_r[i]    =  459 * (i - 128);
        cr_g[i]    = -136 * (i - 128);
        cb_g[i]    =  -55 * (i - 128);
        cb_b[i]    =  541 * (i - 128);
    }
    tables_initialized = true;
}

static inline uint32_t clamp_color(int32_t value) {
    return static_cast<uint32_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

static uint8_t yuv_buffer[MAX_FRAME_BYTES];
static uint8_t compressed_buffer[MAX_FRAME_BYTES];
static uint32_t display_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];

static inline void decode_native_frame(int width, int height) {
    uint32_t y_size  = static_cast<uint32_t>(width * height);
    uint32_t uv_size = (width >> 1) * (height >> 1);
    const uint8_t* y_plane  = yuv_buffer;
    const uint8_t* cb_plane = yuv_buffer + y_size;
    const uint8_t* cr_plane = cb_plane + uv_size;

    for (int y = 0; y < height; y++) {
        int uv_row = (y >> 1) * (width >> 1);
        const uint8_t* y_row = &y_plane[y * width];
        uint32_t* display_row = display_buffer[y];

        for (int x = 0; x < width; x++) {
            int uv_index = uv_row + (x >> 1);
            int32_t Y  = y_table[y_row[x]];
            int32_t cb = cb_plane[uv_index];
            int32_t cr = cr_plane[uv_index];

            uint32_t r = clamp_color((Y + cr_r[cr]) >> 8);
            uint32_t g = clamp_color((Y + cr_g[cr] + cb_g[cb]) >> 8);
            uint32_t b = clamp_color((Y + cb_b[cb]) >> 8);

            display_row[x] = (r << 16) | (g << 8) | b;
        }
    }
}

inline bool play_video(const char* filepath) {
    FILE* video_file = nullptr;

    for (int retry = 0; retry < 20 && !video_file; retry++) {
        video_file = fopen(filepath, "rb");
        if (!video_file) pros::delay(100);
    }

    if (!video_file) {
        printf("Error: Could not open video file '%s'\n", filepath);
        return false;
    }

    uint8_t header[16];
    if (fread(header, 1, 16, video_file) < 16) {
        printf("Error: Invalid video header\n");
        fclose(video_file);
        return false;
    }

    bool is_lz4 = (header[0] == 'V' && header[1] == '5' && (header[2] == 'L' || header[2] == 'Z'));
    bool is_raw = (header[0] == 'V' && header[1] == '5' && header[2] == 'Y' && header[3] == 'U');

    if (!is_lz4 && !is_raw) {
        printf("Error: Unsupported video format magic\n");
        fclose(video_file);
        return false;
    }

    uint16_t width  = static_cast<uint16_t>(header[4] | (header[5] << 8));
    uint16_t height = static_cast<uint16_t>(header[6] | (header[7] << 8));
    uint16_t fps    = static_cast<uint16_t>(header[8] | (header[9] << 8));
    uint32_t frame_bytes = static_cast<uint32_t>(width * height * 3 / 2);
    uint32_t frame_delay_ms = (fps > 0) ? (1000u / fps) : 16u;

    init_color_tables();
    long start_position = ftell(video_file);

    while (true) {
        uint32_t start_time = pros::millis();

        if (is_lz4) {
            uint32_t comp_size = 0;
            if (fread(&comp_size, 1, 4, video_file) < 4 || comp_size > MAX_FRAME_BYTES ||
                fread(compressed_buffer, 1, comp_size, video_file) < comp_size) {
                fseek(video_file, start_position, SEEK_SET);
                continue;
            }

            if (decompress_lz4(compressed_buffer, yuv_buffer, comp_size, frame_bytes) != static_cast<int>(frame_bytes)) {
                fseek(video_file, start_position, SEEK_SET);
                continue;
            }
        } else {
            if (fread(yuv_buffer, 1, frame_bytes, video_file) < frame_bytes) {
                fseek(video_file, start_position, SEEK_SET);
                continue;
            }
        }

        decode_native_frame(width, height);

        vexDisplayCopyRect(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, &display_buffer[0][0], DISPLAY_WIDTH);

        uint32_t elapsed = pros::millis() - start_time;
        if (elapsed < frame_delay_ms) {
            pros::delay(frame_delay_ms - elapsed);
        }
    }

    fclose(video_file);
    return true;
}

} // namespace v5_video
