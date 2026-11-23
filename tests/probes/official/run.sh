#!/bin/bash
# The key cases through edai's OWN runner and checker (runners/computer/run300.c and harness.c, unmodified):
# only the problem table (stream_data.h, in edai's own format) is replaced.
#   bash official/run.sh          (from tests/probes; TARGET=linux-x86_64 on Linux)
set -e
cd "$(dirname "$0")"
R=$(cd ../../.. && pwd); T=${TARGET:-macos-arm64}
mkdir -p build && cp "$R/runners/computer/run300.c" "$R/runners/computer/harness.c" "$R/runners/computer/harness.h" build/ && cp stream_data.h build/
cc -std=c11 -O2 -I"$R/brain/include" -Ibuild build/run300.c build/harness.c "$R/brain/lib/$T/libaa.a" -o build/run300
build/run300 > build/run.txt
paste -d'\n' names.txt <(grep '^p' build/run.txt) | sed 's/^p0/   p0/'
