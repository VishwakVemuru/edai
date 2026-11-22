"""Exact rule search with the Z3 solver: formula templates whose constants Z3 solves for (32-bit integers).
   Ops follow Python semantics on integers: + * & | ^ // % (floor), >>, <<."""
import itertools, time, z3

OPS = ["add", "mul", "and", "or", "xor", "fdiv", "mod", "shr"]

def apply(op, a, c):
    if op == "add": return a + c
    if op == "mul": return a * c
    if op == "and": return a & c
    if op == "or": return a | c
    if op == "xor": return a ^ c
    if op == "fdiv": return z3.If(z3.SRem(a, c) == 0, a / c, z3.If(a < 0, a / c - 1, a / c))   # / is signed, truncating
    if op == "mod": return a % c                  # bvsmod: sign of divisor, so Python % for c > 0
    if op == "shr": return a >> c
    raise ValueError(op)

def cons(op, c):
    if op in ("fdiv", "mod"): return z3.And(c >= 2, c <= 128)
    if op == "shr": return z3.And(c >= 1, c <= 6)
    if op == "mul": return z3.And(c >= -16, c <= 16, c != 0, c != 1)
    if op == "add": return z3.And(c >= -300, c <= 300, c != 0)
    return z3.And(c >= 1, c <= 255)

def pyval(op, a, c):
    if op == "add": return a + c
    if op == "mul": return a * c
    if op == "and": return a & c
    if op == "or": return a | c
    if op == "xor": return a ^ c
    if op == "fdiv": return a // c
    if op == "mod": return a % c
    if op == "shr": return a >> c

def structures(depth):
    for ops in itertools.product(OPS, repeat=depth):
        if any(ops[i] == ops[i + 1] and ops[i] in ("add", "mul", "and", "or", "xor") for i in range(depth - 1)): continue
        yield ops

def solve_chain(xs, ys, ops, square=False, timeout_ms=2000):
    s = z3.Solver(); s.set("timeout", timeout_ms)
    cs = [z3.BitVec(f"c{i}", 32) for i in range(len(ops))]
    for op, c in zip(ops, cs): s.add(cons(op, c))
    for x, y in zip(xs, ys):
        v = z3.BitVecVal(x, 32)
        for op, c in zip(ops, cs): v = apply(op, v, c)
        if square: v = v * v
        s.add(v == y)
    if s.check() != z3.sat: return None
    m = s.model(); return [m[c].as_signed_long() for c in cs]

def run_chain(x, ops, consts, square=False):
    v = x
    for op, c in zip(ops, consts): v = pyval(op, v, c)
    return v * v if square else v

def search(xs, ys, qs, max_depth=3, budget_s=60):
    """Cheapest template that fits: depth 1, then 2, then 3 (with and without an outer square)."""
    t0 = time.time()
    for depth in range(1, max_depth + 1):
        for square in (False, True):
            for ops in structures(depth):
                if time.time() - t0 > budget_s: return None
                c = solve_chain(xs, ys, ops, square)
                if c is not None and all(run_chain(x, ops, c, square) == y for x, y in zip(xs, ys)):
                    return ops, c, square, [run_chain(q, ops, c, square) for q in qs]
    return None

# ---- two branches combined: y = u(x) OP v(x), each branch x or one op with a constant
COMB = ["add", "sub", "mul", "and", "or", "xor"]
def comb(op, a, b):
    return {"add": lambda: a + b, "sub": lambda: a - b, "mul": lambda: a * b,
            "and": lambda: a & b, "or": lambda: a | b, "xor": lambda: a ^ b}[op]()

def solve_pair(xs, ys, o1, o2, cop, timeout_ms=2000):
    s = z3.Solver(); s.set("timeout", timeout_ms)
    c1, c2 = z3.BitVec("c1", 32), z3.BitVec("c2", 32)
    if o1: s.add(cons(o1, c1))
    if o2: s.add(cons(o2, c2))
    for x, y in zip(xs, ys):
        v = z3.BitVecVal(x, 32)
        u = apply(o1, v, c1) if o1 else v
        w = apply(o2, v, c2) if o2 else v
        s.add(comb(cop, u, w) == y)
    if s.check() != z3.sat: return None
    m = s.model()
    return [m[c1].as_signed_long() if o1 else 0, m[c2].as_signed_long() if o2 else 0]

def run_pair(x, o1, o2, cop, c):
    u = pyval(o1, x, c[0]) if o1 else x
    w = pyval(o2, x, c[1]) if o2 else x
    return {"add": u + w, "sub": u - w, "mul": u * w, "and": u & w, "or": u | w, "xor": u ^ w}[cop]

def search_pairs(xs, ys, qs, budget_s=60):
    t0 = time.time()
    branches = [None] + OPS
    for cop in COMB:
        for i, o1 in enumerate(branches):
            for o2 in branches[(i if cop != "sub" else 0):]:
                if o1 is None and o2 is None: continue
                if time.time() - t0 > budget_s: return None
                c = solve_pair(xs, ys, o1, o2, cop)
                if c is not None and all(run_pair(x, o1, o2, cop, c) == y for x, y in zip(xs, ys)):
                    return (o1, o2, cop), c, [run_pair(q, o1, o2, cop, c) for q in qs]
    return None
