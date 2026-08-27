#!/bin/bash
# flash.sh — Auto-build & flash firmware to VEX V5 Brain
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$SCRIPT_DIR/pros_project"

# Ensure ARM GCC toolchain is in PATH
export PATH="/home/camqu9/.gemini/antigravity/scratch/arm-toolchain/bin:$PATH"

echo "=== V5 Auto Flash ==="
echo "[1/2] Compiling firmware..."
cd "$PROJ_DIR"
pros make

echo ""
echo "[2/2] Uploading to VEX V5 Brain (Slot 1)..."
pros upload --slot 1 --icon USER000x.bmp --description "paws.nya.je" "$PROJ_DIR"

echo ""
echo "[+] Done! Program successfully flashed with icon USER000x.bmp and description 'paws.nya.je'."
