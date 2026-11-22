#!/bin/bash
# The whole Pico run in one command (a Mac):  bash run.sh
#   1. hold the board's BOOTSEL button while plugging it in.  A Pico 2 shows up as a drive called RP2350, the original
#      Pico as RPI-RP2; this copies the matching program onto it and the board restarts into the program
#   2. read.py opens the board's serial port (which starts the run), shows every line and checks it against expected.txt
set -e
cd "$(dirname "$0")"
WAIT=${WAIT:-120}
echo "Waiting up to $WAIT s for the board's drive (hold BOOTSEL while you plug it in)..."
VOL=""
for i in $(seq 1 $((WAIT * 2))); do
  if [ -d /Volumes/RP2350 ]; then VOL=/Volumes/RP2350; F=edai-pico2.uf2; break; fi
  if [ -d /Volumes/RPI-RP2 ]; then VOL=/Volumes/RPI-RP2; F=edai-original-pico.uf2; break; fi
  sleep 0.5
done
[ -n "$VOL" ] || { echo "No Pico drive appeared. Unplug, hold BOOTSEL, plug in again, then rerun."; exit 1; }
echo "Copying $F ($(shasum -a 256 $F | cut -c1-16)...)"
cp $F "$VOL/" || true
for i in $(seq 1 40); do [ -d "$VOL" ] || break; sleep 0.5; done
python3 read.py
