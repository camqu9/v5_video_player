# VEX V5 Video Player

Plays `.v5y` compressed video files on the VEX V5 Brain LCD (480x272 @ 60 FPS) while running a 4-motor split arcade drivetrain.

## Quick Start

### 1. Encode Video
Convert any video to `.v5y` format and save to SD card:
```bash
./make_video.sh input.mp4
```

### 2. Build & Flash Firmware
Flash the PROS firmware to slot 1:
```bash
./flash.sh
```

## Project Structure
- `pros_project/`: PROS C++ firmware source (`main.cpp`, `v5_video_player.hpp`)
- `pack_yuv.py`: Python encoder (FFmpeg + LZ4 block compression)
- `make_video.sh`: Helper script to encode video directly to SD card
- `flash.sh`: Build & upload script for PROS CLI

## Controls & Features
- **Drive**: Left stick Y = throttle, right stick X = turn.
- **Brake Mode**: Press **X** to toggle between Coast, Brake, and Hold modes.
- **Video**: Plays `/usd/video.v5y` automatically in a background task.
