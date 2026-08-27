#!/usr/bin/env python3
"""
pack_yuv.py - Encodes video for VEX V5 (.v5y format with optional LZ4 compression).
"""
import subprocess
import struct
import sys
import os

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
        print("Warning: python 'lz4' module not installed. Falling back to raw YUV.")
        use_lz4 = False

    magic = b'V5LZ' if use_lz4 else b'V5YU'
    y_size     = src_w * src_h
    uv_size    = (src_w // 2) * (src_h // 2)
    frame_size = y_size + uv_size * 2

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

    print(f"Encoding '{input_file}' -> '{output_file}' ({src_w}x{src_h} @ {fps}fps)...")
    proc = subprocess.Popen(ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    frame_count = 0
    with open(output_file, 'wb') as f:
        flags = 1 if use_lz4 else 0
        f.write(magic)
        f.write(struct.pack('<HHHIH', src_w, src_h, fps, 0, flags))

        while True:
            raw = proc.stdout.read(frame_size)
            if len(raw) < frame_size:
                break

            if use_lz4:
                comp = lz4.block.compress(raw, store_size=False)
                f.write(struct.pack('<I', len(comp)))
                f.write(comp)
            else:
                f.write(raw)

            frame_count += 1
            if frame_count % 300 == 0:
                print(f"  Encoded {frame_count} frames...")

        f.seek(10)
        f.write(struct.pack('<I', frame_count))

    proc.wait()
    print(f"Done! Encoded {frame_count} frames to {output_file}.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input_video> <output.v5y> [width] [height] [fps] [--raw]")
        sys.exit(1)

    input_path  = sys.argv[1]
    output_path = sys.argv[2]
    w   = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] != "--raw" else DEFAULT_W
    h   = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] != "--raw" else DEFAULT_H
    fps = int(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5] != "--raw" else DEFAULT_FPS
    use_lz4 = "--raw" not in sys.argv

    pack(input_path, output_path, w, h, fps, use_lz4=use_lz4)
