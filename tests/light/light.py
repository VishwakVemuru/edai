"""What the net's light and doubts add (version 1.2), through the brain's public library: rules AM's steps reach only
with the net pointing the way, and rules with examples that are wrong.  Fixed seed; each kind of test, 300 problems.
  - falling lines y = -a x + b (a 2..9): AM's steps cannot build a falling line on their own;
  - steep lines y = a x + b (a 10..40);
  - a line y = a x + b (a 2..9) with one wrong example, and with two (12 examples);
  - plain noise: the answer must be CANNOT.
Right means both answers exactly the rule's values (CANNOT for noise).  Also counts wrong answers called exact.

    python3 light.py [../exactness/answer]      (make builds it against ../../brain/lib/<target>/libaa.a)
"""
import random, subprocess, sys

PROG = sys.argv[1] if len(sys.argv) > 1 else "../exactness/answer"
G = random.Random(2027)


def line(pid, xs, ys, qs):
    return f"{pid} {len(xs)} {' '.join(map(str, xs))} {' '.join(map(str, ys))} {len(qs)} {' '.join(map(str, qs))}"


def answers(lines):
    out = subprocess.run([PROG], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True).stdout.split("\n")
    return {f[0]: (f[1], [int(v) for v in f[2].split(",")]) for f in (ln.split() for ln in out if ln.strip())}


def rule_rows(tag, f, n_choices=(8, 12), bad=0):
    n = G.choice(n_choices); xs = sorted(G.sample(range(-40, 121), n)); ys = [f(x) for x in xs]
    for _ in range(bad):
        j = G.randrange(1, n - 1); ys[j] += G.choice([-1, 1]) * G.randint(5, 60)
    order = list(range(n)); G.shuffle(order)
    qs = G.sample([v for v in range(-40, 151) if v not in xs], 2)
    return line(tag, [xs[j] for j in order], [ys[j] for j in order], qs), [f(q) for q in qs]


def report(name, rows, noise=False):
    res = answers([r[0] for r in rows]); right = exact_wrong = 0; by = {}
    for ln, truth in rows:
        b, a = res[ln.split()[0]]; by[b] = by.get(b, 0) + 1
        ok = b == "CANNOT" if noise else (b != "CANNOT" and a == truth)
        right += ok; exact_wrong += b in ("AM", "SPLITTER") and not ok
    print(f"{name:52s} right {right:3d} of {len(rows)}   wrong answers called exact {exact_wrong:2d}   who answered {by}")


report("falling lines y = -a x + b (a 2..9)", [rule_rows(f"f{i}", lambda x, a=G.randint(2, 9), b=G.randint(-50, 50): -a * x + b) for i in range(300)])
report("steep lines y = a x + b (a 10..40)", [rule_rows(f"s{i}", lambda x, a=G.randint(10, 40), b=G.randint(-50, 50): a * x + b) for i in range(300)])
report("a line with one wrong example", [rule_rows(f"b{i}", lambda x, a=G.randint(2, 9), b=G.randint(-50, 50): a * x + b, bad=1) for i in range(300)])
report("a line with two wrong examples (12 examples)", [rule_rows(f"c{i}", lambda x, a=G.randint(2, 9), b=G.randint(-50, 50): a * x + b, (12,), 2) for i in range(300)])
rows = []
for i in range(300):
    n = G.choice([8, 12]); xs = G.sample(range(-40, 121), n); ys = [G.randint(-500, 500) for _ in xs]
    rows.append((line(f"n{i}", xs, ys, G.sample(range(-40, 121), 2)), None))
report("plain noise (CANNOT is right)", rows, noise=True)
