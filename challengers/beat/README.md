# beat: an open-source challenger to the edai brain

An attempt to beat the edai brain on the same 300-problem challenge, written in the open: every source file, every
answer and every script is here. **Version 2 scores more than edai 1.2 on edai's published sets**, but every one of
those sets has now been used to tune both solvers, so the fair test is a fresh set that neither has seen:
`compare.sh` scores both on any new set in one command.

## Results

Scored with the repository's `score.py`, each set with its own key. edai is version 1.2.

| Set | edai 1.2 | beat 2, portable | beat 2, deep |
|---|---|---|---|
| main (`problems/`) | 274 | 280 | **283** |
| fresh, seed 23 | 270 | 274 | **276** |
| fresh, seed 24 | 279 | 283 | **287** |
| fresh, seed 25 | 270 | 272 | **275** |
| fresh, seed 26 | **284** | 278 | 279 |
| fresh, seed 27 | 272 | 270 | **277** |
| seeds 25–27 | 826 | 820 | **831** |
| **all six sets** | 1,649 | 1,657 | **1,677** |

By kind of problem, over all six sets:

| | edai 1.2 | beat 2, deep |
|---|---|---|
| Rules (720) | 591 | **614** |
| Trends (720) | 700 | **703** |
| Noise (360) | 358 | **360** |

- **portable** is plain C99 with no allocation and no C library: about 19 KB of code and 147 KB of static RAM, so it
  fits a Raspberry Pi Pico or Pico 2. edai keeps 5.2 KB of state, so it needs far less memory.
- **deep** adds a larger search (`c/deep.c`) that needs a few MB and runs only on a computer: about 90 s per set of
  300 on an Apple-silicon Mac, against about 6 s for portable.

## What changed in version 2

Version 1 refused (CANNOT) 15 real trends that edai answered: noisy trends with 8 examples and strong curves. It
decided trend or noise by how well its best smooth fit predicted each example from the others, over all the
examples. Predicting an end example from the rest is extrapolation, and curved trends extrapolate badly, so their
error looked like noise.

Version 2 decides by the interior examples only (all but one at the smallest x and one at the largest), because the
questions always lie between the examples. On edai's six sets that score separates every trend from every noise
problem: with 8 examples the highest trend scored 0.445 and the lowest noise 0.452; with 12, 0.307 and 0.537. The
cut sits in the middle of each gap (0.4485 and 0.406). The estimates themselves are unchanged: had version 1 answered
every trend, its estimates would already have been right on 703 of 720, against edai's 700.

| | version 1, deep | version 2, deep |
|---|---|---|
| trends refused (of 720) | 16 | 0 |
| trends right | 693 | 703 |
| noise right (of 360) | 359 | 360 |
| all six sets | 1,666 | 1,677 |

## How the sets were used: read this before comparing

- Version 1 was designed on the main set and seeds 23 and 24, with seeds 25 to 27 held back; their totals were checked
  twice (810, then 821).
- **Version 2's trend cut was set on all six sets**: the middle of the gap between trends and noise, measured on all
  1,800 problems. Leaving each set out in turn and setting the cut on the other five refused 1 trend and accepted no
  noise on the set left out, so the cut is not fitted to any one set. But no set is unseen any more.
- edai's settings were chosen on a separate design set, but its versions 1.1 and 1.2 were also measured on these sets
  while they were built (see the main README's "Fresh problems").
- The cut is calibrated to edai's generator. On fresh trends and noise from a different generator (noisier trends,
  `±500` noise), version 2 accepted no noise at 12 examples (version 1: 23 of 2,000) and about as much at 8 (60 of
  2,000; version 1: 70), but refused more genuine noisy trends at 12 examples (right 830 of 1,000; version 1: 898).
  For edai's challenge, fresh sets come from the same generator as the six here.

So the numbers above compare two solvers that have both been tuned on these sets. **To settle it, draw fresh sets
from edai's generator and run:**

```bash
bash compare.sh path/to/problems.json path/to/answers.json
```

It runs edai through its own `runners/computer/run300.c` and `harness.c`, unmodified (only the problem table is
generated, by `to_stream_data.py`, which reproduces `runners/computer/expected.txt` line for line on the main set),
runs both versions of beat, and scores all three with `score.py` and the set's key.

## How it works

Each problem is answered from its own examples only.

- **Hidden rules x → y.** Enumerates about 300,000 small integer formulas (`+ − × // % & | ^ >> <<`, squares, digit
  functions, clamps, operations on `a·x + b`), each with `y = a·f(x) + b` on top, and keeps the shortest that fits every
  example exactly. It also tries formulas in two to four pieces, split by parity, `x % m`, sign or a cut point; the cut
  sits mid-way between examples. When every output of a piece is a perfect square, it also looks for the formula being
  squared. A formula is accepted only if it is much shorter, in bits, than the examples it explains, which keeps it
  from fitting noise. Rules are found with integer arithmetic only.
- **deep** adds chains of up to three operations, each with its own constant, such as `((x & 35) ^ 3) % 7`, also used
  inside pieces split by sign, parity and `x % 3`.
- **Sequences.** An exact linear recurrence (up to three earlier terms plus a polynomial in n), or two or three
  alternating steps `a·t + b`, solved over the rationals.
- **Trends or noise.** A weighted average of the 10 best of 24 smooth fits (polynomials of degree 1–4 and Gaussian-process
  smoothers around a line), each weighted by its leave-one-out error. With 5 or more examples, the answer is CANNOT if
  even the best fit predicts the interior examples it did not see poorly against the spread of the readings (above the
  cut). The estimates use only `+ − × /` and the file's own `exp` and `sqrt`, so every IEEE-754 machine gives the same
  answers.

## Reproduce it

From this folder, with a C compiler (gcc or clang) and python3, nothing else:

```bash
bash run.sh
```

It builds both versions, runs all six sets, scores each with `../../score.py`, and checks every answer against
`results/`. `bash run.sh portable` runs only the fast version (under a minute). `CC=gcc bash run.sh` uses another
compiler.

## Other files

| Path | What it is |
|---|---|
| `c/beat.c`, `c/beat.h` | the solver (portable C99, no allocation, no C library) |
| `c/deep.c` | the larger chain search, included when built with `-DBT_DEEP` |
| `c/main.c`, `c/gen_problems.py` | the program that runs a set, and the script that turns `problems.json` into C arrays |
| `compare.sh`, `to_stream_data.py` | edai and beat on any new set, edai through its own runner |
| `results/portable/`, `results/deep/` | every answer of version 2, one file per set |
| `python/solve.py` | the first version, in Python with numpy (`python3 python/solve.py ../../problems/problems.json > answers.txt`, several minutes); it scored 277 on the main set, and its C port scored 1,353 on the five fresh sets |
| `z3/z3search.py` | formula templates whose constants the Z3 solver finds (too slow for the main path; used to learn which formula shapes occur) |
| `pico/sketch.ino` | an Arduino sketch that runs the solver on a Raspberry Pi Pico 2 |
| `pico/wokwi-quick-log.txt`, `pico/edai-wokwi-quick-log.txt` | both solvers on edai's 9-problem quick set in the Wokwi Pico 2 simulator (version 1 of beat, edai 1.0) |
| `tools/elfsize.py`, `tools/uf2info.py` | read code and RAM size from ELF objects and `.uf2` files, with no toolchain needed |

## On the Pico (simulated)

In Wokwi's simulated Pico 2, an early portable version answered edai's 9-problem quick set 9 of 9 in 75 s of simulated
time, against edai 1.0's 96 s, with the same answers as on the Mac. The full 300 have not been run on a simulated or
real board.

To run it: make a Wokwi Pico 2 project, add `c/beat.c`, `c/beat.h` and a `problems.h` made with
`python3 c/gen_problems.py ../../problems/problems.json problems.h`, and use `pico/sketch.ino` as the sketch.

## Memory

Version 2 against edai 1.2. Code and static RAM are read from objects built with `clang -Os` (`tools/elfsize.py`);
peak stack was measured on an Apple-silicon Mac by running each call on a thread whose stack is filled with a pattern
(2,300 problems: the main set and 2,000 built from extreme int64 values; 400 for deep).

| | edai 1.2 | beat 2, portable | beat 2, deep |
|---|---|---|---|
| code and constants, Cortex-M33 (Pico 2) | 25,017 B | 20,144 B | computers only |
| code and constants, Cortex-M0+ (Pico) | 23,098 B | 19,332 B | computers only |
| static RAM | 0 (state: a 5,216-byte object the caller provides) | 147,384 B | 147,384 B |
| heap | 0 | 0 | about 19 MB, allocated once |
| peak stack, measured on the Mac | 2,640 B | 3,876 B | 4,404 B |
| **total RAM** | **about 8 KB** | about 151 KB | about 19.2 MB |

beat's code is smaller, but edai needs about 18 times less RAM than portable and about 2,400 times less than deep.
Most of portable's static RAM is speed: the table of pieces the tree search examines (40 KB), the one-step formulas
kept ready (21 KB), a cache that skips repeated formulas (32 KB) and the groups that share their first checks (16 KB).
