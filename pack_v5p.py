#!/usr/bin/env python3
import argparse, subprocess, struct, sys, os, time
import lz4.block

def fsize(fmt,w,h): return w*h*3 if fmt=="rgb" else w*h+2*((w//2)*(h//2))
def magic(fmt,z): return (b"V5RZ" if z else b"V5RU") if fmt=="rgb" else (b"V5YZ" if z else b"V5YU")

def is_remote(inp):
    return inp.startswith(("http://", "https://", "www.", "ytdl:", "ytsearch:")) or not os.path.exists(inp)

def pack(inp,out,fmt,w,h,fps,z,ytdl_format="bestvideo/best"):
    if fmt=="yuv" and (w%2 or h%2): sys.exit(f"[!] YUV420 needs even dims, got {w}x{h}")
    m = magic(fmt,z)
    print(f"[*] {m.decode()} {fmt.upper()}{',LZ4' if z else ',raw'} {inp} -> {w}x{h}@{fps}")

    yt_p = None
    if is_remote(inp):
        print(f"[*] Streaming '{inp}' via yt-dlp pipe...")
        yt_cmd = ["yt-dlp", "-o", "-", "-f", ytdl_format, "--no-warnings", inp]
        yt_p = subprocess.Popen(yt_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        ffmpeg_input = "pipe:0"
        ffmpeg_stdin = yt_p.stdout
    else:
        ffmpeg_input = inp
        ffmpeg_stdin = subprocess.DEVNULL

    cmd = ["ffmpeg","-nostdin","-y","-i",ffmpeg_input,"-vf",f"scale={w}:{h}:force_original_aspect_ratio=decrease,pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:black,fps={fps}","-pix_fmt","rgb24" if fmt=="rgb" else "yuv420p","-f","rawvideo","pipe:1"]
    fs = fsize(fmt,w,h)
    t0=time.time(); n=0; rt=0; ct=0
    p = subprocess.Popen(cmd, stdin=ffmpeg_stdin, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if yt_p and yt_p.stdout:
        yt_p.stdout.close()

    with open(out,'wb') as f:
        f.write(m); f.write(struct.pack('<HHHHI',w,h,fps,0,0))
        while True:
            raw = p.stdout.read(fs)
            if len(raw) < fs: break
            if z:
                c = lz4.block.compress(raw, mode='default', store_size=False)
                c_len = len(c)
                f.write(struct.pack('<I',c_len)); f.write(c); ct += c_len
                c_info = f" | {c_len/1024:.1f}KB ({fs/c_len:.2f}x) | avg {rt+fs:.0f}/{ct:.0f} ({(rt+fs)/ct:.2f}x)"
            else:
                f.write(raw)
                ct += fs
                c_info = ""
            rt += fs; n += 1
            el = time.time() - t0
            cur_fps = n / el if el > 0 else 0
            sys.stdout.write(f"\r[*] Frame {n:5d} | {el:.1f}s | {cur_fps:.1f} fps | {ct/1e6:.2f}MB{c_info}   ")
            sys.stdout.flush()

        # Update frame count in header
        f.seek(12); f.write(struct.pack('<I',n))

        # Explicit sync notification so USB drive writes don't look like hangs
        sys.stdout.write(f"\r[*] Rendered {n} frames ({time.time()-t0:.1f}s). Flushing & syncing {ct/1e6:.2f}MB to disk...   ")
        sys.stdout.flush()
        f.flush()
        os.fsync(f.fileno())

    if p.stdout:
        p.stdout.close()
    try:
        p.wait(timeout=3)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait()

    if yt_p:
        try:
            yt_p.wait(timeout=2)
        except subprocess.TimeoutExpired:
            yt_p.kill()
            yt_p.wait()

    print()
    print(f"[+] {n} frames, {time.time()-t0:.1f}s, {os.path.getsize(out)/1e6:.2f}MB" + (f", {rt/ct:.2f}x ratio" if z and ct else ""))

if __name__ == "__main__":
    a = argparse.ArgumentParser(description="Pack video file or online stream (via yt-dlp) into V5P format.")
    a.add_argument("input", help="Input video file path or URL (e.g. YouTube, Twitch, etc.)")
    a.add_argument("output", help="Output .v5p file path")
    a.add_argument("--format", choices=["rgb","yuv"], default="yuv", help="Pixel format (default: yuv)")
    a.add_argument("--no-compress", action="store_true", help="Disable LZ4 compression")
    a.add_argument("--width", type=int, default=480, help="Output width (default: 480)")
    a.add_argument("--height", type=int, default=272, help="Output height (default: 272)")
    a.add_argument("--fps", type=int, default=60, help="Output framerate (default: 60)")
    a.add_argument("--ytdl-format", default="bestvideo/best", help="yt-dlp format selector (default: bestvideo/best)")
    r = a.parse_args()
    pack(r.input, r.output, r.format, r.width, r.height, r.fps, not r.no_compress, r.ytdl_format)
