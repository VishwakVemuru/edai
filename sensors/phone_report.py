"""What the brain did on the phone, from the page's saved runs.   python3 phone_report.py results/iphone-run-*.json
A saved file holds every run since the page was opened ("Run again" adds to it), so runs are split where the time restarts."""
import json, sys, statistics as st

def runs(d):
    R, A = d["readings"], d["answers"]; cut_r = [0] + [i for i in range(1, len(R)) if R[i]["t"] < R[i - 1]["t"]] + [len(R)]
    cut_a = [0] + [i for i in range(1, len(A)) if A[i]["t"] < A[i - 1]["t"]] + [len(A)]
    return [(R[cut_r[k]:cut_r[k + 1]], A[cut_a[k]:cut_a[k + 1]]) for k in range(len(cut_r) - 1)]

seen = set()
for path in sys.argv[1:]:
    d = json.load(open(path)); steps = [s["name"] for s in d["steps"]]
    for n, (R, A) in enumerate(runs(d)):
        key = (len(R), R[0]["v"] if R else None, R[-1]["t"] if R else None)
        if key in seen:
            continue
        seen.add(key)
        print(f"{path.split('/')[-1]} run {n + 1}: {len(R)} readings over {R[-1]['t'] / 1000:.0f} s, brain asked {len(A)} times")
        print(f"  {'step':12} {'tilt range':>14} {'asked':>6} {'answered':>9} {'typical miss':>13} {'90% miss under':>15} {'brain ms':>9}")
        for i, s in enumerate(steps):
            r = [x["v"] for x in R if x["step"] == i]; a = [x for x in A if x["step"] == i]
            got = sorted(x["miss"] for x in a if x["by"] != "CANNOT")
            ms = st.median([x["ms"] for x in a]) if a else 0
            rng = f"{min(r):6.1f}..{max(r):5.1f}" if r else ""
            p90 = got[int(0.9 * (len(got) - 1))] if got else None
            print(f"  {s:12} {rng:>14} {len(a):6d} {100 * len(got) / len(a) if a else 0:8.0f}% "
                  f"{(f'{st.median(got):.1f} deg') if got else '':>13} {(f'{p90:.1f} deg') if got else '':>15} {ms:9.1f}")
        by = {}
        for x in A: by[x["by"]] = by.get(x["by"], 0) + 1
        print("  who answered:", by)
