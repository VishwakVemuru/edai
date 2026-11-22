"""Scores answers to the 300 problems.   python3 score.py my-answers.txt

my-answers.txt has one line per problem: its id, then the two answers or CANNOT, for example
    p000 210,205
    p001 CANNOT
(the runners' own lines, "p000 trend by=NET ans=210,205 ...", are read as well).  A rule needs both answers exact,
a trend both answers within its tolerance, noise needs CANNOT.  A missing problem counts as wrong."""
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
key = {a["id"]: a for a in json.load(open(os.path.join(HERE, "problems", "answers.json")))}
given = {}
for line in open(sys.argv[1]):
    m = re.match(r"\s*(p\d{3})\b(.*)", line)
    if not m: continue
    rest = m.group(2); a = re.search(r"ans=(\S+)", rest); tok = a.group(1) if a else (rest.split() or [""])[0]
    given[m.group(1)] = "CANNOT" if tok.upper() == "CANNOT" else [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", tok)][:2]
right = {"rule": 0, "trend": 0, "noise": 0}; count = {"rule": 0, "trend": 0, "noise": 0}
for pid, k in key.items():
    f = k["family"]; count[f] += 1; g = given.get(pid)
    if f == "noise": ok = g == "CANNOT"
    elif not isinstance(g, list) or len(g) != 2: ok = False
    elif f == "rule": ok = g == [float(v) for v in k["answer"]]
    else: ok = all(abs(x - t) <= k["tolerance"] for x, t in zip(g, k["answer"]))
    right[f] += ok
print(f"right {sum(right.values())} of {len(key)}: rules {right['rule']}/{count['rule']}, trends {right['trend']}/{count['trend']}, noise {right['noise']}/{count['noise']}")
