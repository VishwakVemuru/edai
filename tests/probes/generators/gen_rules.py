"""Genuine integer rules, each with fresh random constants; questions never repeat an example's x.
   Families range from ones edai's problems contain (linear, parity, mod, xor) to plain math it may not search."""
import random
rng = random.Random(99)
def dsum(v): return sum(map(int, str(abs(v))))
FAM = [
  ("linear",       lambda r: (lambda a, b: lambda x: a * x + b)(r.choice([-9,-7,-5,-3,-2,2,3,4,5,6,7,8,9]), r.randint(-60, 60))),
  ("quadratic",    lambda r: (lambda a, b, c: lambda x: a*x*x + b*x + c)(r.choice([-3,-2,-1,1,2,3]), r.randint(-9, 9), r.randint(-30, 30))),
  ("cubic",        lambda r: (lambda a, c: lambda x: a*x**3 + c)(r.choice([-2,-1,1,2]), r.randint(-30, 30))),
  ("cubic_full",   lambda r: (lambda a, b, c: lambda x: a*x**3 + b*x + c)(r.choice([-2,-1,1,2,3]), r.randint(-9, 9), r.randint(-30, 30))),
  ("mod_small",    lambda r: (lambda m: lambda x: x % m)(r.randint(3, 12))),
  ("mod_prime",    lambda r: (lambda m: lambda x: x % m)(r.choice([13,17,19,23,29,31,37,41,43,47]))),
  ("affine_mod",   lambda r: (lambda a, b, m: lambda x: (a*x + b) % m)(r.randint(2, 9), r.randint(0, 20), r.choice([7,11,13,17,23,31]))),
  ("square_mod",   lambda r: (lambda m: lambda x: (x*x) % m)(r.choice([7,10,11,13,17]))),
  ("xor",          lambda r: (lambda k: lambda x: x ^ k)(r.randint(1, 255))),
  ("and",          lambda r: (lambda k: lambda x: x & k)(r.randint(1, 255))),
  ("floordiv",     lambda r: (lambda k: lambda x: x // k)(r.randint(2, 20))),
  ("abs_linear",   lambda r: (lambda a, b: lambda x: a*abs(x) + b)(r.randint(2, 7), r.randint(-20, 20))),
  ("max_const",    lambda r: (lambda k, a: lambda x: a*max(x, k))(r.randint(-20, 40), r.randint(1, 5))),
  ("sign_split",   lambda r: (lambda a, b, c, d: lambda x: a*x + b if x < 0 else c*x + d)(r.randint(-5, 5), r.randint(-20, 20), r.randint(-5, 5), r.randint(-20, 20))),
  ("parity_split", lambda r: (lambda a, b, c, d: lambda x: a*x + b if x % 2 == 0 else c*x + d)(r.randint(1, 9), r.randint(-20, 20), r.randint(1, 9), r.randint(-20, 20))),
  ("digit_sum",    lambda r: (lambda a: lambda x: a * dsum(x))(r.randint(1, 5))),
  ("triangle",     lambda r: (lambda c: lambda x: x*(x+1)//2 + c)(r.randint(-20, 20))),
  ("x_times_mod",  lambda r: (lambda m: lambda x: x * (x % m))(r.randint(3, 9))),
  ("two_bits",     lambda r: (lambda k1, k2: lambda x: (x | k1) ^ k2)(r.randint(1, 63), r.randint(1, 63))),
  ("shift_add",    lambda r: (lambda k, c: lambda x: (x << k) + c)(r.randint(1, 4), r.randint(-50, 50))),
]
lines, meta = [], []
for name, make in FAM:
    for n in (8, 12):
        for _ in range(150):
            f = make(rng)
            pool = rng.sample(range(-40, 121), n + 2)
            xs, qs = pool[:n], pool[n:]
            ys = [f(x) for x in xs]; ans = [f(q) for q in qs]
            lines.append(f"R {n} {' '.join(map(str, xs))} {' '.join(map(str, ys))} 2 {qs[0]} {qs[1]}")
            meta.append(f"{name} {n} {ans[0]} {ans[1]}")
open("rules.lines", "w").write("\n".join(lines) + "\n"); open("rules.meta", "w").write("\n".join(meta) + "\n")
print(len(lines))
