#!/bin/bash
# flash.sh — Build and upload firmware to VEX V5 Brain
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$SCRIPT_DIR/pros_project"

# Add local toolchain to PATH if available
if [ -d "$HOME/.gemini/antigravity/scratch/arm-toolchain/bin" ]; then
    export PATH="$HOME/.gemini/antigravity/scratch/arm-toolchain/bin:$PATH"
fi

cd "$PROJ_DIR"
pros make
pros upload --slot 1 "$PROJ_DIR"
