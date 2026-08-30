#!/usr/bin/env python3
"""
pack_yuv_lz4.py - LZ4-compressed Raw YUV420p Video Encoder for VEX V5 Brain
Stores native YUV420p frames, each LZ4-block-compressed independently.
BT.709 limited-range math is applied directly on raw Y/Cb/Cr bytes in the
C++ decoder — this script does NOT touch color range/space.

Requires: pip install lz4

File format (.v5y):
  Header (16 bytes):
    [0-3]   Magic: 'V5YZ'
    [4-5]   uint16_t src_width  (e.g. 480)
    [6-7]   uint16_t src_height (e.g. 272)
    [8-9]   uint16_t fps
    [10-11] uint16_t reserved (0)
    [12-15] uint32_t frame_count

  Per frame:
    [0-3]   uint32_t compressed_size
    [4..]   compressed_size bytes of raw LZ4 block data
            (decompresses to exactly src_width * src_height * 3/2 bytes,
             laid out as Y plane, then Cb plane, then Cr plane)
"""
import subprocess
import struct
import sys
import os
import time

import lz4.block

SRC_W   = 480
SRC_H   = 272
FPS     = 60

def pack(input_file, output_file):
    print(f"[*] V5YZ Encoder (LZ4) — {input_file}")
    print(f"[*] Target: {SRC_W}x{SRC_H} @ {FPS}fps")

    # Probe actual dimensions so we can report them
    probe = subprocess.run([
        "ffprobe", "-v", "error", "-select_streams", "v:0",
        "-show_entries", "stream=width,height,r_frame_rate,color_range,color_space",
        "-of", "csv=s=x:p=0", input_file
    ], capture_output=True, text=True)
    print(f"[*] Source info: {probe.stdout.strip()}")

    # FFmpeg: decode -> scale to SRC_W x SRC_H -> output raw YUV420p
    # NOTE: We do NOT convert from limited range here.
    # The C++ decoder applies BT.709 limited-range math directly on raw Y/Cb/Cr bytes.
    ffmpeg_cmd = [
        "ffmpeg", "-y", "-i", input_file,
        "-vf", (
            f"scale={SRC_W}:{SRC_H}:force_original_aspect_ratio=decrease,"
            f"pad={SRC_W}:{SRC_H}:(ow-iw)/2:(oh-ih)/2:black,"
            f"fps={FPS}"
        ),
        "-pix_fmt", "yuv420p",
        "-f", "rawvideo", "pipe:1"
    ]

    y_size  = SRC_W * SRC_H
    uv_size = (SRC_W // 2) * (SRC_H // 2)
    frame_size = y_size + uv_size * 2

    print(f"[*] Raw frame size: {frame_size} bytes "
          f"({frame_size * FPS / 1024 / 1024:.2f} MB/s uncompressed at {FPS}fps)")
    print(f"[*] Decoding, compressing, and writing frames...")

    t0 = time.time()
    frame_count = 0
    raw_total = 0
    comp_total = 0

    proc = subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    with open(output_file, 'wb') as f:
        # Write placeholder header (frame_count patched at the end)
        f.write(b'V5YZ')
        f.write(struct.pack('<HHHHI', SRC_W, SRC_H, FPS, 0, 0))  # 16 bytes total

        while True:
            raw = proc.stdout.read(frame_size)
            if len(raw) < frame_size:
                break

            # store_size=False: pure LZ4 block, no embedded size header —
            # the C++ decoder already knows the decompressed size from w*h.
            compressed = lz4.block.compress(raw, mode='default', store_size=False)

            f.write(struct.pack('<I', len(compressed)))
            f.write(compressed)

            raw_total += frame_size
            comp_total += len(compressed)
            frame_count += 1
            if frame_count % 100 == 0:
                elapsed = time.time() - t0
                fps_cur = frame_count / elapsed
                ratio = raw_total / comp_total if comp_total else 0
                print(f"  [>] {frame_count} frames encoded ({fps_cur:.1f} fps, "
                      f"{ratio:.2f}x compression so far)")

        # Patch frame_count in header
        f.seek(12)
        f.write(struct.pack('<I', frame_count))

    proc.wait()
    elapsed = time.time() - t0
    fsize = os.path.getsize(output_file)
    ratio = raw_total / comp_total if comp_total else 0
    avg_frame_bytes = comp_total / frame_count if frame_count else 0

    print(f"[+] Done! {frame_count} frames in {elapsed:.1f}s")
    print(f"[+] Output: {output_file}  ({fsize / 1024 / 1024:.2f} MB)")
    print(f"[+] Compression ratio: {ratio:.2f}x  (raw {raw_total/1024/1024:.2f} MB -> "
          f"compressed {comp_total/1024/1024:.2f} MB)")
    print(f"[+] Avg compressed frame size: {avg_frame_bytes/1024:.1f} KB")
    print(f"[+] SD read bandwidth needed: {avg_frame_bytes * FPS / 1024 / 1024:.2f} MB/s @ {FPS}fps")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input_video> <output.v5y>")
        sys.exit(1)
    pack(sys.argv[1], sys.argv[2])
