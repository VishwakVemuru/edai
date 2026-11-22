"""Checks the text from the simulator (or a board) against the reference answers, problem by problem, even when only
some problems ran.   python3 check_log.py wokwi-output.txt [reference]   (the reference defaults to expected.txt here)"""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
norm = lambda l: " ".join(l.split())                       # a pasted log may lose repeated spaces
REF = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "expected.txt")
ref = {l.split()[0]: norm(l) for l in open(REF).read().splitlines() if re.match(r"^p\d{3} ", l)}
got = {}
for line in open(sys.argv[1], errors="replace").read().splitlines():
    m = re.match(r"^\s*(p\d{3}) ", line)
    if m: got[m.group(1)] = norm(re.sub(r" ms=\S+ cycles=\S+\s*$", "", line.strip()))
same = [p for p in got if got[p] == ref.get(p)]
print(f"problems the chip answered: {len(got)}")
print(f"the same line as the reference (who answered, both answers, self-check, right or wrong): {len(same)} of {len(got)}")
print(f"right among them: {sum(1 for p in got if got[p].endswith('right=1'))} of {len(got)}")
for p in sorted(set(got) - set(same)):
    print("  differs:", p, "| reference:", ref.get(p), "| chip:", got[p])
for line in open(sys.argv[1], errors="replace").read().splitlines():
    if line.strip().split(" ")[0] in ("EDAI", "BOARD", "LIBRARY", "SUMMARY", "WHO", "TIME", "STACK"): print(line.strip())
