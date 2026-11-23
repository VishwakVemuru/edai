# What the net's light and doubts add (version 1.2)

In version 1.2, when AM's first look finds no rule it can prove, the net learns the examples and shows AM where to search
again: which way the rule runs and how steeply (the light), and which examples look wrong (the doubts). This test
regenerates, with a fixed seed, the kinds of problem where that matters, and runs them through the brain's public
library (the same `answer` program as `../exactness`).

```bash
make                      # on a Mac with Apple silicon
make TARGET=linux-x86_64  # or linux-aarch64, on Linux
```

| 300 problems each | Version 1.1 | Version 1.2 |
|---|---:|---:|
| falling lines y = −a·x + b, a from 2 to 9 | 2 | **295** |
| steep lines y = a·x + b, a from 10 to 40 | 120 | **174** |
| a line y = a·x + b with one wrong example | 33 | **243** |
| a line with two wrong examples (12 examples) | 12 | **149** |
| plain noise (CANNOT is right) | 297 | 297 |

Right means both answers exactly the rule's values (CANNOT for noise). Wrong answers called exact: 0 and 0 on falling
lines, 0 and 2 on steep lines, 9 and 11 with one wrong example, 0 and 1 with two. An answer from the doubts is
reported as `AM_EST`: AM's rule on the examples it kept, an estimate, since it disagrees with the examples set aside.

The Makefile shows how to run the same test on version 1.1 from this repository's history.
