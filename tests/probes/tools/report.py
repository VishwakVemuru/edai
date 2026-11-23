"""Prints each finding's numbers from the runs in out/."""
import sys, collections, math
D = sys.argv[1]
def rd(n): return [l.split() for l in open(f"{D}/{n}")]
def meta(n): return [m.split() for m in open(f"{D}/{n}").read().split("\n")[:-1]]
# F1: exact answers that contradict their own examples
M, N, W = meta("self.meta"), rd("self.native"), rd("self.wasm")
bad = [(m, n) for m, n in zip(M, N) if int(n[1]) in (1, 2) and (int(n[2]), int(n[3])) != (int(m[2]), int(m[3]))]
same = sum(n[1:4] == w[1:4] for n, w in zip(N, W))
print(f"== F1. Exact answers that contradict the problem's own examples (questions repeat an example's x)")
print(f"   {len(bad)} of {len(M)} problems; by family: {dict(collections.Counter(m[0] for m, _ in bad))}")
print(f"   native and WebAssembly agree on {same} of {len(M)}")
# F2 and F4: contradictory examples, low-variety noise
M, N = meta("cases.meta"), rd("cases.native")
B = rd("cases.beat") if __import__("os").path.exists(f"{D}/cases.beat") else None
cells = collections.OrderedDict()
for i, (m, n) in enumerate(zip(M, N)):
    k = " ".join(m); c = cells.setdefault(k, collections.Counter()); c["n"] += 1; c["e"] += int(n[1]) in (1, 2)
    if B: c["b"] += int(B[i][1]) in (1, 2)
print(f"\n== F2. Exact rules claimed for contradictory examples (one x with two different y; no rule can fit)")
for k, c in cells.items():
    if k.startswith("contra"): print(f"   {k:16s} edai exact claims {c['e']:4d}/{c['n']}" + (f"   beat {c['b']}/{c['n']}" if B else ""))
print(f"\n== F4. Exact rules claimed on pure noise with few distinct outputs")
for k, c in cells.items():
    if k.startswith("noise"): print(f"   {k:18s} edai {c['e']:4d}/{c['n']}" + (f"   beat {c['b']}/{c['n']}" if B else ""))
# F3: simple rules
M, N = meta("rules.meta"), rd("rules.native")
B = rd("rules.beat") if __import__("os").path.exists(f"{D}/rules.beat") else None
T = collections.OrderedDict()
for i, (m, n) in enumerate(zip(M, N)):
    a = (int(m[2]), int(m[3])); c = T.setdefault(m[0], collections.Counter()); c["n"] += 1
    right = (int(n[2]), int(n[3])) == a and int(n[1]) != 0
    c["e"] += right; c["ew"] += int(n[1]) in (1, 2) and not right
    if B: c["b"] += (int(B[i][2]), int(B[i][3])) == a and int(B[i][1]) != 0
print(f"\n== F3. Simple integer rules (fresh random constants; questions outside the examples)")
tot = collections.Counter()
for f, c in T.items():
    print(f"   {f:13s} edai right {c['e']:3d}/{c['n']}  claimed exact but wrong {c['ew']:3d}" + (f"   beat right {c['b']:3d}/{c['n']}" if B else ""))
    for k, v in c.items(): tot[k] += v
print(f"   {'ALL':13s} edai right {tot['e']}/{tot['n']}  claimed exact but wrong {tot['ew']}" + (f"   beat right {tot['b']}/{tot['n']}" if B else ""))

# probes aimed at version 1.1's rule (skip one to three smallest-x examples)
import os
if os.path.exists(f"{D}/v11.native"):
    M = [m.split() for m in open(f"{D}/v11.meta").read().split("\n")[:-1]]; N = rd("v11.native")
    imp = contra = exact = 0
    for m, n in zip(M, N):
        by = int(n[1]); vals = [int(n[2]), int(n[3])]
        if by in (1, 2):
            exact += 1
            if m[1] == "IMPOSSIBLE": imp += 1
            elif any(mk != "-" and int(mk) != v for mk, v in zip(m[2:], vals)): contra += 1
    print(f"\n== Probes aimed at 1.1's rule: {len(M)} problems, {exact} exact answers, {imp} on impossible data, {contra} contradicting an example")
