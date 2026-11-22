"""Exact answers that contradict their own examples (GitHub issue #1), through the brain's public library.

Five kinds of test, regenerated with a fixed seed, as the issue describes them:
  - a two-piece rule (x - 3 for odd x, 2x for even x), where one question repeats an example's x;
  - a clean line plus one x repeated with a different y (no rule can fit: 8 and 12 examples);
  - noise taking only 2 or 4 values, where one question repeats an example's x.
An exact answer (AM or SPLITTER) contradicts the examples when its answer to a repeated x differs from that example's y,
or when the examples themselves contradict each other.  Also the issue's four example problems.

    python3 exactness.py [./answer]          (make builds ./answer against ../../brain/lib/<target>/libaa.a)
"""
import collections, random, subprocess, sys

PROG = sys.argv[1] if len(sys.argv) > 1 else "./answer"
R = random.Random(2026)


def line(i, xs, ys, qs):
    return f"s{i} {len(xs)} {' '.join(map(str, xs))} {' '.join(map(str, ys))} {len(qs)} {' '.join(map(str, qs))}"


def answers(lines):
    out = subprocess.run([PROG], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout.split("\n")
    return [ln.split() for ln in out if ln.strip()]


def report(name, rows, contradicts):
    res = answers([r[0] for r in rows]); c = collections.Counter()
    for (ln, meta), (_, by, ans) in zip(rows, res):
        exact = by in ("AM", "SPLITTER")
        c["exact"] += exact
        c["bad"] += exact and contradicts(meta, [int(v) for v in ans.split(",")])
    print(f"{name:62s} {len(rows):5d} | exact answers {c['exact']:4d} | exact but contradicting the examples {c['bad']:4d}")
    return c["bad"]


bad = 0
print(f"{'test':62s} {'n':>5s} |")
rows = []
for i in range(1000):
    n = R.choice([6, 8]); xs = R.sample(range(-40, 121), n); ys = [x - 3 if x % 2 else 2 * x for x in xs]
    k = R.randrange(n); qs = [R.choice(range(-40, 121)), xs[k]]
    rows.append((line(i, xs, ys, qs), ys[k]))
bad += report("two-piece rule, one question repeats an example", rows, lambda y, a: a[1] != y)
for n in (8, 12):
    rows = []
    for i in range(400):
        a, b = R.randint(2, 9), R.randint(-20, 20); xs = R.sample(range(-40, 121), n - 1); xs.append(xs[0])
        ys = [a * x + b for x in xs]; ys[-1] += R.choice([-3, -2, -1, 1, 2, 3]); qs = R.sample(range(-40, 121), 2)
        rows.append((line(i, xs, ys, qs), None))
    bad += report(f"a line, plus one x repeated with a different y ({n} examples)", rows, lambda y, a: True)
for vals in (2, 4):
    rows = []
    for i in range(400):
        xs = R.sample(range(-40, 121), 8); ys = [R.randrange(vals) for _ in xs]; k = R.randrange(8); qs = [R.choice(range(-40, 121)), xs[k]]
        rows.append((line(i, xs, ys, qs), ys[k]))
    bad += report(f"noise with {vals} values (8 examples), one question repeats an x", rows, lambda y, a: a[1] != y)

print("\nthe issue's four examples:")
x3 = [88, 5, -31, -12, 40, 17, 63, -2]
cases = [("examples include -35 -> -38; asks 95, -35", [83, 2, 73, -35, 95, 59], [80, 4, 70, -38, 92, 56], [95, -35]),
         ("x = 5 -> 1 and x = 5 -> 2 (no rule fits)", [5, 5, 10, 20, 30, 40, 50, 60], [1, 2, 20, 40, 60, 80, 100, 120], [70, 80]),
         ("y = -5x + 60 (truth -90, -440)", x3, [-5 * x + 60 for x in x3], [30, 100]),
         ("y = 2x + 7 (truth 67, 207)", x3, [2 * x + 7 for x in x3], [30, 100])]
for (name, xs, ys, qs), (_, by, ans) in zip(cases, answers([line(i, xs, ys, qs) for i, (_, xs, ys, qs) in enumerate(cases)])):
    print(f"  {name:44s} {by:8s} {ans.replace(',', ', ')}")
print("\nEXACTNESS " + ("PASSED: no exact answer contradicts its examples" if bad == 0 else f"FAILED: {bad} exact answers contradict their examples"))
sys.exit(1 if bad else 0)
