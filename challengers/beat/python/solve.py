"""A solver for the edai 300-problem challenge. Uses only each problem's own examples.
   python3 solve.py problems.json > answers.txt"""
import json, sys, math, os
from fractions import Fraction as Fr
import numpy as np

# ---------------------------------------------------------------- feature library
def digsum(v): return np.array([sum(map(int, str(abs(int(t))))) for t in v], dtype=np.int64)
def digrev(v): return np.array([int(str(abs(int(t)))[::-1]) * (1 if t >= 0 else -1) for t in v], dtype=np.int64)
def popc(v): return np.array([bin(int(t) & 0xFFFFFFFF).count("1") for t in v], dtype=np.int64)
def ndig(v): return np.array([len(str(abs(int(t)))) for t in v], dtype=np.int64)

def cmod(a, k): return np.fmod(a, k)
def cdiv(a, k): return (np.sign(a) * (np.abs(a) // k)).astype(np.int64)

def unary_ops():
    """(name, cost, fn) — fn maps an int64 vector to an int64 vector."""
    ops = [("abs", 1, np.abs), ("sq", 1, lambda v: v * v), ("cube", 2, lambda v: v * v * v),
           ("digsum", 2, digsum), ("rev", 2, digrev), ("popc", 2, popc), ("ndig", 2, ndig),
           ("sign", 1, np.sign), ("neg", 1, lambda v: -v)]
    for k in list(range(2, 13)) + [16, 20, 25, 32, 50, 64, 100]:
        ops.append((f"%{k}", 1, lambda v, k=k: np.mod(v, k)))
        ops.append((f"c%{k}", 2, lambda v, k=k: cmod(v, k)))
        ops.append((f"//{k}", 1, lambda v, k=k: np.floor_divide(v, k)))
        ops.append((f"c/{k}", 2, lambda v, k=k: cdiv(v, k)))
    for k in list(range(1, 16)) + [31, 63, 127, 255]:
        ops.append((f"&{k}", 2, lambda v, k=k: v & k))
        ops.append((f"|{k}", 2, lambda v, k=k: v | k))
        ops.append((f"^{k}", 2, lambda v, k=k: v ^ k))
    for k in range(1, 5):
        ops.append((f">>{k}", 2, lambda v, k=k: v >> k))
        ops.append((f"<<{k}", 2, lambda v, k=k: v << k))
    return ops

UOPS = unary_ops()
BOPS = [("+", lambda a, b: a + b), ("-", lambda a, b: a - b), ("*", lambda a, b: a * b),
        ("min", np.minimum), ("max", np.maximum), ("&", lambda a, b: a & b),
        ("|", lambda a, b: a | b), ("^", lambda a, b: a ^ b)]

def build_features(X, depth2=True):
    """Bottom-up enumeration with observational-equivalence dedupe over all inputs X.
    Returns list of (cost, name, vector)."""
    seen = {}
    out = []
    def add(cost, name, v):
        if np.any(np.abs(v) > 10**12): return
        key = v.tobytes()
        if key in seen:
            j = seen[key]
            if out[j][0] > cost: out[j] = (cost, name, v)
            return
        seen[key] = len(out)
        out.append((cost, name, v))
    x = np.asarray(X, dtype=np.int64)
    add(0, "x", x)
    lvl1 = []
    for n, c, f in UOPS:
        v = f(x); before = len(out); add(c, f"{n}(x)", v)
        if len(out) > before: lvl1.append(out[-1])
    lvl1 = [t for t in out[1:] if t[0] <= 2]
    for k in range(-20, 21):
        add(2, f"max(x,{k})", np.maximum(x, k)); add(2, f"min(x,{k})", np.minimum(x, k))
    inner_ops = [t for t in UOPS if t[0][0] in "%/c&|^>" or t[0] in ("sq", "abs")]
    for a in list(range(2, 13)) + [-1, -2, -3]:
        for b in range(-12, 13):
            lin = a * x + b
            if b: add(2, f"({a}x+{b})", lin)
            for n, c, f in inner_ops:
                add(c + 2 + (b != 0), f"{n}({a}x+{b})", f(lin))
    if depth2:
        for c1, n1, v1 in list(lvl1):
            for n, c, f in UOPS:
                add(c1 + c + 1, f"{n}({n1})", f(v1))
        base = [(0, "x", x)] + [t for t in lvl1 if t[0] <= 2]
        for i, (c1, n1, v1) in enumerate(base):
            for (c2, n2, v2) in base[i:]:
                for n, f in BOPS:
                    add(c1 + c2 + 2, f"({n1}{n}{n2})", f(v1, v2))
    out.sort(key=lambda t: t[0])
    return out

# ---------------------------------------------------------------- exact fitting
def fit_affine(Fm, y, nq):
    """For each row F of Fm (features over examples + questions), test y == a*F + b exactly on examples.
    Returns list of (row index, a, b) that fit."""
    E = Fm[:, :-nq] if nq else Fm
    yy = np.asarray(y, dtype=np.float64)
    Ef = E.astype(np.float64)
    mF = Ef.mean(1, keepdims=True); my = yy.mean()
    dF = Ef - mF
    var = (dF * dF).sum(1)
    cov = (dF * (yy - my)).sum(1)
    with np.errstate(divide="ignore", invalid="ignore"):
        a = np.where(var > 0, cov / np.where(var > 0, var, 1), 0.0)
    b = my - a * mF[:, 0]
    pred = a[:, None] * Ef + b[:, None]
    ok = np.all(np.abs(pred - yy) < 1e-6, axis=1) & (var > 0)
    return [(i, a[i], b[i]) for i in np.nonzero(ok)[0]]

def nice(v):
    f = Fr(v).limit_denominator(12)
    return f if abs(float(f) - v) < 1e-7 else None

def exact_preds(a, b, Fq):
    a, b = nice(a), nice(b)
    if a is None or b is None: return None
    out = []
    for q in Fq:
        r = a * int(q) + b
        if r.denominator != 1: return None
        out.append(int(r))
    return out

def coef_cost(a, b):
    a, b = nice(a), nice(b)
    c = 0
    if a != 1: c += 1 + (a.denominator != 1) * 2 + (abs(a) > 10)
    if b != 0: c += 1 + (b.denominator != 1) * 2
    return c

def best_affine(feats, xs, ys, qs, max_cost=99, min_checks=1):
    """Simplest feature F with y = a F + b exactly on examples (needs >= 2 + min_checks examples)."""
    n = len(xs)
    if n < 2 + min_checks: return None
    # constant
    if len(set(ys)) == 1 and n >= 1 + min_checks:
        return (0, "const", [ys[0]] * len(qs))
    sel = [t for t in feats if t[0] <= max_cost]
    if not sel: return None
    Fm = np.stack([t[2] for t in sel])
    best = None
    for i, a, b in fit_affine(Fm, ys, len(qs)):
        # a feature with few distinct values on the examples carries little evidence
        if len(set(Fm[i, :n].tolist())) < 2: continue
        p = exact_preds(a, b, Fm[i, n:])
        if p is None: continue
        cost = sel[i][0] + coef_cost(a, b)
        if best is None or cost < best[0]:
            best = (cost, f"{nice(a)}*{sel[i][1]}+{nice(b)}", p)
    return best

# ---------------------------------------------------------------- rules x -> y
def conditions(xs):
    conds = []
    for m in range(2, 11):
        for r in range(m):
            conds.append((1, f"x%{m}=={r}", lambda v, m=m, r=r: np.mod(v, m) == r))
    sx = sorted(set(xs))
    th = []
    for lo, hi in zip(sx, sx[1:]):
        t = -0.5 if lo < 0 <= hi else (lo + hi) / 2   # a question in the gap goes to the nearer side
        conds.append((1, f"x<={t:g}", lambda v, t=t: v <= t))
    return conds

def budget(checks):
    """Largest feature cost a leaf may use, given how many examples it has beyond its fitted coefficients."""
    return {1: 2, 2: 4}.get(checks, -1 if checks < 1 else 99)

def numbits(f):
    f = Fr(f)
    return 1 + 2 * math.log2(abs(f.numerator) + 1) + 2 * math.log2(f.denominator)

def coefbits(a, b):
    a, b = nice(a), nice(b)
    return (0 if a == 1 else numbits(a)) + (0 if b == 0 else numbits(b))

class RuleCtx:
    def __init__(self, xs, ys, qs):
        self.xs, self.ys, self.qs = xs, ys, list(qs)
        self.n = len(xs)
        X = np.array(xs + list(qs), dtype=np.int64)
        self.X = X
        self.feats = build_features(X)
        self.cost = np.array([t[0] for t in self.feats])
        self.names = [t[1] for t in self.feats]
        self.Fi = np.stack([t[2] for t in self.feats])
        self.Ff = self.Fi.astype(np.float64)
        self.cache = {}
        cnt = np.cumsum(np.bincount(self.cost))
        self.fbits = lambda c: math.log2(cnt[min(c, len(cnt) - 1)])
        if os.environ.get("FIXED_FBITS"):
            self.fbits = lambda c: math.log2(FBITS_TABLE[min(c, 6)])

    def leaf(self, eidx, qidx, maxcost=99):
        """Best affine leaf y = a F + b over examples eidx; predictions for question indices qidx.
        Returns (cost, name, preds) or None."""
        key = (tuple(eidx), tuple(qidx), maxcost)
        if key in self.cache: return self.cache[key]
        ys = [self.ys[i] for i in eidx]; k = len(eidx)
        best = None
        if len(set(ys)) == 1 and k >= 2:
            best = (1 + numbits(ys[0]), f"{ys[0]}", [ys[0]] * len(qidx))
        lim = min(maxcost, budget(k - 0))
        if lim >= 0 and k >= 2:
            rows = np.nonzero(self.cost <= lim)[0]
            E = self.Ff[np.ix_(rows, eidx)]
            yy = np.asarray(ys, float)
            mF = E.mean(1); my = yy.mean(); dF = E - mF[:, None]
            var = (dF * dF).sum(1); cov = (dF * (yy - my)).sum(1)
            av = np.where(var > 0, cov / np.where(var > 0, var, 1), 0.0)
            bv = my - av * mF
            ok = (var > 0) & np.all(np.abs(av[:, None] * E + bv[:, None] - yy) < 1e-6, axis=1)
            npar = (np.abs(av - 1) > 1e-9).astype(int) + (np.abs(bv) > 1e-9)
            bud = np.vectorize(budget)(k - npar) if len(npar) else npar
            ok &= self.cost[rows] <= bud
            ok &= np.abs(av * 27720 - np.round(av * 27720)) < 1e-5
            ok &= np.abs(bv * 27720 - np.round(bv * 27720)) < 1e-5
            cand = np.nonzero(ok)[0]
            cand = cand[np.argsort(self.cost[rows[cand]], kind="stable")]
            for j in cand[:400]:
                fc = self.cost[rows[j]]
                if best is not None and self.fbits(fc) + 1 >= best[0]: break
                a, b = nice(av[j]), nice(bv[j])
                if a is None or b is None: continue
                c = self.fbits(fc) + coefbits(av[j], bv[j]) + 1
                if best is not None and c >= best[0]: continue
                qv = [self.Fi[rows[j], self.n + q] for q in qidx]
                p = exact_preds(av[j], bv[j], qv)
                if p is None: continue
                best = (c, f"{a}*{self.names[rows[j]]}+{b}", p)
        self.cache[key] = best
        return best

    def pair_fits(self, maxcost=3):
        """y = a F + b G + c for two cheap features (covers exact quadratics, x + x%k, ...)."""
        rows = np.nonzero(self.cost <= maxcost)[0][:400]
        n = self.n
        if n < 6: return None
        E = self.Ff[rows][:, :n]
        yy = np.asarray(self.ys, float)
        best = None
        ones = np.ones(n)
        for ii in range(len(rows)):
            Fa = E[ii]
            A = np.stack([np.broadcast_to(Fa, E.shape), E, np.broadcast_to(ones, E.shape)], axis=2)  # (m, n, 3)
            sol = np.einsum("min,n->mi", np.linalg.pinv(A), yy)
            good = np.linalg.matrix_rank(A) == 3
            res = np.abs(np.einsum("mni,mi->mn", A, sol) - yy).max(1)
            for jj in np.nonzero(good & (res < 1e-6))[0]:
                if jj <= ii: continue
                cs = [nice(v) for v in sol[jj]]
                if any(c is None for c in cs) or cs[0] == 0 or cs[1] == 0: continue
                c = self.fbits(self.cost[rows[ii]]) + self.fbits(self.cost[rows[jj]]) + 2 + sum(numbits(v) for v in cs if v not in (0, 1))
                if best is not None and c >= best[0]: continue
                pr = []
                for q in range(len(self.qs)):
                    v = cs[0] * int(self.Fi[rows[ii], n + q]) + cs[1] * int(self.Fi[rows[jj], n + q]) + cs[2]
                    if v.denominator != 1: break
                    pr.append(int(v))
                else:
                    best = (c, f"{cs[0]}*{self.names[rows[ii]]}+{cs[1]}*{self.names[rows[jj]]}+{cs[2]}", pr)
        return best

def solve_rule(ex, qs):
    xs = [e[0] for e in ex]; ys = [e[1] for e in ex]
    ctx = RuleCtx(xs, ys, qs)
    n, nq = len(xs), len(qs)
    allE, allQ = list(range(n)), list(range(nq))
    cands = []
    g = ctx.leaf(allE, allQ)
    if g: cands.append(g)
    if g and g[0] <= 8: return g
    pf = ctx.pair_fits() if os.environ.get("PAIRS", "1") == "1" else None
    if pf: cands.append(pf)
    conds = conditions(xs)
    masks = [(cc, cn, cf(ctx.X)) for cc, cn, cf in conds]

    def split(eidx, qidx, depth, maxcost, out=None):
        """Best tree over the given examples: a leaf, or cond -> (leaf | subtree)."""
        best = ctx.leaf(eidx, qidx, maxcost)
        if depth == 0: return best
        seen = set()
        for cc, cn, m in masks:
            L = [i for i in eidx if m[i]]; R = [i for i in eidx if not m[i]]
            if len(L) < 2 or len(R) < 2: continue
            key = tuple(L)
            if key in seen: continue
            seen.add(key)
            LQ = [q for q in qidx if m[n + q]]; RQ = [q for q in qidx if not m[n + q]]
            l = split(L, LQ, depth - 1, maxcost)
            if l is None: continue
            r = split(R, RQ, depth - 1, maxcost)
            if r is None: continue
            c = l[0] + r[0] + math.log2(len(masks)) + 1
            if out is None and best is not None and c >= best[0]: continue
            ans = {}
            for q, v in zip(LQ, l[2]): ans[q] = v
            for q, v in zip(RQ, r[2]): ans[q] = v
            cand = (c, f"[{cn} ? {l[1]} : {r[1]}]", [ans[q] for q in qidx])
            if out is not None: out.append(cand)
            if best is None or c < best[0]: best = cand
        return best

    split(allE, allQ, 1, 99, cands)
    if n >= 8:
        split(allE, allQ, 2, 4, cands)
    # accept only a model that is much shorter than the examples it explains
    ybits = math.log2(max(max(ys) - min(ys) + 2, YFLOOR))
    if DEBUG:
        for c in sorted(cands)[:4]: print(f"   bits={c[0]:.1f} limit={(n - MARGIN) * ybits:.1f} {c[1]} -> {c[2]}", file=sys.stderr)
    cands = [c for c in cands if c[0] <= (n - MARGIN) * ybits]
    if not cands: return None
    cands.sort(key=lambda t: t[0])
    if not VOTE: return cands[0]
    # Bayesian vote: every accepted model backs its answers with weight 2^-bits
    w = {}
    for c in cands:
        k = tuple(c[2]); w[k] = w.get(k, 0) + 2.0 ** (cands[0][0] - c[0])
    top = max(w, key=w.get)
    return next(c for c in cands if tuple(c[2]) == top)

MARGIN = 2
VOTE = int(os.environ.get('VOTE', '1'))
YFLOOR = 32
FBITS_TABLE = [1, 41, 554, 1842, 17512, 61017, 85089]   # median distinct formulas with cost <= c
import os
DEBUG = os.environ.get('DEBUG')

# ---------------------------------------------------------------- sequences
def solve_linear_exact(rows, rhs):
    """Solve rows * c = rhs exactly over rationals with least-squares-free elimination; None if inconsistent."""
    m = [list(map(Fr, r)) + [Fr(v)] for r, v in zip(rows, rhs)]
    ncol = len(rows[0]); piv = []
    r = 0
    for c in range(ncol):
        p = next((i for i in range(r, len(m)) if m[i][c] != 0), None)
        if p is None: continue
        m[r], m[p] = m[p], m[r]
        inv = m[r][c]
        m[r] = [v / inv for v in m[r]]
        for i in range(len(m)):
            if i != r and m[i][c] != 0:
                f = m[i][c]; m[i] = [a - f * b for a, b in zip(m[i], m[r])]
        piv.append(c); r += 1
    for i in range(r, len(m)):
        if m[i][-1] != 0: return None
    if r < ncol: return None  # under-determined
    sol = [Fr(0)] * ncol
    for i, c in enumerate(piv): sol[c] = m[i][-1]
    return sol

def seq_models(t):
    """Yield (cost, name, next2) for exact models of the sequence t."""
    n = len(t)
    # linear recurrence: t_k = sum a_i t_{k-i} + c0 + c1 k + c2 k^2
    for order in range(0, 4):
        for npoly in range(0, 4):
            npar = order + npoly
            if npar == 0: continue
            eqs = n - order
            if eqs - npar < 2: continue
            rows, rhs = [], []
            for k in range(order, n):
                rows.append([t[k - i] for i in range(1, order + 1)] + [k ** d for d in range(npoly)])
                rhs.append(t[k])
            sol = solve_linear_exact(rows, rhs)
            if sol is None: continue
            seq = list(t)
            for k in range(n, n + 2):
                v = sum(sol[i - 1] * seq[k - i] for i in range(1, order + 1)) + sum(sol[order + d] * k ** d for d in range(npoly))
                seq.append(v)
            if any(Fr(v).denominator != 1 for v in seq[n:]): continue
            cost = 2 * npar + sum(1 for s in sol if s.denominator != 1) * 2
            yield (cost, f"rec{order}+poly{npoly} {sol}", [int(v) for v in seq[n:]])
    # alternating two operations: t1=f(t0), t2=g(t1), ... f,g affine (a*t+b)
    for per in (2, 3):
        sols = []
        okall = True
        for ph in range(per):
            pairs = [(t[k - 1], t[k]) for k in range(1, n) if k % per == ph]
            if len(pairs) < 2: okall = False; break
            found = None
            for a in [Fr(v) for v in range(-5, 6)] + [Fr(1, 2), Fr(-1, 2), Fr(1, 3)]:
                b = pairs[0][1] - a * pairs[0][0]
                if all(a * u + b == w for u, w in pairs):
                    found = (a, b); break
            if found is None: okall = False; break
            sols.append(found)
        if okall:
            seq = list(t)
            for k in range(n, n + 2):
                a, b = sols[k % per]; seq.append(a * seq[-1] + b)
            if all(Fr(v).denominator == 1 for v in seq[n:]):
                yield (3 * per, f"alt{per} {sols}", [int(v) for v in seq[n:]])

def solve_seq(t):
    best = None
    for m in seq_models(t):
        if best is None or m[0] < best[0]: best = m
    return best

# ---------------------------------------------------------------- trends vs noise
def _poly(d):
    def f(x, y, q):
        s = x.std() or 1; mu = x.mean()
        return np.polyval(np.polyfit((x - mu) / s, y, d), (q - mu) / s)
    return f

def _gp(ls, noise):
    """Gaussian-process smoother around a straight line."""
    def f(x, y, q):
        k = lambda a, b: np.exp(-0.5 * ((a[:, None] - b[None, :]) / ls) ** 2)
        c = np.polyfit(x, y, 1); r = y - np.polyval(c, x); sc = r.std() or 1
        w = np.linalg.solve(k(x, x) + noise * np.eye(len(x)), r / sc)
        return np.polyval(c, q) + sc * (k(q, x) @ w)
    return f

MODELS = [_poly(1), _poly(2), _poly(3), _poly(4)] + [_gp(l, nz) for l in (15, 25, 40, 60, 100) for nz in (0.05, 0.2, 0.5, 1.0)]

def _loo(x, y, m):
    n = len(x); e = []
    for i in range(n):
        k = np.ones(n, bool); k[i] = False
        e.append(m(x[k], y[k], x[i:i + 1])[0] - y[i])
    return float(np.sqrt(np.mean(np.square(e))))

def trend(xs, ys, qs, top=10):
    """Returns (score, answers): score = best leave-one-out error over the spread of y (about 1 for noise)."""
    x = np.asarray(xs, float); y = np.asarray(ys, float); q = np.asarray(qs, float)
    sc = sorted((_loo(x, y, m), i) for i, m in enumerate(MODELS))
    use = sc[:top]
    w = np.array([1 / (s * s + 1e-9) for s, _ in use])
    pred = (w[:, None] * np.array([MODELS[i](x, y, q) for _, i in use])).sum(0) / w.sum()
    return sc[0][0] / (y.std() or 1.0), [float(v) for v in pred], "ens"

# ---------------------------------------------------------------- driver
def solve(p):
    if p["type"] == "sequence":
        t = p["terms"]
        m = solve_seq(t)
        if m: return m[2], f"seq {m[1]}"
        return "CANNOT", "seq none"
    ex, qs = p["examples"], p["questions"]
    r = solve_rule(ex, qs)
    if r: return r[2], f"rule {r[1]}"
    xs = [e[0] for e in ex]; ys = [e[1] for e in ex]
    score, ans, deg = trend(xs, ys, qs)
    if score < TREND_CUT:
        return [round(a, 2) for a in ans], f"trend deg{deg} s={score:.2f}"
    return "CANNOT", f"noise s={score:.2f}"

TREND_CUT = 0.6

if __name__ == "__main__":
    probs = json.load(open(sys.argv[1]))
    only = set(sys.argv[2:])
    for p in probs:
        if only and p["id"] not in only: continue
        ans, why = solve(p)
        a = "CANNOT" if ans == "CANNOT" else ",".join(str(v) for v in ans)
        print(f"{p['id']} {a}  # {why}", flush=True)
