#!/bin/bash
# make_video.sh — Convert video to .v5y for VEX V5 Brain
set -e

INPUT="$1"
if [ -z "$INPUT" ]; then
    echo "Usage: $0 <input_video> [sd_path|width] [height] [fps]"
    exit 1
fi

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

python3 "$SCRIPT_DIR/pack_yuv.py" "$INPUT" "$OUTPUT" "$WIDTH" "$HEIGHT" "$FPS"
sync
echo "Done! Output written to $OUTPUT"
