"""Questions that repeat an example's x: an exact rule must answer that example's y.
   Mix of real rules (which the brain should get), trend-like data and noise of several kinds. No duplicate x."""
import random
rng = random.Random(7)
lines, meta = [], []
FAM = {
  "linear":   lambda x: 3 * x - 7,
  "square":   lambda x: x * x - 5,
  "mod7":     lambda x: x % 7,
  "xor13":    lambda x: x ^ 13,
  "parity":   lambda x: 2 * x if x % 2 == 0 else x - 3,
  "noise2":   None, "noise4": None, "noise1000": None, "trend": None,
}
for fam, f in FAM.items():
    for n in (6, 8, 12, 16):
        for _ in range(250):
            xs = rng.sample(range(-40, 121), n)
            if f: ys = [f(x) for x in xs]
            elif fam == "trend": a, b = rng.uniform(-5, 5), rng.uniform(-100, 100); ys = [round(a * x + b + rng.gauss(0, 15)) for x in xs]
            else: L = int(fam[5:]); ys = [rng.randrange(L) if L < 1000 else rng.randint(-500, 500) for _ in xs]
            i, j = rng.sample(range(n), 2)
            lines.append(f"R {n} {' '.join(map(str, xs))} {' '.join(map(str, ys))} 2 {xs[i]} {xs[j]}")
            meta.append(f"{fam} {n} {ys[i]} {ys[j]}")
open("self.lines", "w").write("\n".join(lines) + "\n"); open("self.meta", "w").write("\n".join(meta) + "\n")
print(len(lines))
