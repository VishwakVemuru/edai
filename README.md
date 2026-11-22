# edai: the 300-problem challenge

A small brain for edge devices answers 300 problems from a handful of examples each. It joins AM, a reasoner that
finds the exact rule behind examples, with a small neural net that estimates when no rule exists and checks itself
first. It scores **275 of 300**, holds that score on 1,500 problems nobody had seen, and needs only **about 8 KB of
memory**. It is supplied here **compiled**, so that **anyone can replicate the result** on their own device, and anyone
can try to beat it.

## The 300 problems

| Kind | How many | Right means |
|---|---|---|
| Hidden rules: examples of an integer rule x → y (or the first terms of a sequence), and two questions | 120 | both answers exact |
| Noisy trends: noisy readings along a smooth trend, and two points to estimate | 120 | both estimates within the problem's tolerance |
| Pure noise: examples that follow no rule | 60 | the answer CANNOT |

- `problems/problems.json` holds the problems.
- `problems/answers.json` is the key: the kind of each problem, its answer and, for trends, the tolerance.
- `score.py` scores any answers file.

## Results so far

| Device | How it ran | Score | Rules · trends · noise | Time for all 300 |
|---|---|---|---|---|
| Mac, Apple silicon | `runners/computer` (native library) | **275** | 97 · 119 · 59 | 6.3 s |
| Mac, Apple silicon | `runners/browser` (in the browser) | **275** | 97 · 119 · 59 | 13.2 s |
| iPhone | `runners/browser` (in the browser) | **275** | not recorded | not recorded |
| Other phones | `runners/browser` | not run yet | | |
| Raspberry Pi Pico 2, simulated in Wokwi | `runners/wokwi`, quick set of 9 | **9 of 9** | 3 · 3 · 3 | 96 s (simulated) |
| Raspberry Pi Pico 2 or Pico, a real board | `runners/pico` | waiting for a board | | |

On the Mac the native run gives, problem by problem, the very same line as the reference in
`runners/computer/expected.txt`: who answered, both answers, the self-check, right or wrong. An iPhone, running the
browser page, gave the same score: 275. On the simulated Pico 2, the 9 problems of the quick set gave the same answers
as the reference (`results/wokwi-pico2-quick.txt`, recorded while the challenger below was being built).

Who answered the 300: AM 80 (exact rules), AM's splitter 29 (exact rules in pieces), the net 127 (estimates that
passed its self-check), and CANNOT 64.

The brain does each step of its maths the same way on every target, so every device should give the same 300 lines.
Running it on yours is the test.

## Fresh problems

The 300 problems are public with their key, so a solver can end up tuned to them without meaning to. To check that a
score holds on problems nobody has seen, five more sets of 300 were drawn from the same generator. The brain's
settings had been chosen beforehand on a separate design set of 150 problems, and none of these sets was used to
choose anything. The brain, unchanged, scored:

| Set | Score | Rules · trends · noise |
|---|---|---|
| the main set (`problems/`) | 275 | 97 · 119 · 59 |
| fresh, seed 23 | 270 | 94 · 117 · 59 |
| fresh, seed 24 | 278 | 100 · 118 · 60 |
| fresh, seed 25 | 269 | 98 · 111 · 60 |
| fresh, seed 26 | 285 | 106 · 119 · 60 |
| fresh, seed 27 | 272 | 96 · 116 · 60 |
| **all five fresh sets** | **1,374 of 1,500** | 494 · 581 · 299 |

That is 274.8 per set on average, the same as its 275 on the main set.

To replicate it, run `cd runners/computer && make fresh`. It runs the brain on each fresh set, scores it, and compares
every line with that set's reference.

## Real sensor data

The same brain, unchanged, was also run on real sensors:
- temperature sensors that drift and fail (Intel Berkeley Lab);
- a motor that develops a bearing fault (Case Western Reserve University);
- a MacBook's chip temperature, read live;
- an iPhone's tilt sensor, with the brain running on the phone.

It followed drift and smooth movement closely, and answered exactly when the motor's behaviour changed. It refused
readings that follow no pattern: 97% of scrambled readings and 91% of random shaking. Given a neighbouring sensor, it
caught a drifting one up to 14 hours before the reading passed 35 °C. The numbers, and how to replicate them, are in
[`sensors/README.md`](sensors/README.md).

## Challengers

[`challengers/beat`](challengers/beat/README.md) is an open-source solver written to beat this brain, with every source
file, answer and script. Its author designed it on the main set and fresh seeds 23 and 24, and held seeds 25 to 27 back.

| | edai | beat, portable | beat, deep |
|---|---|---|---|
| **held-out sets (seeds 25–27)** | **826** | 810 | 821 |
| all six sets (including beat's design sets) | 1,649 | 1,646 | 1,666 |
| memory | **about 8 KB** | 147 KB of static RAM | a few MB (computers only) |

On the held-out sets beat leads on rules (302 against 300) and edai leads on trends (346 against 340) and noise (180
against 179). edai chose its settings on a separate design set and used none of these sets, so 826 is the cleaner
number. beat's own README says where each of its sets was used.

## How it fits in 8 KB of memory

The brain keeps everything in one object of 5,216 bytes, which you place wherever you like (static memory, a task's
stack, a pool). It reports this itself: on the Pico 2 it prints `brain 5216 bytes, stack bound 2960 bytes`.

| What the 5,216 bytes hold | Bytes |
|---|---|
| the problem in hand: up to 16 examples and 2 questions | 288 |
| the net's 289 learned numbers | 1,156 |
| what learning needs: two running averages and the current slope for each number | 3,468 |
| the 72 units' values and two counters | 296 |
| a check word | 8 |
| **total** | **5,216** |

It needs so little because:
1. **It keeps no model.** It learns each problem from that problem's own few examples, from scratch, and keeps nothing
   afterwards, so there are no stored weights and no training data. The same 5,216 bytes serve every problem.
2. **The net is small:** 289 numbers in 32-bit floating point. With what learning needs, that is 4,920 bytes.
3. **AM keeps no tables.** It tries each short chain of integer steps in turn and keeps only the best answer for each
   part of the problem, so its search fits on the stack.
4. **No heap, and no globals.** The library never allocates memory and has no global or static data of its own: all
   state is in the object you own.
5. **The stack is bounded and measured:** the deepest any call goes is worked out from the compiler's frame sizes. It is
   at most 3,072 bytes on any target, and 2,960 bytes on the Pico 2's Cortex-M33.
6. **Its maths is its own.** `exp`, `tanh` and square roots are computed by the library's own code, with no tables and no
   maths library. That is also why every chip gives the same answers.

The trade is time. Because it learns on every question, it spends time where other solvers spend memory: about 20 ms
per problem on a Mac and 69 ms per sensor question on an iPhone. On the simulated 150 MHz Pico 2 an exact rule takes
about 45 ms, and a problem the net must learn takes about 15 s.

## Replicate it

**In a browser, on any phone or computer.** Open `runners/browser/index.html` and press **Run**. Once it's open, it
needs no network. To reach a phone, serve the folder from a computer on the same Wi-Fi:

```bash
cd runners/browser && python3 -m http.server 8000
```

Then open `http://<that computer's address>:8000` on the phone.

**Natively on a Mac or a Linux computer.**

```bash
cd runners/computer && make check
```

On Linux, use `make check TARGET=linux-x86_64` or `TARGET=linux-aarch64`. It prints the score and compares every line
with the reference.

**On a Raspberry Pi Pico 2, or the original Pico.** Hold BOOTSEL, plug the board into a Mac, then run:

```bash
bash runners/pico/run.sh
```

It copies the matching program onto the board, runs the 300 problems on the chip, and checks every line. On Linux,
copy the `.uf2` file onto the board's drive and run `python3 runners/pico/read.py`.

**On a simulated Pico 2, with no hardware.** Open https://wokwi.com/projects/new/pi-pico-2, press F1 in the code editor,
choose **Upload Firmware and Start Simulation...**, and pick a file from `runners/wokwi`. See `runners/wokwi/README.md`.

**Check the files.** `SHA256SUMS.txt` lists the fingerprint of every compiled file:

```bash
shasum -a 256 -c SHA256SUMS.txt
```

## Beat it

Write your own solver. It gives one line per problem, either `p000 210,205` or `p001 CANNOT`. Then:

```bash
python3 score.py your-answers.txt
```

Use only each problem's own examples; the key is for scoring. A solver tuned to the main set usually scores lower on
fresh problems, so score the five fresh sets too, each with its own key:

```bash
python3 score.py your-answers-23.txt --key problems/fresh/seed-23/answers.json
```

## What is here

| Path | What it holds |
|---|---|
| `problems/` | the 300 problems and the answer key |
| `problems/fresh/` | five fresh sets of 300 problems, each with its own key |
| `score.py` | the scorer |
| `brain/include/aa.h`, `brain/lib/<target>/libaa.a` | the compiled brain for 9 targets: Cortex-M0+, M4F, M33 and M7; RISC-V (imc, imafc); macOS on Apple silicon; Linux on x86-64 and AArch64 |
| `runners/browser/` | one page with the brain compiled to WebAssembly inside it |
| `runners/computer/` | a small C program that runs the 300 problems through the library, and the fresh sets (`make fresh`) |
| `runners/pico/` | the programs for a Pico 2 and the original Pico, the reader and checker, and the program's source for reference |
| `runners/wokwi/` | the programs for a simulated Pico 2 in the Wokwi browser simulator, and a checker |
| `sensors/` | the brain on real sensor data: the tools, the phone page and the results |
| `challengers/beat/` | an open-source challenger, with its source, answers and results |
| `results/results.json` | the results above |

## About the brain

- **What it does:** when an exact rule behind the examples exists, AM finds it. When none exists, the net learns the
  examples and estimates, but only after checking itself on examples it hid from itself. Otherwise it says CANNOT.
- **What it needs:** about 8 KB of memory (5,216 bytes of state and at most 3 KB of stack), and no training data,
  network, operating system or memory allocation. See "How it fits in 8 KB of memory" above.
- **Its size:** on an Arm microcontroller a complete program around it takes about 28 KB of program space.
- **How it is supplied:** compiled. These files are here so that anyone can check the results.
