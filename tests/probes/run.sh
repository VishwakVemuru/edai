#!/bin/bash
# The probes from issue #1, against this repository's compiled brain.
#   bash run.sh                          on a Mac with Apple silicon
#   TARGET=linux-x86_64 bash run.sh      on Linux (or linux-aarch64)
# Needs cc (clang or gcc), python3 and node. Takes a few minutes and uses every core.
set -e
cd "$(dirname "$0")"
export EDAI_REPO=$(cd ../.. && pwd)
TARGET=${TARGET:-macos-arm64}
LIB=$EDAI_REPO/brain/lib/$TARGET/libaa.a; INC=$EDAI_REPO/brain/include
mkdir -p build out
cc -std=c11 -O2 -I"$INC" tools/native_solve.c "$LIB" -o build/native_solve
cc -std=c11 -O1 -I"$INC" tools/contract.c "$LIB" -o build/contract
cc -std=c11 -O2 -I"$INC" tools/stack_probe.c "$LIB" -o build/stack_probe -lpthread
cp build/native_solve tools/native_solve
BEAT=$EDAI_REPO/challengers/beat/c
if [ -f "$BEAT/beat.c" ]; then
  cc -std=c99 -O2 -ffp-contract=off -I"$BEAT" tools/beat_solve.c "$BEAT/beat.c" -o build/beat_solve && cp build/beat_solve tools/beat_solve
fi
J=$(sysctl -n hw.ncpu 2>/dev/null || nproc)
run() { python3 tools/prun.py "$1" "$2" "$3" -j "$J"; }

echo "brain: $(grep -m1 -o 'AA_VERSION_MINOR [0-9]*' "$INC/aa.h" | sed 's/AA_VERSION_MINOR /version 1./'), library $TARGET"
( cd out && for g in gen_selfcheck gen_rules gen_v11; do python3 ../generators/$g.py >/dev/null; done && python3 ../generators/gen_false_exact.py 400 >/dev/null )
for f in self cases rules v11; do run native out/$f.lines out/$f.native; done
run wasm out/self.lines out/self.wasm
if [ -f tools/beat_solve ]; then for f in cases rules; do run beat out/$f.lines out/$f.beat; done; fi
python3 tools/report.py out

echo; echo "== The API contract (each case in its own process)"; ./build/contract | tail -1
echo; echo "== Stack (aa.h: at most AA_STACK_BYTES on any target)"
python3 tools/to_lines.py "$EDAI_REPO/problems/problems.json" | ./build/stack_probe > out/stack.txt
echo; echo "== Heap and globals (README: no allocation, no global or static data)"
echo "   external symbols the library needs: $(nm -u "$LIB" 2>/dev/null | grep -v ':$' | grep -cv '^$' || true)"
echo "   data or bss symbols in the library: $(nm "$LIB" 2>/dev/null | awk 'NF >= 3 && $2 ~ /^[DdBbCc]$/' | wc -l | tr -d ' ')"
