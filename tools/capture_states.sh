#!/usr/bin/env bash
# Capture screenshots of the CHIP-8 debugger running headlessly.
#
# Usage: tools/capture_states.sh <binary> <output-dir> <rom-key> <steps> [label]
#   binary      path to the compiled learning_chip8 executable
#   output-dir  where the .png files are written
#   rom-key     key that loads the ROM (1, 2 or 3, as handled in main.c)
#   steps       number of space presses (instructions to advance)
#   label       optional file name prefix (default: rom<rom-key>)
#
# Produces two screenshots: the freshly loaded ROM and the state after
# advancing <steps> instructions with space.
#
# Requirements: Xvfb, xdotool, ImageMagick (import).
set -e

if [ $# -lt 4 ]; then
    echo "usage: $0 <binary> <output-dir> <rom-key> <steps> [label]" >&2
    exit 1
fi

BIN="$(readlink -f "$1")"
OUT_DIR="$(readlink -f "$2")"
ROM_KEY="$3"
STEPS="$4"
LABEL="${5:-rom${ROM_KEY}}"
BIN_DIR="$(dirname "$BIN")"
mkdir -p "$OUT_DIR"

Xvfb :77 -screen 0 1920x1080x24 &
XVFB_PID=$!
trap "kill $XVFB_PID 2>/dev/null" EXIT
export DISPLAY=:77
sleep 1

cd "$BIN_DIR"
"$BIN" &
APP_PID=$!
sleep 2

WIN=$(xdotool search --name "CHIP-8" | head -1)
echo "window: $WIN"
xdotool windowfocus --sync "$WIN"
sleep 0.3

shoot() {
    sleep 0.4
    local dest="$OUT_DIR/$1.png"
    import -window "$WIN" "$dest"
    echo "captured $dest"
}

# load the ROM (message shows for 5 s, so capture promptly)
xdotool key --delay 150 "$ROM_KEY"
shoot "${LABEL}-loaded"

# advance <steps> instructions with space
xdotool key --delay 150 --repeat "$STEPS" space
shoot "${LABEL}-${STEPS}-instructions"

kill $APP_PID 2>/dev/null || true
wait $APP_PID 2>/dev/null || true
