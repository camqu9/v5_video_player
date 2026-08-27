#!/bin/bash
# make_video.sh — Pack video for VEX V5 Brain display (Native 480x272 @ 60 FPS LZ4)
# Usage: ./make_video.sh <input_video> [sd_card_path|width] [width] [height] [fps]
#
# Examples:
#   ./make_video.sh my_video.mp4                          # Defaults to 480x272 @ 60fps LZ4
#   ./make_video.sh my_video.mp4 480 272 60               # Custom resolution/fps
#   ./make_video.sh my_video.mp4 /run/media/user/MYSDCARD # Custom SD path

set -e

INPUT="$1"

if [ -z "$INPUT" ]; then
    echo "Usage: $0 <input_video> [sd_card_path|width] [height] [fps]"
    echo ""
    echo "Examples:"
    echo "  $0 myvideo.mp4"
    echo "  $0 myvideo.mp4 480 272 60"
    echo "  $0 myvideo.mp4 /run/media/user/MYSDCARD 480 272 60"
    exit 1
fi

# Detect whether 2nd argument is a numeric width or an SD card directory path
if [[ "$2" =~ ^[0-9]+$ ]]; then
    SD="/run/media/$(whoami)/$(ls /run/media/$(whoami)/ 2>/dev/null | head -1)"
    WIDTH="$2"
    HEIGHT="${3:-272}"
    FPS="${4:-60}"
else
    SD="${2:-/run/media/$(whoami)/$(ls /run/media/$(whoami)/ 2>/dev/null | head -1)}"
    WIDTH="${3:-480}"
    HEIGHT="${4:-272}"
    FPS="${5:-60}"
fi

OUTPUT="$SD/video.v5y"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -d "$SD" ]; then
    echo "ERROR: SD card not found at '$SD'"
    echo "Plug in your MicroSD card and try again, or specify path as 2nd argument."
    exit 1
fi

echo "=== V5 Video Packer ==="
echo "Input     : $INPUT"
echo "Output    : $OUTPUT"
echo "Target Res: ${WIDTH}x${HEIGHT} @ ${FPS}fps (LZ4 Compressed)"
echo ""

python3 "$SCRIPT_DIR/pack_yuv.py" "$INPUT" "$OUTPUT" "$WIDTH" "$HEIGHT" "$FPS"

echo ""
echo "Syncing SD card..."
sync
echo "Done! MicroSD updated with $OUTPUT."
echo "Put SD in your V5 Brain and run slot 1 to play."
