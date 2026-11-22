#!/bin/bash
# Rebuilds the beat solver, runs it on the main set and the five fresh sets, scores every set with the repository's
# score.py, and compares every answer with the results stored in results/.
#
#   bash run.sh            both versions: "portable" (fits a Pico) and "deep" (computers only, a larger search)
#   bash run.sh portable   one version
#   CC=gcc bash run.sh     another compiler (default: cc)
#
# Needs a C99 compiler with the GCC/Clang builtins (gcc or clang) and python3. No other dependencies.
set -e
cd "$(dirname "$0")"
REPO=../..
CC=${CC:-cc}
VERSIONS=${1:-"portable deep"}
SETS="main seed-23 seed-24 seed-25 seed-26 seed-27"
mismatch=0
for v in $VERSIONS; do
  case $v in
    portable) FLAGS="" ;;
    deep)     FLAGS="-DBT_DEEP -DBT_DEEP_ALWAYS=1" ;;
    *) echo "unknown version $v"; exit 1 ;;
  esac
  echo "== $v"
  total=0; heldout=0
  for s in $SETS; do
    if [ $s = main ]; then P=$REPO/problems; else P=$REPO/problems/fresh/$s; fi
    d=build/$v/$s; mkdir -p $d results/$v
    python3 c/gen_problems.py $P/problems.json $d/problems.h
    cp c/main.c c/beat.h $d/
    $CC -std=c99 -O2 -ffp-contract=off $FLAGS -o $d/beat $d/main.c c/beat.c
    start=$(date +%s)
    (cd $d && ./beat > answers.txt 2>/dev/null)
    secs=$(( $(date +%s) - start ))
    score=$(python3 $REPO/score.py $d/answers.txt --key $P/answers.json)
    n=$(echo "$score" | awk '{print $2}'); total=$((total + n))
    case $s in seed-25|seed-26|seed-27) heldout=$((heldout + n)) ;; esac
    # compare answers (not the timing fields) with the stored results
    if [ -f results/$v/$s.txt ]; then
      if diff -q <(sed -E 's/ ms=[0-9]+//; s/ q=[0-9]+//' $d/answers.txt) <(sed -E 's/ ms=[0-9]+//; s/ q=[0-9]+//' results/$v/$s.txt) >/dev/null; then same="same answers as results/$v/$s.txt"
      else same="DIFFERS from results/$v/$s.txt"; mismatch=1; fi
    else cp $d/answers.txt results/$v/$s.txt; same="stored as results/$v/$s.txt"; fi
    printf "%-8s %s  (%ss; %s)\n" "$s" "$score" "$secs" "$same"
  done
  echo "   all six sets: $total of 1800; held-out sets (seed-25, 26, 27): $heldout of 900 (edai: 826)"
done
[ $mismatch = 0 ] && echo "every answer matches the stored results" || { echo "some answers differ from the stored results"; exit 1; }
