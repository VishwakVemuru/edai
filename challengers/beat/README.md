# beat: an open-source challenger to the edai brain

An attempt to beat the edai brain on the same 300-problem challenge, written in the open: every source file, every
answer and every script is here. **It did not beat edai on problems neither had seen**, but it came close, and it beats
edai on the rule problems.

## Results

Scored with the repository's `score.py`, each set with its own key.

| Set | edai | beat, portable | beat, deep |
|---|---|---|---|
| main (`problems/`) | 275 | 280 | 283 |
| fresh, seed 23 | 270 | 273 | 275 |
| fresh, seed 24 | 278 | 283 | 287 |
| fresh, seed 25 | 269 | 269 | **272** |
| fresh, seed 26 | **285** | 275 | 276 |
| fresh, seed 27 | 272 | 266 | **273** |
| **held-out sets (seeds 25–27)** | **826** | 810 | 821 |
| all six sets | 1,649 | 1,646 | 1,666 |

On the held-out sets, by kind of problem:

| | edai | beat, deep |
|---|---|---|
| Rules | 300 | **302** |
| Trends | **346** | 340 |
| Noise | **180** | 179 |

- **portable** is plain C99 with no allocation and no C library: about 19 KB of code on a Cortex-M33 or M0+ and
  147 KB of static RAM, so it fits a Raspberry Pi Pico or Pico 2.
- **deep** adds a larger search (`c/deep.c`) that needs a few MB and runs only on a computer: about 90 s per set of
  300 on an Apple-silicon Mac, against about 6 s for portable.

## How the sets were used: read this before comparing

- The **main set** was used to design and tune the first version (it scored 277 there). That version then scored
  1,353 on the five fresh sets against edai's 1,374, which showed the tuning did not carry over.
- After that, the main set and **seeds 23 and 24** were the design sets: failures there were studied, with their
  answers, to decide what to add.
- **Seeds 25, 26 and 27** were held out. No problem in them was ever looked at individually, but their total score was
  checked twice (810 after the first round of changes, 821 after the second). Treat 821 as slightly optimistic.
- edai chose its settings beforehand on a separate design set of 150 problems and used none of these sets, so on the
  held-out sets its 826 is the cleaner number.

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
  smoothers around a line), each weighted by its leave-one-out error. If even the best fit predicts the examples it did
  not see little better than their average, the answer is CANNOT. The estimates use only `+ − × /` and the file's own
  `exp` and `sqrt`, so every IEEE-754 machine gives the same answers.

## Reproduce it

From this folder, with a C compiler (gcc or clang) and python3, nothing else:

```bash
bash run.sh
```

It builds both versions, runs all six sets, scores each with `../../score.py`, and checks every answer against
`results/`. `bash run.sh portable` runs only the fast version (under a minute). `CC=gcc bash run.sh` uses another
compiler. On Apple silicon both versions gave identical answers in every run.

## Other files

| Path | What it is |
|---|---|
| `c/beat.c`, `c/beat.h` | the solver (portable C99, no allocation, no C library) |
| `c/deep.c` | the larger chain search, included when built with `-DBT_DEEP` |
| `c/main.c`, `c/gen_problems.py` | the program that runs a set, and the script that turns `problems.json` into C arrays |
| `results/portable/`, `results/deep/` | every answer, one file per set |
| `python/solve.py` | the first version, in Python with numpy (`python3 python/solve.py ../../problems/problems.json > answers.txt`, several minutes); it scored 277 on the main set, and its C port scored 1,353 on the five fresh sets |
| `z3/z3search.py` | formula templates whose constants the Z3 solver finds (too slow for the main path; used to learn which formula shapes occur) |
| `pico/sketch.ino` | an Arduino sketch that runs the solver on a Raspberry Pi Pico 2 |
| `pico/wokwi-quick-log.txt`, `pico/edai-wokwi-quick-log.txt` | both solvers on edai's 9-problem quick set in the Wokwi Pico 2 simulator |
| `tools/elfsize.py`, `tools/uf2info.py` | read code and RAM size from ELF objects and `.uf2` files, with no toolchain needed |

## On the Pico (simulated)

In Wokwi's simulated Pico 2, an earlier portable version (before the wide bit masks and the square step) answered edai's
9-problem quick set 9 of 9 in 75 s of simulated time, against edai's 96 s, with the same answers as on the Mac. The
full 300 have not been run on a simulated or real board.

To run it: make a Wokwi Pico 2 project, add `c/beat.c`, `c/beat.h` and a `problems.h` made with
`python3 c/gen_problems.py ../../problems/problems.json problems.h`, and use `pico/sketch.ino` as the sketch.

Measured sizes of the portable version (`tools/elfsize.py` on objects from `clang -Os`):

| Chip | Code and constants | Static RAM | Largest stack frame |
|---|---|---|---|
| Cortex-M33 (Pico 2) | 19,704 B | 147,384 B | 2,760 B |
| Cortex-M0+ (Pico) | 18,932 B | 147,384 B | 2,760 B |

edai's brain is about 21.7 KB of code and keeps 5.2 KB of state, so it needs far less memory.
