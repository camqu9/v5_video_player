#pragma once

#include "pros/screen.hpp"
#include "pros/rtos.hpp"
#include "liblvgl/libs/lz4/lz4.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <atomic>

// V5R format: raw interleaved RGB24 frames (no chroma subsampling, no
// color-matrix math). Optionally LZ4-compressed per frame.
class V5P {
public:
    // Request the currently-playing video() (running on another task) to
    // stop. Takes effect within about one frame period, then blanks the
    // screen to its default (eraser) color.
    void stop() {
        stop_requested_.store(true, std::memory_order_relaxed);
    }

    bool video(const char* filepath) {
        stop_requested_.store(false, std::memory_order_relaxed);

        FILE* fp = std::fopen(filepath, "rb");
        if (fp == nullptr) {
            std::printf("V5P: could not open '%s'\n", filepath);
            return false;
        }

        uint8_t head[HEADER_SIZE];
        if (std::fread(head, 1, HEADER_SIZE, fp) != HEADER_SIZE) {
            std::printf("V5P: '%s' is too short to hold a header\n", filepath);
            std::fclose(fp);
            return false;
        }

        bool compressed;
        if (std::memcmp(head, "V5RZ", 4) == 0) {
            compressed = true;
        } else if (std::memcmp(head, "V5RU", 4) == 0) {
            compressed = false;
        } else {
            std::printf("V5P: '%s' has an unrecognised magic\n", filepath);
            std::fclose(fp);
            return false;
        }

        uint16_t hw = 0, hh = 0, hfps = 0;
        std::memcpy(&hw, head + 4, sizeof(hw));
        std::memcpy(&hh, head + 6, sizeof(hh));
        std::memcpy(&hfps, head + 8, sizeof(hfps));

        const int w = static_cast<int>(hw);
        const int h = static_cast<int>(hh);
        const uint32_t fps = hfps ? static_cast<uint32_t>(hfps) : 30u;

        if (w <= 0 || h <= 0 || w > MAX_DIM || h > MAX_DIM) {
            std::printf("V5P: unusable frame size %dx%d\n", w, h);
            std::fclose(fp);
            return false;
        }

        const std::size_t pixel_count = static_cast<std::size_t>(w) * h;
        const std::size_t frame_size = pixel_count * 3; // R,G,B interleaved
        const std::size_t max_compressed =
            static_cast<std::size_t>(LZ4_compressBound(static_cast<int>(frame_size)));

        std::vector<uint8_t> rgb24(frame_size);
        std::vector<uint8_t> comp;
        std::vector<uint32_t> rgb(pixel_count);
        if (compressed) {
            comp.reserve(max_compressed);
        }

        const int draw_w = std::min(w, SCREEN_W);
        const int draw_h = std::min(h, SCREEN_H);
        const int dst_x = SCREEN_LEFT + (SCREEN_W - draw_w) / 2;
        const int dst_y = SCREEN_TOP + (SCREEN_H - draw_h) / 2;
        const int src_x = (w - draw_w) / 2;
        const int src_y = (h - draw_h) / 2;
        uint32_t* const src = rgb.data() + static_cast<std::size_t>(src_y) * w + src_x;

        uint32_t next_frame = pros::millis();
        uint32_t period_acc = 0;
        bool ok = true;

        bool stopped = false;

        for (;;) {
            if (stop_requested_.load(std::memory_order_relaxed)) {
                stopped = true;
                break;
            }

            if (compressed) {
                uint32_t packed_size = 0;
                if (std::fread(&packed_size, sizeof(packed_size), 1, fp) != 1) {
                    break;
                }
                if (packed_size == 0 || packed_size > max_compressed) {
                    std::printf("V5P: implausible compressed size %lu\n",
                                static_cast<unsigned long>(packed_size));
                    ok = false;
                    break;
                }

                comp.resize(packed_size);
                if (std::fread(comp.data(), 1, packed_size, fp) != packed_size) {
                    std::printf("V5P: truncated frame\n");
                    ok = false;
                    break;
                }

                const int decoded = LZ4_decompress_safe(
                    reinterpret_cast<const char*>(comp.data()),
                    reinterpret_cast<char*>(rgb24.data()),
                    static_cast<int>(packed_size),
                    static_cast<int>(frame_size));

                if (decoded < 0 || static_cast<std::size_t>(decoded) != frame_size) {
                    std::printf("V5P: LZ4 decompress failed (ret=%d, expected %lu)\n",
                                decoded, static_cast<unsigned long>(frame_size));
                    ok = false;
                    break;
                }
            } else {
                if (std::fread(rgb24.data(), 1, frame_size, fp) != frame_size) {
                    break;
                }
            }

            rgb24_to_packed(rgb24.data(), pixel_count, rgb.data());

            pros::screen::copy_area(static_cast<int16_t>(dst_x),
                                    static_cast<int16_t>(dst_y),
                                    static_cast<int16_t>(dst_x + draw_w - 1),
                                    static_cast<int16_t>(dst_y + draw_h - 1),
                                    src,
                                    static_cast<int32_t>(w));

            period_acc += 1000;
            const uint32_t period = period_acc / fps;
            period_acc %= fps;

            next_frame += period;
            const int32_t wait = static_cast<int32_t>(next_frame - pros::millis());
            if (wait > 0) {
                pros::delay(static_cast<uint32_t>(wait));
            } else {
                next_frame = pros::millis();
                pros::delay(1);
            }
        }

        std::fclose(fp);

        if (stopped) {
            pros::screen::erase();
        }

        return ok;
    }

private:
    static constexpr int HEADER_SIZE = 16;
    static constexpr int MAX_DIM = 1024;

    static constexpr int SCREEN_LEFT = 0;
    static constexpr int SCREEN_TOP = 0;
    static constexpr int SCREEN_W = 480;
    static constexpr int SCREEN_H = 272;

    std::atomic<bool> stop_requested_{false};

    // Straight byte-triplet -> packed 0x00RRGGBB, no math, no clamping needed
    // since the bytes are already 0-255.
    static void rgb24_to_packed(const uint8_t* rgb24, std::size_t pixel_count, uint32_t* out) {
        const uint8_t* p = rgb24;
        for (std::size_t i = 0; i < pixel_count; ++i) {
            const uint32_t r = p[0];
            const uint32_t g = p[1];
            const uint32_t b = p[2];
            out[i] = (r << 16) | (g << 8) | b;
            p += 3;
        }
    }
};
