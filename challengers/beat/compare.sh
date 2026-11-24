#!/bin/bash
# Scores edai and beat on the same set of problems, each with the repository's score.py and the set's key.
# Meant for fresh sets that neither solver has seen.
#
#   bash compare.sh problems.json answers.json            both beat versions
#   bash compare.sh problems.json answers.json portable   one version (portable or deep)
#   TARGET=linux-x86_64 bash compare.sh ...               on Linux (edai's library for that target)
#
# edai runs through its own runners/computer/run300.c and harness.c, unmodified; only the problem table is generated
# (to_stream_data.py). beat runs through c/main.c. Needs cc and python3.
set -e
cd "$(dirname "$0")"
PJ=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); AJ=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
VERSIONS=${3:-"portable deep"}
REPO=../..; TARGET=${TARGET:-macos-arm64}; CC=${CC:-cc}
W=build/compare; rm -rf $W; mkdir -p $W/edai

python3 to_stream_data.py "$PJ" "$AJ" $W/edai/stream_data.h
cp $REPO/runners/computer/run300.c $REPO/runners/computer/harness.c $REPO/runners/computer/harness.h $W/edai/
$CC -std=c11 -O2 -I$REPO/brain/include -I$W/edai $W/edai/run300.c $W/edai/harness.c $REPO/brain/lib/$TARGET/libaa.a -o $W/edai/run300
$W/edai/run300 > $W/edai/answers.txt
printf "%-14s %s\n" "edai $(grep -m1 -o 'AA_VERSION_MINOR [0-9]*' $REPO/brain/include/aa.h | sed 's/AA_VERSION_MINOR /1./')" "$(python3 $REPO/score.py $W/edai/answers.txt --key "$AJ")"

for v in $VERSIONS; do
  case $v in portable) FLAGS="" ;; deep) FLAGS="-DBT_DEEP -DBT_DEEP_ALWAYS=1" ;; *) echo "unknown version $v"; exit 1 ;; esac
  d=$W/$v; mkdir -p $d
  python3 c/gen_problems.py "$PJ" $d/problems.h && cp c/main.c c/beat.h $d/
  $CC -std=c99 -O2 -ffp-contract=off $FLAGS -o $d/beat $d/main.c c/beat.c
  (cd $d && ./beat > answers.txt 2>/dev/null)
  printf "%-14s %s\n" "beat $v" "$(python3 $REPO/score.py $d/answers.txt --key "$AJ")"
done
