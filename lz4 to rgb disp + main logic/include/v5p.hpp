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
extern "C" {
    void vexDisplayCopyRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t *pSrc, int32_t srcStride);
}

class V5P {
public:
    void stop() { st_.store(true, std::memory_order_relaxed); }
    bool video(const char* fp, bool loop = true) {
        st_.store(false, std::memory_order_relaxed);

        // Wait for SD card to be ready
        uint32_t wait_start = pros::millis();
        while (!pros::usd::is_installed()) {
            pros::screen::set_pen(pros::Color::yellow);
            pros::screen::print(pros::E_TEXT_MEDIUM, 1, "V5P: Waiting for SD card...");
            pros::delay(200);
            if (pros::millis() - wait_start > 10000) {
                pros::screen::set_pen(pros::Color::red);
                pros::screen::print(pros::E_TEXT_MEDIUM, 1, "V5P: SD card not detected!");
                return false;
            }
        }

        FILE* f = nullptr;
        for (int retries = 0; retries < 20; ++retries) {
            f = std::fopen(fp, "rb");
            if (f) break;
            pros::delay(100);
        }
        if (!f) {
            pros::screen::set_pen(pros::Color::red);
            pros::screen::print(pros::E_TEXT_MEDIUM, 1, "V5P: Cannot open %s", fp);
            std::printf("V5P: could not open '%s'\n", fp);
            return false;
        }

        uint8_t H[16];
        if (std::fread(H, 1, 16, f) != 16) {
            pros::screen::set_pen(pros::Color::red);
            pros::screen::print(pros::E_TEXT_MEDIUM, 1, "V5P: '%s' too short", fp);
            std::printf("V5P: '%s' too short\n", fp);
            std::fclose(f);
            return false;
        }
        bool Z; int CF;
        if (!std::memcmp(H, "V5RU", 4)) { Z = false; CF = 0; }
        else if (!std::memcmp(H, "V5RZ", 4)) { Z = true; CF = 0; }
        else if (!std::memcmp(H, "V5YU", 4)) { Z = false; CF = 1; }
        else if (!std::memcmp(H, "V5YZ", 4)) { Z = true; CF = 1; }
        else {
            pros::screen::set_pen(pros::Color::red);
            pros::screen::print(pros::E_TEXT_MEDIUM, 1, "V5P: bad magic in '%s'", fp);
            std::printf("V5P: bad magic in '%s'\n", fp);
            std::fclose(f);
            return false;
        }
        uint16_t a=0,b=0,c=0;
        std::memcpy(&a,H+4,2); std::memcpy(&b,H+6,2); std::memcpy(&c,H+8,2);
        const int w=a, h=b; const uint32_t fr = c?c:30u;
        if (w<=0||h<=0||w>1024||h>1024) {
            pros::screen::set_pen(pros::Color::red);
            pros::screen::print(pros::E_TEXT_MEDIUM, 1, "V5P: bad size %dx%d", w, h);
            std::printf("V5P: bad size %dx%d\n",w,h);
            std::fclose(f);
            return false;
        }
        if (CF==1 && ((w&1)||(h&1))) {
            pros::screen::set_pen(pros::Color::red);
            pros::screen::print(pros::E_TEXT_MEDIUM, 1, "V5P: YUV420 needs even dims");
            std::printf("V5P: YUV420 needs even dims, got %dx%d\n",w,h);
            std::fclose(f);
            return false;
        }
        const std::size_t pc=(std::size_t)w*h, cw=(std::size_t)w/2, ch=(std::size_t)h/2;
        const std::size_t fs = CF==0 ? pc*3 : pc+cw*ch*2;
        const std::size_t mc = (std::size_t)LZ4_compressBound((int)fs);
        std::vector<uint8_t> buf(fs), cz; std::vector<uint32_t> px(pc);
        if (Z) cz.reserve(mc);
        const uint8_t *yp=buf.data(), *up=yp+pc, *vp=up+cw*ch;
        constexpr int SW = 480, SH = 272;
        const int dw=std::min(w,SW), dh=std::min(h,SH);
        const int dx=(SW-dw)/2, dy=(SH-dh)/2, sx=(w-dw)/2, sy=(h-dh)/2;
        uint32_t* const s = px.data() + (std::size_t)sy*w + sx;
        std::vector<uint32_t> canvas;
        if (dw != SW || dh != SH) {
            canvas.assign((std::size_t)SW * SH, 0);
        }
        uint32_t nf = pros::millis(), pa = 0; bool ok=true, stp=false;
        for (;;) {
            if (st_.load(std::memory_order_relaxed)) { stp=true; break; }
            if (Z) {
                uint32_t ps=0;
                if (std::fread(&ps,4,1,f)!=1) {
                    if (loop) {
                        std::fseek(f, 16, SEEK_SET);
                        continue;
                    }
                    break;
                }
                if (!ps||ps>mc) { std::printf("V5P: bad compressed size %lu\n",(unsigned long)ps); ok=false; break; }
                cz.resize(ps);
                if (std::fread(cz.data(),1,ps,f)!=ps) { std::printf("V5P: truncated frame\n"); ok=false; break; }
                int dc = LZ4_decompress_safe((const char*)cz.data(),(char*)buf.data(),(int)ps,(int)fs);
                if (dc<0 || (std::size_t)dc!=fs) { std::printf("V5P: LZ4 fail (%d)\n",dc); ok=false; break; }
            } else {
                if (std::fread(buf.data(),1,fs,f)!=fs) {
                    if (loop) {
                        std::fseek(f, 16, SEEK_SET);
                        continue;
                    }
                    break;
                }
            }
            if (CF==0) r2p(buf.data(),pc,px.data()); else y2p(yp,up,vp,w,h,px.data());

            if (dw == SW && dh == SH) {
                vexDisplayCopyRect(0, 0, SW, SH, px.data(), SW);
            } else {
                for (int row = 0; row < dh; ++row) {
                    std::memcpy(canvas.data() + (std::size_t)(dy + row) * SW + dx,
                                s + (std::size_t)row * w,
                                (std::size_t)dw * sizeof(uint32_t));
                }
                vexDisplayCopyRect(0, 0, SW, SH, canvas.data(), SW);
            }

            pa+=1000; const uint32_t pd=pa/fr; pa%=fr; nf+=pd;
            const int32_t wt=(int32_t)(nf-pros::millis());
            if (wt>0) pros::delay((uint32_t)wt); else { nf=pros::millis(); pros::delay(1); }
        }
        std::fclose(f);
        if (stp) pros::screen::erase();
        return ok;
    }
private:
    std::atomic<bool> st_{false};
    static inline uint8_t cl(int v) { return v<0?0:(v>255?255:(uint8_t)v); }
    static void r2p(const uint8_t* p, std::size_t n, uint32_t* o) {
        for (std::size_t i=0;i<n;++i,p+=3) o[i]=((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];
    }
    static void y2p(const uint8_t* Y, const uint8_t* U, const uint8_t* V, int w, int h, uint32_t* o) {
        const int cw=w/2;
        for (int j=0;j<h;++j) {
            const uint8_t *yr=Y+(std::size_t)j*w, *ur=U+(std::size_t)(j/2)*cw, *vr=V+(std::size_t)(j/2)*cw;
            uint32_t* or_=o+(std::size_t)j*w;
            for (int i=0;i<w;++i) {
                const int C=(int)yr[i]-16, D=(int)ur[i/2]-128, E=(int)vr[i/2]-128;
                or_[i] = ((uint32_t)cl((298*C+459*E+128)>>8)<<16) | ((uint32_t)cl((298*C-55*D-136*E+128)>>8)<<8) | (uint32_t)cl((298*C+541*D+128)>>8);
            }
        }
    }
};
