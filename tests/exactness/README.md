# Exact answers that contradict their examples

[Issue #1](https://github.com/devkancheti4-design/edai/issues/1) found that version 1.0 of the brain sometimes called
an answer exact (`AA_AM` or `AA_SPLITTER`) when it contradicted the problem's own examples. This test regenerates the
issue's kinds of problem with a fixed seed and runs them through the brain's public library:

| Test | Problems |
|---|---|
| a two-piece rule (x − 3 for odd x, 2x for even x), where one question repeats an example's x | 1,000 |
| a clean line, plus one x repeated with a different y, so that no rule can fit (8 and 12 examples) | 400 each |
| noise taking only 2 or 4 values, where one question repeats an example's x | 400 each |

An exact answer contradicts its examples when its answer to a repeated x differs from that example's y, or when the
examples contradict each other. It also runs the issue's four example problems.

```bash
make                      # on a Mac with Apple silicon
make TARGET=linux-x86_64  # or linux-aarch64, on Linux
```

| | Version 1.0 | Version 1.1 |
|---|---:|---:|
| two-piece rule: exact answers / contradicting | 939 / 16 | 907 / **0** |
| a line plus a contradicting example, 8 examples | 142 / 142 | 0 / **0** |
| the same, 12 examples | 84 / 84 | 0 / **0** |
| noise with 2 values | 341 / 0 | 341 / **0** |
| noise with 4 values | 389 / 87 | 199 / **0** |
| **all 2,600: exact answers that contradict their examples** | **329** | **0** |

The Makefile shows how to run the same test on version 1.0 from this repository's history. Noise with only two values
still gets an exact answer on 341 of 400 draws: 8 such examples often fit a short rule by chance, and that rule does
fit every example.
