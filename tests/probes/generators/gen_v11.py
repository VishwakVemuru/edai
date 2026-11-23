"""Probes aimed at version 1.1's proof rule: an exact answer stands if one rule fits all examples, or all but the
one to three of smallest x with every question at or above the rest; contradicting examples prove nothing.
Each line's meta: tag, then 'IMPOSSIBLE' if no function fits the examples, then for each question either the y of
the example it repeats or '-' (free)."""
import random
rng = random.Random(1123)
RULES = [lambda x: 3 * x + 1, lambda x: x % 7, lambda x: 2 * x if x % 2 == 0 else x - 3, lambda x: x * x - 5, lambda x: x ^ 13]
lines, meta = [], []
def emit(tag, xs, ys, qs, impossible):
    ex = {}
    for x, y in zip(xs, ys): ex.setdefault(x, y)
    marks = [str(ex[q]) if (q in ex and not impossible) else "-" for q in qs]
    lines.append(f"R {len(xs)} {' '.join(map(str, xs))} {' '.join(map(str, ys))} {len(qs)} {' '.join(map(str, qs))}")
    meta.append(f"{tag} {'IMPOSSIBLE' if impossible else 'ok'} {' '.join(marks)}")
for _ in range(300):
    for ri, f in enumerate(RULES):
        n = rng.choice([8, 12, 16])
        xs = sorted(rng.sample(range(-40, 121), n - 1))
        ys = [f(x) for x in xs]
        # A: one contradicting duplicate at the smallest, middle or largest x
        for where, idx in (("min", 0), ("mid", (n - 1) // 2), ("max", n - 2)):
            x2, y2 = xs + [xs[idx]], ys + [ys[idx] + rng.choice([-1, 1]) * rng.randint(1, 9)]
            emit(f"A-contra-{where}", x2, y2, rng.sample(range(-40, 121), 2), True)
        # B: 1-3 off-rule examples at the smallest x, questions above them (free) or repeating a skipped x
        k = rng.randint(1, 3)
        xb = sorted(rng.sample(range(-40, 121), n))
        yb = [f(x) for x in xb]
        for j in range(k): yb[j] = yb[j] + rng.choice([-1, 1]) * rng.randint(1, 20)
        hi = [q for q in range(xb[k], 121) if q not in xb]
        emit(f"B-skip{k}-above", xb, yb, rng.sample(hi, 2), False)
        emit(f"B-skip{k}-repeat-skipped", xb, yb, [xb[rng.randrange(k)], rng.choice(hi)], False)
        emit(f"B-skip{k}-repeat-kept", xb, yb, [xb[rng.randrange(k, n)], rng.choice(hi)], False)
        # C: ties at the edge of the skipped zone
        xc = sorted(rng.sample(range(-40, 121), n - 1)); yc = [f(x) for x in xc]
        for j in range(2): yc[j] += rng.randint(1, 9)
        t = xc[2]
        emit("C-tie-same-y", xc + [t], yc + [yc[2]], [t, rng.choice(range(t, 121))], False)
        emit("C-tie-diff-y", xc + [t], yc + [yc[2] + 5], [t, rng.choice(range(t, 121))], True)
        emit("C-tie-skipped-vs-kept", xc + [xc[1]], yc + [f(xc[1])], [xc[1], rng.choice(range(xc[3], 121))], True)
open("v11.lines", "w").write("\n".join(lines) + "\n"); open("v11.meta", "w").write("\n".join(meta) + "\n")
print(len(lines), "probes")
