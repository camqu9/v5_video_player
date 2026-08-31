#!/usr/bin/env python3
"""
decode_v5y.py - Reference decoder for .v5y (V5YZ) files.

This exists purely to sanity-check pack_yuv_lz4.py + V5P.hpp on a desktop
before trusting them on a V5 Brain: it parses the exact same header/frame
layout and uses the exact same integer BT.709 limited-range math as
V5P.hpp's yuv420_to_rgb_bt709(), so what you see here is what the brain
should show (module a real screen's color response).

Requires: pip install lz4 numpy opencv-python

Usage:
  Play back in a window:
    python3 decode_v5y.py input.v5y --play

  Dump a single frame as a PNG (e.g. frame 0):
    python3 decode_v5y.py input.v5y --frame 0 --out frame0.png

  Re-encode to a normal .mp4 you can open anywhere (visual diff vs source):
    python3 decode_v5y.py input.v5y --out check.mp4
"""
import argparse
import struct
import sys

import numpy as np
import lz4.block

try:
    import cv2
except ImportError:
    cv2 = None

SUPPORTED_MAGICS = {
    b"V5YZ": {"format": "yuv", "compressed": True, "desc": "YUV420 + LZ4"},
    b"V5YU": {"format": "yuv", "compressed": False, "desc": "YUV420 Raw"},
    b"V5RZ": {"format": "rgb", "compressed": True, "desc": "RGB24 + LZ4"},
    b"V5RU": {"format": "rgb", "compressed": False, "desc": "RGB24 Raw"},
}
HEADER_STRUCT = "<4sHHHHI"  # magic, width, height, fps, reserved, frame_count
HEADER_SIZE = struct.calcsize(HEADER_STRUCT)


def read_header(f):
    raw = f.read(HEADER_SIZE)
    if len(raw) != HEADER_SIZE:
        raise ValueError("file too short to contain a header")
    magic, width, height, fps, reserved, frame_count = struct.unpack(HEADER_STRUCT, raw)
    if magic not in SUPPORTED_MAGICS:
        valid = ", ".join(m.decode() for m in SUPPORTED_MAGICS.keys())
        raise ValueError(f"bad magic {magic!r}, expected one of [{valid}]")
    info = SUPPORTED_MAGICS[magic]
    return {
        "magic": magic.decode("ascii", errors="replace"),
        "format": info["format"],
        "compressed": info["compressed"],
        "desc": info["desc"],
        "width": width,
        "height": height,
        "fps": fps,
        "reserved": reserved,
        "frame_count": frame_count,
    }


def read_frames(f, width, height, compressed=True, fmt="yuv"):
    """Yields decompressed or raw frame bytes."""
    if fmt == "rgb":
        frame_size = width * height * 3
    else:
        y_size = width * height
        c_size = (width // 2) * (height // 2)
        frame_size = y_size + 2 * c_size

    while True:
        if compressed:
            size_raw = f.read(4)
            if len(size_raw) < 4:
                return  # clean EOF
            (compressed_size,) = struct.unpack("<I", size_raw)
            if compressed_size == 0 or compressed_size > (1 << 28):
                raise ValueError(f"implausible compressed frame size: {compressed_size}")

            comp = f.read(compressed_size)
            if len(comp) != compressed_size:
                raise ValueError("unexpected EOF reading frame data")

            raw = lz4.block.decompress(comp, uncompressed_size=frame_size)
            if len(raw) != frame_size:
                raise ValueError(f"decompressed size mismatch: got {len(raw)}, expected {frame_size}")

            yield raw
        else:
            raw = f.read(frame_size)
            if len(raw) < frame_size:
                return  # clean EOF
            yield raw


def i420_to_rgb_bt709(raw, width, height):
    """
    Mirrors V5P.hpp's yuv420_to_rgb_bt709() exactly (same integer fixed-point
    coefficients: 298 / 459 / 55 / 136 / 541, >>8), so the two should agree
    pixel-for-pixel modulo the C++ side's per-pixel clamp_u8.

    Returns an (H, W, 3) uint8 array in RGB order.
    """
    y_size = width * height
    c_size = y_size // 4
    cw, ch = width // 2, height // 2

    y_plane = np.frombuffer(raw, dtype=np.uint8, count=y_size).reshape(height, width).astype(np.int32)
    cb_plane = np.frombuffer(raw, dtype=np.uint8, count=c_size, offset=y_size).reshape(ch, cw).astype(np.int32)
    cr_plane = np.frombuffer(raw, dtype=np.uint8, count=c_size, offset=y_size + c_size).reshape(ch, cw).astype(np.int32)

    # Upsample chroma 2x2 nearest-neighbor, exactly like the C++ decoder's
    # u_row[i / 2] / v_row[i / 2] integer-division indexing.
    cb_up = np.repeat(np.repeat(cb_plane, 2, axis=0), 2, axis=1)[:height, :width]
    cr_up = np.repeat(np.repeat(cr_plane, 2, axis=0), 2, axis=1)[:height, :width]

    Y = (y_plane - 16) * 298
    Cb = cb_up - 128
    Cr = cr_up - 128

    r = (Y + 459 * Cr + 128) >> 8
    g = (Y - 55 * Cb - 136 * Cr + 128) >> 8
    b = (Y + 541 * Cb + 128) >> 8

    rgb = np.stack([r, g, b], axis=-1)
    return np.clip(rgb, 0, 255).astype(np.uint8)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help=".v5y file produced by pack_yuv_lz4.py")
    ap.add_argument("--play", action="store_true", help="play back in a window (needs opencv-python)")
    ap.add_argument("--out", help="write decoded frames to an image (single --frame) or video file")
    ap.add_argument("--frame", type=int, help="only decode this frame index (0-based); use with --out for a PNG")
    args = ap.parse_args()

    if (args.play or (args.out and args.frame is None)) and cv2 is None:
        sys.exit("opencv-python is required for --play or video --out; pip install opencv-python")

    with open(args.input, "rb") as f:
        hdr = read_header(f)
        print(f"[*] {args.input}: [{hdr['magic']} - {hdr['desc']}] {hdr['width']}x{hdr['height']} @ {hdr['fps']}fps, "
              f"{hdr['frame_count']} frames (header count)")

        w, h = hdr["width"], hdr["height"]
        fmt = hdr["format"]
        compressed = hdr["compressed"]
        writer = None
        decoded_count = 0

        for i, raw in enumerate(read_frames(f, w, h, compressed=compressed, fmt=fmt)):
            if args.frame is not None and i != args.frame:
                continue

            if fmt == "yuv":
                rgb = i420_to_rgb_bt709(raw, w, h)
            else:
                rgb = np.frombuffer(raw, dtype=np.uint8, count=w * h * 3).reshape((h, w, 3))

            bgr = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR) if cv2 is not None else rgb[:, :, ::-1]

            if args.frame is not None:
                if args.out:
                    cv2.imwrite(args.out, bgr)
                    print(f"[+] wrote frame {i} to {args.out}")
                else:
                    print("[*] pass --out to save this frame, or --play to view it")
                    if cv2 is not None:
                        cv2.imshow("v5p frame", bgr)
                        cv2.waitKey(0)
                break

            if args.play:
                cv2.imshow("v5p playback", bgr)
                delay_ms = max(1, int(1000 / (hdr["fps"] or 30)))
                if cv2.waitKey(delay_ms) & 0xFF == ord('q'):
                    break

            if args.out:
                if writer is None:
                    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
                    writer = cv2.VideoWriter(args.out, fourcc, hdr["fps"] or 30, (w, h))
                writer.write(bgr)

            decoded_count += 1

        if writer is not None:
            writer.release()
            print(f"[+] wrote {decoded_count} frames to {args.out}")
        if args.play:
            cv2.destroyAllWindows()
            print(f"[+] played {decoded_count} frames")


if __name__ == "__main__":
    main()
