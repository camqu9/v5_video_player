#!/usr/bin/env python3
import argparse, subprocess, struct, sys, os, time
import lz4.block

def fsize(fmt,w,h): return w*h*3 if fmt=="rgb" else w*h+2*((w//2)*(h//2))
def magic(fmt,z): return (b"V5RZ" if z else b"V5RU") if fmt=="rgb" else (b"V5YZ" if z else b"V5YU")

def resolve_source(inp, ytdl_format="bestvideo/best"):
    # If local file exists, use it directly
    if os.path.exists(inp):
        return inp

    # Attempt resolution via yt-dlp for URLs or queries
    print(f"[*] Resolving '{inp}' via yt-dlp...")
    try:
        import yt_dlp
        ydl_opts = {
            'format': ytdl_format,
            'quiet': True,
            'no_warnings': True,
        }
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            info = ydl.extract_info(inp, download=False)
            if 'entries' in info:
                info = info['entries'][0]
            url = info.get('url')
            title = info.get('title', inp)
            if url:
                print(f"[+] Found stream: {title}")
                return url
    except Exception:
        pass

    try:
        res = subprocess.run(["yt-dlp", "-g", "-f", ytdl_format, inp],
                             capture_output=True, text=True, check=True)
        url = res.stdout.strip().split('\n')[0]
        if url:
            print(f"[+] Found stream via yt-dlp CLI")
            return url
    except Exception:
        pass

    return inp

def pack(inp,out,fmt,w,h,fps,z,ytdl_format="bestvideo/best"):
    if fmt=="yuv" and (w%2 or h%2): sys.exit(f"[!] YUV420 needs even dims, got {w}x{h}")
    src = resolve_source(inp, ytdl_format)
    m = magic(fmt,z)
    print(f"[*] {m.decode()} {fmt.upper()}{',LZ4' if z else ',raw'} {inp} -> {w}x{h}@{fps}")
    cmd = ["ffmpeg","-nostdin","-y","-i",src,"-vf",f"scale={w}:{h}:force_original_aspect_ratio=decrease,pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:black,fps={fps}","-pix_fmt","rgb24" if fmt=="rgb" else "yuv420p","-f","rawvideo","pipe:1"]
    fs = fsize(fmt,w,h)
    t0=time.time(); n=0; rt=0; ct=0
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
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
        f.seek(12); f.write(struct.pack('<I',n))
    if p.stdout:
        p.stdout.close()
    try:
        p.wait(timeout=2)
    except subprocess.TimeoutExpired:
        p.kill()
        p.wait()
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
