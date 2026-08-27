#!/usr/bin/env python3
"""
pack_yuv.py - LZ4 Compressed YUV420p Video Encoder for VEX V5 Brain
Encodes video for native SD 480x272 resolution (or 240x136) at up to 60 FPS using LZ4 block compression.

File format (.v5y):
  Header (16 bytes):
    [0-3]   Magic: 'V5LZ' (LZ4 compressed) or 'V5YU' (raw)
    [4-5]   uint16_t src_width  (default 480)
    [6-7]   uint16_t src_height (default 272)
    [8-9]   uint16_t fps        (default 60)
    [10-13] uint32_t frame_count
    [14-15] uint16_t flags      (0x01 = LZ4)

  Per frame (when LZ4 compressed):
    [0-3]   uint32_t comp_size
    [4..]   uint8_t comp_bytes[comp_size]
"""
import subprocess
import struct
import sys
import os
import time

try:
    import lz4.block
    HAS_LZ4 = True
except ImportError:
    HAS_LZ4 = False

DEFAULT_W   = 480
DEFAULT_H   = 272
DEFAULT_FPS = 60

def pack(input_file, output_file, src_w=DEFAULT_W, src_h=DEFAULT_H, fps=DEFAULT_FPS, use_lz4=True):
    if use_lz4 and not HAS_LZ4:
        print("[!] Warning: python 'lz4' package not installed, falling back to raw V5YU.")
        use_lz4 = False

    magic = b'V5LZ' if use_lz4 else b'V5YU'
    mode_str = "LZ4 Compressed" if use_lz4 else "Raw YUV"

    print(f"[*] V5 Video Encoder ({mode_str}) — {input_file}")
    print(f"[*] Target: {src_w}x{src_h} @ {fps}fps")

    # Probe actual dimensions
    probe = subprocess.run([
        "ffprobe", "-v", "error", "-select_streams", "v:0",
        "-show_entries", "stream=width,height,r_frame_rate,color_range,color_space",
        "-of", "csv=s=x:p=0", input_file
    ], capture_output=True, text=True)
    print(f"[*] Source info: {probe.stdout.strip()}")

    # FFmpeg: decode -> scale to src_w x src_h -> output raw YUV420p
    ffmpeg_cmd = [
        "ffmpeg", "-y", "-i", input_file,
        "-vf", (
            f"scale={src_w}:{src_h}:force_original_aspect_ratio=decrease,"
            f"pad={src_w}:{src_h}:(ow-iw)/2:(oh-ih)/2:black,"
            f"fps={fps}"
        ),
        "-pix_fmt", "yuv420p",
        "-f", "rawvideo", "pipe:1"
    ]

    y_size     = src_w * src_h
    uv_size    = (src_w // 2) * (src_h // 2)
    frame_size = y_size + uv_size * 2

    print(f"[*] Raw frame size: {frame_size} bytes ({frame_size * fps / 1024 / 1024:.2f} MB/s uncompressed)")
    print(f"[*] Encoding frames...")

    t0 = time.time()
    frame_count = 0
    total_comp_bytes = 0

    proc = subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    with open(output_file, 'wb') as f:
        # Write placeholder 16-byte header
        # Magic (4B), width (2B), height (2B), fps (2B), frame_count (4B), flags (2B)
        flags = 1 if use_lz4 else 0
        f.write(magic)
        f.write(struct.pack('<HHHIH', src_w, src_h, fps, 0, flags))

        while True:
            raw = proc.stdout.read(frame_size)
            if len(raw) < frame_size:
                break

            if use_lz4:
                comp = lz4.block.compress(raw, store_size=False)
                comp_len = len(comp)
                f.write(struct.pack('<I', comp_len))
                f.write(comp)
                total_comp_bytes += comp_len + 4
            else:
                f.write(raw)
                total_comp_bytes += frame_size

            frame_count += 1
            if frame_count % 100 == 0:
                elapsed = time.time() - t0
                fps_cur = frame_count / elapsed
                ratio = (total_comp_bytes / (frame_count * frame_size)) * 100 if use_lz4 else 100
                print(f"  [>] {frame_count} frames encoded ({fps_cur:.1f} fps speed, size={ratio:.1f}%)")

        # Patch frame_count in header (byte index 10)
        f.seek(10)
        f.write(struct.pack('<I', frame_count))

    proc.wait()
    elapsed = time.time() - t0
    fsize = os.path.getsize(output_file)
    orig_size = frame_count * frame_size
    ratio = (fsize / orig_size) * 100 if orig_size > 0 else 100
    sd_bandwidth = (fsize / frame_count * fps / (1024 * 1024)) if frame_count > 0 else 0

    print(f"[+] Done! {frame_count} frames in {elapsed:.1f}s")
    print(f"[+] Output: {output_file} ({fsize / 1024 / 1024:.2f} MB)")
    print(f"[+] Compression: {100 - ratio:.1f}% reduction (compressed to {ratio:.1f}% of original)")
    print(f"[+] SD card read speed needed: {sd_bandwidth:.2f} MB/s @ {fps}fps")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input_video> <output.v5y> [width={DEFAULT_W}] [height={DEFAULT_H}] [fps={DEFAULT_FPS}] [--raw]")
        sys.exit(1)

    input_path  = sys.argv[1]
    output_path = sys.argv[2]
    w   = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] != "--raw" else DEFAULT_W
    h   = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] != "--raw" else DEFAULT_H
    fps = int(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5] != "--raw" else DEFAULT_FPS
    use_lz4 = "--raw" not in sys.argv

    pack(input_path, output_path, w, h, fps, use_lz4=use_lz4)
