# AI Context & Project State Handover — VEX V5 Video Player & Robot System

> **Handover Document for Future AI Agents / Developers**
> **Last Updated**: August 26, 2026
> **Project Location**: `/run/media/camqu9/AA4C-DBA8/v5_video_player/`
> **Target Device**: VEX V5 Robot Brain (ARM Cortex-A9 @ 667 MHz, 480×272 LCD)

---

## 1. Project Overview & Architecture

This project is a high-performance video player and 4-motor 6WD robot control system built on the **PROS 4** framework for the VEX V5 Robot Brain.

### Core Naming & Specifications:
1. **Project Name**: `v5_video_player`
2. **Header & Namespace**: `v5_video_player.hpp` (`namespace v5_video`, with backward-compatibility aliases `v5_sd60` and `v5_yuv`).
3. **Video Container File**: `/usd/video.v5y` on MicroSD card root.
4. **Native SD Resolution Video @ 60 FPS**: Plays smooth 480×272 video directly on the V5 Brain LCD via MicroSD card.
5. **LZ4 Frame Compression**: Reduces file size by **69.1%** (e.g. 600 MB down to 185 MB) and SD read bandwidth down to ~3.47 MB/s.
6. **Strict CPU Budget (<= 10% CPU)**: Single-blit hardware LCD updates + integer LUT decoding keep total CPU usage under **~8.5% at 60 FPS**.
7. **4-Motor 6WD Drivetrain Control**: Driver arcade control with exponential power curves, deadband filtering, and brake mode toggle running concurrently with background video playback.
8. **Program Branding**: Program icon set to **`USER000x.bmp`** and description set to **`paws.nya.je`** on Slot 1.

---

## 2. Directory Structure & File Map

```
/run/media/camqu9/AA4C-DBA8/
├── video.v5y                  # Packed LZ4 video stream on MicroSD card root
├── AI_CONTEXT.md              # Root copy of AI context handover document
└── v5_video_player/
    ├── AI_CONTEXT.md          # Primary AI context handover document
    ├── README.md              # Project quick start & command reference
    ├── make_video.sh          # Helper script to convert any video file to video.v5y
    ├── pack_yuv.py            # FFmpeg + LZ4 Python video encoder
    ├── flash.sh               # 1-click build & flash script for LLM / User
    └── pros_project/          # PROS C++ Firmware Project (v5_video_player)
        ├── project.pros       # PROS project metadata & upload_options
        ├── common.mk          # Make configuration (CXX_STANDARD?=gnu++20)
        ├── Makefile           # Main PROS Makefile
        ├── include/
        │   └── v5_video_player.hpp # Primary C++ video decoder & renderer header
        └── src/
            └── main.cpp       # 4-motor 6WD drivetrain + background video task
```

---

## 3. Robot Hardware & Drivetrain Specifications

- **Chassis**: 6-Wheel Drive (6WD) omni-wheel drivetrain powered by 4 VEX V5 Smart Motors.
- **Motor Ports**:
  - **Left Front**: Port `1`
  - **Left Back**: Port `2`
  - **Right Front**: Port `-9` (Reversed)
  - **Right Back**: Port `-10` (Reversed)
- **Drive Logic (`src/main.cpp`)**:
  - **Split Arcade Drive**: Left Stick Y (Axis 3) = Throttle; Right Stick X (Axis 1) = Steering.
  - **Cubic Power Curve**: `input^3` normalization for high precision at slow speeds and 100% full power at max stick push.
  - **Deadband**: `DEADZONE = 5` to ignore stick drift.
  - **Brake Mode Toggle**: Press **Button X** to cycle between `COAST` (1 rumble `.`), `BRAKE` (2 rumbles `..`), and `HOLD` park-lock (3 rumbles `...`).
  - **Background Video**: Video player runs in a background RTOS task (`v5_video::play_video`) while driving.

---

## 4. Video Encoder Specs (`pack_yuv.py` / `.v5y`)

- **Format**: `.v5y` (with LZ4 block compression)
- **Header (16 bytes)**:
  - `[0-3]` Magic: `b'V5LZ'` (or `b'V5YU'` for raw uncompressed)
  - `[4-5]` `uint16_t src_width` (default 480)
  - `[6-7]` `uint16_t src_height` (default 272)
  - `[8-9]` `uint16_t fps` (default 60)
  - `[10-13]` `uint32_t frame_count`
  - `[14-15]` `uint16_t flags` (`0x01` = LZ4)
- **Encoding Command**:
  ```bash
  python3 pack_yuv.py input.mp4 /run/media/camqu9/AA4C-DBA8/video.v5y 480 272 60
  ```

---

## 5. Build Toolchain & Flashing Instructions

- **ARM Toolchain Location**: `/home/camqu9/.gemini/antigravity/scratch/arm-toolchain/bin/arm-none-eabi-g++`
- **PROS Upload Metadata**:
  - Configured in `project.pros`:
    ```json
    "upload_options": {
        "description": "paws.nya.je",
        "icon": "USER000x.bmp"
    }
    ```
- **Automated Flashing**:
  ```bash
  /run/media/camqu9/AA4C-DBA8/v5_video_player/flash.sh
  ```
