# The probes from issue #1

These are the tests behind [issue #1](https://github.com/devkancheti4-design/edai/issues/1): fixed-seed generators,
the programs that run their problems through the brain's public library (natively and in the WebAssembly build of
`runners/browser`), and a report of what they find. They test the brain only through `brain/include/aa.h`.

```bash
bash run.sh                          # on a Mac with Apple silicon
TARGET=linux-x86_64 bash run.sh      # on Linux (or linux-aarch64)
bash official/run.sh                 # the key cases through runners/computer/run300.c and harness.c, unmodified
```

`run.sh` needs `cc`, `python3` and `node`, and takes a few minutes on all cores. `results-1.1.txt` and `results-1.2.txt`
are its output for versions 1.1 and 1.2 on a Mac. To run it on version 1.0, check out the commit before `e75b0a3`
(keep this folder).

## What each probe tests

| Probe | Generator | Problems | What it checks |
|---|---|---|---|
| F1 | `gen_selfcheck.py` | 9,000 | A question repeats an example's x; an exact answer (`by` = AM or SPLITTER) must then give that example's y. Rules (a line, x², x % 7, x XOR 13, a two-piece parity rule), noisy trends, and noise with 2, 4 and about 1,000 values. Also runs on the WebAssembly build. |
| F2 | `gen_false_exact.py` | 1,600 | A clean line plus one x repeated with a different y, so that no rule can fit: any exact answer is false. 4, 8, 12 and 16 examples. |
| F3 | `gen_rules.py` | 6,000 | Twenty families of integer rules with fresh random constants, questions never equal to an example: right answers, and wrong answers called exact. |
| F4 | `gen_false_exact.py` | 7,200 | Pure noise with 2 to 100 distinct values at 8, 12 and 16 examples: exact answers claimed. |
| 1.1 | `gen_v11.py` | 13,500 | Aimed at 1.1's proof (all examples, or all but the one to three of smallest x with every question at or above the rest): contradicting duplicates at the smallest, middle and largest x; one to three off-rule examples at the smallest x with questions above them or repeating a skipped or a kept example; ties at the edge of the skipped zone. |
| Contract | `tools/contract.c` | 43 cases | Every documented return code, each case in its own process: NULL pointers, ranges, uninitialised and corrupted brains, versions, overlapping buffers, the stream API with NaN, infinity and 3 million observations. |
| Stack | `tools/stack_probe.c` | the main 300 | Each call runs on a thread whose stack is filled with a pattern; the deepest changed byte, minus an empty call through the same path. |

Where `challengers/beat` is present, F2, F3 and F4 also run through it for comparison.

## Results on Apple silicon

| | 1.0 | 1.1 | 1.2 |
|---|---|---|---|
| F1: exact answers that contradict their own examples (of 9,000) | 197 | **0** | **0** |
| F2: exact answers for examples that contradict each other (of 1,600) | 142 | **0** | **0** |
| 1.1 probes: exact answers on impossible data or contradicting an example (of 13,500) | — | **0** | **0** |
| F3: right answers on simple rules (of 6,000) | 2,502 | 2,460 | **2,764** |
| F3: wrong answers called exact | 434 | 241 | 252 |
| F4: exact answers on 0/1 noise with 8 examples (of 400) | 340 | 340 | 340 |
| Contract: cases as documented (of 43) | 42 | **43** | **43** |
| Stack, worst over the main 300 (aa.h: at most 3,072 bytes) | 2,592 bytes | 2,592 bytes | 2,640 bytes |
| Native and WebAssembly give the same answers | 9,000 of 9,000 | 9,000 of 9,000 | 9,000 of 9,000 |

`results-1.2.txt` is the same run on version 1.2 (added by the maintainer). The net's light in 1.2 mostly shows in F3:
linear rules 179 to 289 right (falling lines are now found), cubic 98 to 203, sign-split 111 to 154, quadratic 16 to
33; wrong answers called exact went from 241 to 252 (sign-split 12 to 18, max-const 25 to 28, linear 0 to 1, abs-linear
10 to 11).

Beyond this folder, the same comparison was run on 35,600 problems (the 1,800 published ones, 10,000 built from
extreme int64 values, and the problems above): native and WebAssembly agreed on all of them on both versions, and the
worst stack over 11,800 of them was 2,592 bytes on both.

## The key cases through edai's own runner

`official/run.sh` replaces only the problem table of `runners/computer` (in its own format) and runs `run300.c` and
`harness.c` unmodified:

| Case | 1.0 | 1.1 |
|---|---|---|
| two-piece rule; a question repeats the example −35 → −38 | `by=AM ans=92,4 right=0` | `by=NET ans=92,-38 right=1` |
| x = 5 → 1 and x = 5 → 2: no rule can fit | `by=AM ans=140,160` | `by=NET ans=132,143` (an estimate, not called exact) |
| y = −5x + 60 | `by=NET ans=-91,-434 right=0` | the same |
| y = x(x+1)/2 | `by=NET ans=394,4564 right=0` | the same |
| y = x² − 3x + 2 | `by=NET ans=673,8737 right=0` | the same |
| y = 2x + 7 | `by=AM ans=67,207 right=1` | the same |
