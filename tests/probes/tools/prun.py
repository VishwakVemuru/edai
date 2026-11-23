"""Runs a solver over a lines file in parallel chunks, keeping line order.
   python3 prun.py native|wasm in.lines out.txt [-j N]"""
import subprocess, sys, os, tempfile
which, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
j = int(sys.argv[sys.argv.index("-j") + 1]) if "-j" in sys.argv else 8
here = os.path.dirname(os.path.abspath(__file__))
cmd = {"native": [os.path.join(here, "native_solve")], "wasm": ["node", os.path.join(here, "wasm_solve.mjs")], "beat": [os.path.join(here, "beat_solve")]}[which]
lines = open(src).read().splitlines()
k = max(1, (len(lines) + j - 1) // j)
chunks = [lines[i:i + k] for i in range(0, len(lines), k)]
procs = []
for c in chunks:
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    procs.append(p)
outs = []
import threading
res = [None] * len(procs)
def feed(i, p, c): res[i] = p.communicate("\n".join(c) + "\n")[0]
ts = [threading.Thread(target=feed, args=(i, p, c)) for i, (p, c) in enumerate(zip(procs, chunks))]
[t.start() for t in ts]; [t.join() for t in ts]
out = "".join(res)
assert out.count("\n") == len(lines), f"line count mismatch {out.count(chr(10))} vs {len(lines)}"
open(dst, "w").write(out)
