#!/usr/bin/env python3
import argparse, subprocess, struct, sys, os, time
import lz4.block

def fsize(fmt,w,h): return w*h*3 if fmt=="rgb" else w*h+2*((w//2)*(h//2))
def magic(fmt,z): return (b"V5RZ" if z else b"V5RU") if fmt=="rgb" else (b"V5YZ" if z else b"V5YU")

def pack(inp,out,fmt,w,h,fps,z):
    if fmt=="yuv" and (w%2 or h%2): sys.exit(f"[!] YUV420 needs even dims, got {w}x{h}")
    m = magic(fmt,z)
    print(f"[*] {m.decode()} {fmt.upper()}{',LZ4' if z else ',raw'} {inp} -> {w}x{h}@{fps}")
    cmd = ["ffmpeg","-y","-i",inp,"-vf",f"scale={w}:{h}:force_original_aspect_ratio=decrease,pad={w}:{h}:(ow-iw)/2:(oh-ih)/2:black,fps={fps}","-pix_fmt","rgb24" if fmt=="rgb" else "yuv420p","-f","rawvideo","pipe:1"]
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
                f.write(struct.pack('<I',len(c))); f.write(c); ct += len(c)
            else:
                f.write(raw)
            rt += fs; n += 1
        f.seek(12); f.write(struct.pack('<I',n))
    p.wait()
    print(f"[+] {n} frames, {time.time()-t0:.1f}s, {os.path.getsize(out)/1e6:.2f}MB" + (f", {rt/ct:.2f}x ratio" if z and ct else ""))

if __name__ == "__main__":
    a = argparse.ArgumentParser()
    a.add_argument("input"); a.add_argument("output")
    a.add_argument("--format", choices=["rgb","yuv"], default="yuv")
    a.add_argument("--no-compress", action="store_true")
    a.add_argument("--width", type=int, default=480)
    a.add_argument("--height", type=int, default=272)
    a.add_argument("--fps", type=int, default=60)
    r = a.parse_args()
    pack(r.input, r.output, r.format, r.width, r.height, r.fps, not r.no_compress)