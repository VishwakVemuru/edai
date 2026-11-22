"""edai's published brain on real lab recordings.

    python3 -I lab.py ./sensor_run data/intel/data.txt.gz data/cwru-normal/97.mat data/cwru-fault/105.mat results/lab.json

Every question is the same kind as edai's trend problems: 14 readings in a row, 12 given as examples (x = when, y = the
reading as a whole number), the 4th and 9th held back and asked.  The brain answers both (an exact rule, or an estimate
that passed its self-check) or says CANNOT.  "Miss" is the larger of its two errors against the real held-back readings.

  1  drift      Intel Berkeley Lab temperature sensors (2004).  When a sensor's battery runs down its reading creeps
                up and finally sticks at 122.15 C.  Windows of readings in a row (about 31 s apart), by phase:
                healthy (all below 35 C), drifting (35 to 122 C), stuck (122.15 C).
  1b drift      the same sensors against a healthy neighbour: the brain learns "neighbour -> this sensor" from 12
     check      hourly pairs on a healthy day, then says what this sensor should read every hour after; the gap
                between that and what it does read is the drift.
  2  motor      Case Western bearing rig, drive-end vibration at 12 kHz, motor at 0 hp: 10 s running normally (97.mat)
                then 10 s with an inner-race fault (105.mat), joined at a known point.  The reading is the vibration
                level (RMS) of each 0.05 s; windows before, across and after the change.
  3  no         healthy sensor readings 20 minutes apart (real daily swings), asked as they are, and again with the
     pattern    same values scrambled in time so that no pattern is left."""
import collections
import concurrent.futures as cf
import datetime as dt
import gzip
import json
import random
import subprocess
import sys

import numpy as np
from scipy.io import loadmat

RUNNER, INTEL, NORMAL, FAULT, OUT = sys.argv[1:6]
UTC = dt.timezone.utc          # the recording's clock times are read as UTC, so the hours are the same on every computer
W, HOLD = 14, (4, 9)
EX = [i for i in range(W) if i not in HOLD]
rng = random.Random(7)


def ask(windows, workers=10):
    """windows: [(id, xs, ys, qs)] in whole numbers  ->  {id: {"by":, "ans":, "check":, "est":}}"""
    chunks = [windows[i::workers] for i in range(workers)]

    def run(chunk):
        if not chunk:
            return {}
        text = "".join(f"{w} {len(xs)} {' '.join(map(str, xs))} {' '.join(map(str, ys))} {len(qs)} {' '.join(map(str, qs))}\n"
                       for w, xs, ys, qs in chunk)
        out = subprocess.run([RUNNER], input=text, capture_output=True, text=True, check=True).stdout
        res = {}
        for line in out.splitlines():
            p = line.split()
            res[p[0]] = dict(kv.split("=", 1) for kv in p[1:])
        return res

    res = {}
    with cf.ThreadPoolExecutor(workers) as ex:
        for r in ex.map(run, chunks):
            res.update(r)
    return res


def frame(ts, vs, scale, xunit):
    """14 readings (times ts, values vs) -> examples, questions and the two held-back true readings"""
    xa = [int(round((t - ts[0]) / xunit)) for t in ts]
    return ([xa[i] for i in EX], [int(round(vs[i] * scale)) for i in EX], [xa[i] for i in HOLD], [vs[i] for i in HOLD])


def miss(res, truth, scale):
    if res.get("by", "CANNOT") == "CANNOT":
        return None
    a = [int(v) / scale for v in res["ans"].split(",")]
    return max(abs(a[0] - truth[0]), abs(a[1] - truth[1]))


def summary(rows):
    """rows: [(by, miss)]"""
    n = len(rows); got = [m for b, m in rows if b != "CANNOT"]
    by = collections.Counter(b for b, _ in rows)
    s = dict(windows=n, answered=len(got), answered_pct=round(100 * len(got) / n, 1) if n else None, by=dict(by))
    if got:
        s.update(median_miss=round(float(np.median(got)), 4), p90_miss=round(float(np.percentile(got, 90)), 4))
    return s


def show(name, s, unit):
    extra = f", miss median {s['median_miss']:g} {unit}, 90% under {s['p90_miss']:g} {unit}" if "median_miss" in s else ""
    print(f"    {name:28s} {s['windows']:5d} windows: answered {s['answered_pct']:5.1f}%  {s['by']}{extra}")


report = {}

# ---------------------------------------------------------------------------------------------------- the Intel lab
print("reading the Intel lab recording...", flush=True)
raw = collections.defaultdict(dict)
with gzip.open(INTEL, "rt", errors="replace") as f:
    for line in f:
        p = line.split()
        if len(p) != 8:
            continue
        try:
            m = int(p[3]); t = dt.datetime.fromisoformat(p[0] + " " + p[1][:15]).replace(tzinfo=UTC).timestamp(); v = float(p[4])
        except ValueError:
            continue
        raw[m][t] = v                                           # one reading per moment (the file repeats some)
series = {m: (np.array(sorted(d)), np.array([d[t] for t in sorted(d)])) for m, d in raw.items()}

# failing sensors with a long life and a real drift phase
fail = []
for m, (ts, vs) in sorted(series.items()):
    drift_n = int(np.sum((vs >= 35) & (vs < 122.1)))
    if len(vs) >= 30000 and np.any(vs >= 122.1) and drift_n >= 300:
        fail.append((m, drift_n))
motes = [m for m, _ in sorted(fail, key=lambda x: -series[x[0]][0].size)[:6]]
print("sensors used for drift:", motes, flush=True)

# 1: windows of readings in a row, by phase
wins, meta = [], {}
for m in motes:
    ts, vs = series[m]
    starts = [s for s in range(0, len(ts) - W) if ts[s + W - 1] - ts[s] <= 14 * 60]     # no gaps over 14 min
    phase = {}
    for s in starts:
        seg = vs[s:s + W]
        phase[s] = "stuck" if seg.min() >= 122.1 else ("healthy" if seg.max() < 35 else ("drifting" if seg.max() < 122.1 else "mixed"))
    for ph, k in (("healthy", 300), ("drifting", 600), ("stuck", 100)):
        cand = [s for s in starts if phase[s] == ph]
        pick = cand if len(cand) <= k else [cand[int(i * len(cand) / k)] for i in range(k)]
        for s in pick:
            xs, ys, qs, truth = frame(ts[s:s + W], vs[s:s + W], 100, 10.0)
            wid = f"d{m}_{s}"; wins.append((wid, xs, ys, qs)); meta[wid] = (m, ph, truth)
print(f"1 drift: asking {len(wins)} windows...", flush=True)
res = ask(wins)
rows = collections.defaultdict(list)
for wid, (m, ph, truth) in meta.items():
    rows[ph].append((res[wid]["by"], miss(res[wid], truth, 100)))
jitter = float(np.median(np.concatenate([np.abs(np.diff(series[m][1][series[m][1] < 35])) for m in motes])))
report["drift"] = dict(sensors=motes, sensor_jitter_C=round(jitter, 4), phases={ph: summary(rows[ph]) for ph in ("healthy", "drifting", "stuck")})
print(f"  1 drift (Intel lab, readings in a row about 31 s apart; a sensor's own reading moves {jitter:.3f} C between neighbouring readings):")
for ph in ("healthy", "drifting", "stuck"):
    show(ph, report["drift"]["phases"][ph], "C")

# 1, slower: the median of each 5 minutes, 14 of them (70 minutes), so that real movement can stand out from the jitter
wins, meta = [], {}
for m in motes:
    ts, vs = series[m]; b5 = collections.defaultdict(list)
    for t, v in zip(ts, vs):
        b5[int(t // 300)].append(v)
    keys = sorted(k for k, v in b5.items() if len(v) >= 3)
    val = {k: float(np.median(b5[k])) for k in keys}
    starts = [i for i in range(len(keys) - W) if keys[i + W - 1] - keys[i] == W - 1]
    phase = {}
    for i in starts:
        seg = [val[keys[i + j]] for j in range(W)]
        phase[i] = "stuck" if min(seg) >= 122.1 else ("healthy" if max(seg) < 35 else ("drifting" if max(seg) < 122.1 else "mixed"))
    for ph, k in (("healthy", 300), ("drifting", 400), ("stuck", 100)):
        cand = [i for i in starts if phase[i] == ph]
        pick = cand if len(cand) <= k else [cand[int(j * len(cand) / k)] for j in range(k)]
        for i in pick:
            tt = [keys[i + j] * 300.0 for j in range(W)]; vv = [val[keys[i + j]] for j in range(W)]
            xs, ys, qs, truth = frame(tt, vv, 100, 30.0)
            wid = f"f{m}_{i}"; wins.append((wid, xs, ys, qs)); meta[wid] = (m, ph, truth)
res = ask(wins)
rows = collections.defaultdict(list)
for wid, (m, ph, truth) in meta.items():
    rows[ph].append((res[wid]["by"], miss(res[wid], truth, 100)))
report["drift_5min"] = {ph: summary(rows[ph]) for ph in ("healthy", "drifting", "stuck")}
print("  1 drift, slower (the median of each 5 minutes, 70 minutes a window):")
for ph in ("healthy", "drifting", "stuck"):
    show(ph, report["drift_5min"][ph], "C")

# 1b: the drift against a healthy neighbour
hourly = {}
for m, (ts, vs) in series.items():
    h = collections.defaultdict(list)
    for t, v in zip(ts, vs):
        h[int(t // 3600)].append(v)
    hourly[m] = {k: float(np.median(v)) for k, v in h.items() if len(v) >= 20}
checks = []
for m in motes:
    H = hourly[m]; hours = sorted(H)
    first_hot = min(k for k in hours if H[k] >= 35); stuck = min(k for k in hours if H[k] >= 122.1)
    # the neighbour: still healthy until this sensor sticks, and the closest match over this sensor's healthy hours
    best = None
    for n, Hn in hourly.items():
        if n == m:
            continue
        common = [k for k in hours if k in Hn and k < first_hot - 24]
        if len(common) < 200 or any(Hn.get(k, 0) >= 35 for k in range(first_hot - 72, stuck + 1)):
            continue
        a = np.array([H[k] for k in common]); b = np.array([Hn[k] for k in common])
        d = float(np.median(np.abs((a - a.mean()) - (b - b.mean()))))
        if best is None or d < best[0]:
            best = (d, n)
    if best is None:
        continue
    n = best[1]; Hn = hourly[n]
    day0 = first_hot - 72                                   # learn on the day that starts 3 days before the drift shows
    learn = [k for k in range(day0, day0 + 24, 2) if k in H and k in Hn]
    xs = [int(round(Hn[k] * 100)) for k in learn]; ys = [int(round(H[k] * 100)) for k in learn]
    if len(set(xs)) < len(xs) or len(xs) < 8:
        continue
    order = sorted(range(len(xs)), key=lambda i: xs[i]); xs = [xs[i] for i in order]; ys = [ys[i] for i in order]
    asks, when = [], []
    for k in range(day0 - 7 * 24, stuck + 1):
        if k in H and k + 1 in H and k in Hn and k + 1 in Hn:
            asks.append((f"c{m}_{k}", xs, ys, [int(round(Hn[k] * 100)), int(round(Hn[k + 1] * 100))])); when.append(k)
    r = ask(asks)
    gaps = []
    for (wid, _, _, _), k in zip(asks, when):
        rr = r[wid]
        if rr["by"] == "CANNOT":
            gaps.append((k, None))
        else:
            e = [int(v) / 100 for v in rr["ans"].split(",")]
            gaps.append((k, round(H[k] - e[0], 3)))
    known = [(k, g) for k, g in gaps if g is not None]
    def first_over(lim):
        for i in range(len(known) - 2):
            if all(known[i + j][1] > lim for j in range(3)):
                return known[i][0]
        return None
    hours_before = lambda k: None if k is None else round((first_hot - k), 1)
    calm = [g for k, g in known if k < day0]
    checks.append(dict(sensor=m, neighbour=n, learned_on=dt.datetime.fromtimestamp(day0 * 3600, UTC).isoformat(),
                       answered_pct=round(100 * len(known) / len(gaps), 1),
                       gap_before_learning_day_C=dict(median=round(float(np.median(calm)), 3), p90_abs=round(float(np.percentile(np.abs(calm), 90)), 3)) if calm else None,
                       first_hour_gap_over_1C_hours_before_35C=hours_before(first_over(1.0)),
                       first_hour_gap_over_3C_hours_before_35C=hours_before(first_over(3.0)),
                       hours_from_35C_to_stuck=stuck - first_hot,
                       daily_gap_C={}))
    daily = collections.defaultdict(list)
    for k, g in known:
        daily[dt.datetime.fromtimestamp(k * 3600, UTC).date().isoformat()].append(g)
    checks[-1]["daily_gap_C"] = {d: round(float(np.median(v)), 2) for d, v in sorted(daily.items())}
    # an alarm: the gap's 6-hour running median leaves the band it kept in the calm week before the learning day
    gk = dict(known); roll = {}
    for k in sorted(gk):
        near = [gk[j] for j in range(k - 5, k + 1) if j in gk]
        if len(near) >= 4:
            roll[k] = float(np.median(near))
    calm_roll = [abs(v) for k, v in roll.items() if k < day0]
    band = 1.5 * max(calm_roll) if calm_roll else None
    alarm = next((k for k in sorted(roll) if k >= day0 and band is not None and roll[k] > band), None)
    checks[-1].update(alarm_band_C=None if band is None else round(band, 2),
                      alarm_hours_before_35C=None if alarm is None else first_hot - alarm,
                      alarm_hours_before_stuck=None if alarm is None else stuck - alarm,
                      reading_at_alarm_C=None if alarm is None else round(H[alarm], 2),
                      hourly_gap_C={int(k): g for k, g in known})
report["drift_check"] = checks
print("  1b drift against a healthy neighbour (learned from 12 hourly pairs on one healthy day):")
for c in checks:
    print(f"    sensor {c['sensor']:2d} vs {c['neighbour']:2d}: answered {c['answered_pct']}% of hours; calm-week band +-{c['alarm_band_C']} C; "
          f"alarm {c['alarm_hours_before_35C']} h before the reading passed 35 C and {c['alarm_hours_before_stuck']} h before it stuck "
          f"(it read {c['reading_at_alarm_C']} C then)")

# ---------------------------------------------------------------------------------------------------- 2: the motor
nm, fm = loadmat(NORMAL), loadmat(FAULT)
dn, df = nm["X097_DE_time"].ravel(), fm["X105_DE_time"].ravel()
level = lambda s: np.sqrt(np.mean(s[: len(s) // 600 * 600].reshape(-1, 600) ** 2, axis=1))
ln, lf = level(dn)[:200], level(df)[:200]
lev = np.concatenate([ln, lf]); change = len(ln); tl = np.arange(len(lev)) * 0.05
wins, meta = [], {}
for s in range(0, len(lev) - W + 1):
    xs, ys, qs, truth = frame(tl[s:s + W], lev[s:s + W], 10000, 0.005)
    ph = "before" if s + W <= change else ("after" if s >= change else "across")
    wid = f"m{s}"; wins.append((wid, xs, ys, qs)); meta[wid] = (s, ph, truth)
res = ask(wins)
rows = collections.defaultdict(list); after_answers = []
for wid, (s, ph, truth) in sorted(meta.items(), key=lambda x: x[1][0]):
    rows[ph].append((res[wid]["by"], miss(res[wid], truth, 10000)))
    if ph == "after":
        after_answers.append(res[wid]["by"] != "CANNOT")
first_after = next((i for i, a in enumerate(after_answers) if a), None)
report["motor"] = dict(level_normal_g=round(float(ln.mean()), 4), level_fault_g=round(float(lf.mean()), 4),
                       jitter_normal_g=round(float(np.median(np.abs(np.diff(ln)))), 4), jitter_fault_g=round(float(np.median(np.abs(np.diff(lf)))), 4),
                       phases={ph: summary(rows[ph]) for ph in ("before", "across", "after")},
                       first_answer_after_change_windows=first_after)
mo = report["motor"]
print(f"  2 motor (vibration level {mo['level_normal_g']} g normal -> {mo['level_fault_g']} g with the fault; "
      f"jitter {mo['jitter_normal_g']} -> {mo['jitter_fault_g']} g):")
for ph in ("before", "across", "after"):
    show(ph, mo["phases"][ph], "g")

# ---------------------------------------------------------------------------------------------------- 3: no pattern
healthy = [m for m, (ts, vs) in series.items() if len(vs) > 20000]
wins, meta = [], {}
for m in healthy:
    H = hourly[m]
    ts, vs = series[m]; ok = vs < 35
    t20 = collections.defaultdict(list)
    for t, v in zip(ts[ok], vs[ok]):
        t20[int(t // 1200)].append(v)
    keys = sorted(k for k, v in t20.items() if len(v) >= 10)
    runs = [keys[i:i + W] for i in range(0, len(keys) - W, 9) if keys[i + W - 1] - keys[i] == W - 1]
    for r in runs:
        vals = [float(np.median(t20[k])) for k in r]
        if max(vals) - min(vals) < 1.0:
            continue
        tt = [k * 1200.0 for k in r]
        wins.append((f"o{m}_{r[0]}",) + frame(tt, vals, 100, 120.0)[:3]); meta[f"o{m}_{r[0]}"] = ("as recorded", frame(tt, vals, 100, 120.0)[3])
        sh = vals[:]; rng.shuffle(sh)
        wins.append((f"s{m}_{r[0]}",) + frame(tt, sh, 100, 120.0)[:3]); meta[f"s{m}_{r[0]}"] = ("scrambled", frame(tt, sh, 100, 120.0)[3])
pairs = sorted({w[0][1:] for w in wins})
keep = set(pairs if len(pairs) <= 400 else [pairs[int(i * len(pairs) / 400)] for i in range(400)])
wins = [w for w in wins if w[0][1:] in keep]
res = ask(wins)
rows = collections.defaultdict(list)
for w in wins:
    kind, truth = meta[w[0]]; rows[kind].append((res[w[0]]["by"], miss(res[w[0]], truth, 100)))
report["no_pattern"] = {k: summary(rows[k]) for k in ("as recorded", "scrambled")}
print("  3 no pattern (healthy sensors, readings 20 minutes apart that swing at least 1 C):")
for k in ("as recorded", "scrambled"):
    show(k, report["no_pattern"][k], "C")

json.dump(report, open(OUT, "w"), indent=1)
print("saved", OUT)
