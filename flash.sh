#!/bin/bash
# flash.sh — Build and upload firmware to VEX V5 Brain
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -d "$HOME/.gemini/antigravity/scratch/arm-toolchain/bin" ]; then
    export PATH="$HOME/.gemini/antigravity/scratch/arm-toolchain/bin:$PATH"
fi

pros make
pros upload --slot 1 .
