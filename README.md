# VEX V5 Video Tools

Utilities to convert video files to `.v5y` format for playback on the VEX V5 Brain LCD (480x272 @ 60 FPS).

## Files
- `pack_yuv.py`: Python video encoder (FFmpeg + LZ4 block compression)
- `make_video.sh`: Shell helper script to convert videos to MicroSD card root (`/usd/video.v5y`)
- `flash.sh`: Build & upload script for PROS CLI

## Usage
Convert any video file to `.v5y` and write to SD card:
```bash
./make_video.sh input.mp4
```
