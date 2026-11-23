"""Problems where any exact-rule answer is provably false or unjustified:
   contradictory examples (one x twice with different y), and pure noise with few output levels.
   Writes cases.lines (for native_solve / wasm_solve) and cases.meta (one tag per line)."""
import random, sys
rng = random.Random(20261122)
N_PER = int(sys.argv[1]) if len(sys.argv) > 1 else 400
lines, meta = [], []
def qs(): return rng.sample(range(-40, 121), 2)
# contradictory: a clean linear rule, then one x repeated with a different y
for n in (4, 8, 12, 16):
    for _ in range(N_PER):
        xs = rng.sample(range(-40, 121), n - 1)
        a, b = rng.randint(-9, 9) or 1, rng.randint(-50, 50)
        ys = [a * x + b for x in xs]
        dup = rng.randrange(n - 1)
        xs.append(xs[dup]); ys.append(ys[dup] + rng.choice([-1, 1]) * rng.randint(1, 50))
        order = list(range(n)); rng.shuffle(order)
        xs = [xs[i] for i in order]; ys = [ys[i] for i in order]
        lines.append(f"R {n} {' '.join(map(str, xs))} {' '.join(map(str, ys))} 2 {' '.join(map(str, qs()))}")
        meta.append(f"contra n={n}")
# pure noise with L output levels
for L in (2, 3, 4, 8, 16, 100):
    for n in (8, 12, 16):
        for _ in range(N_PER):
            xs = rng.sample(range(-40, 121), n); ys = [rng.randrange(L) for _ in xs]
            lines.append(f"R {n} {' '.join(map(str, xs))} {' '.join(map(str, ys))} 2 {' '.join(map(str, qs()))}")
            meta.append(f"noise L={L} n={n}")
open("cases.lines", "w").write("\n".join(lines) + "\n"); open("cases.meta", "w").write("\n".join(meta) + "\n")
print(len(lines), "cases")
