"""edai's published brain, live, on this Mac's own chip temperature sensor.

    python3 mac_live.py ./sensor_run ./hidtemp out/mac-live.json      (macOS on Apple silicon; make sensor_run hidtemp first)

The reading is the chip's temperature (the mean of its die sensors, read through the HID event system as activity
monitors do), averaged over each second.  Every 2 seconds the brain is asked about the last 14 readings the same way as
the lab recordings: 12 given, the 4th and 9th held back and asked.  It answers (an exact rule, or an estimate that
passed its self-check) or says CANNOT.

Five parts, about 6 minutes, nothing for you to do:
    idle          nothing running: the reading only jitters
    heavy work    every core busy: the chip heats up and keeps creeping up (a drift)
    half work     half the cores: the behaviour changes, the temperature settles somewhere else
    random work   the work switches on and off at random every 1 to 4 s: no pattern
    cooling       nothing running: the chip cools down"""
import json
import os
import random
import signal
import subprocess
import sys
import time

RUNNER, HID, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
W, HOLD = 14, (4, 9)
EX = [i for i in range(W) if i not in HOLD]
SCALE = float(os.environ.get("MAC_LIVE_SCALE", "1"))                 # a shorter trial run
PARTS = [(n, s * SCALE) for n, s in [("idle", 60), ("heavy work", 120), ("half work", 60), ("random work", 60), ("cooling", 60)]]
CORES = os.cpu_count() or 8
rng = random.Random(11)


class Work:
    def __init__(self):
        self.procs = []

    def set(self, n):
        while len(self.procs) > n:
            p = self.procs.pop(); p.terminate(); p.wait()
        while len(self.procs) < n:
            self.procs.append(subprocess.Popen(["yes"], stdout=subprocess.DEVNULL))


work = Work()
brain = subprocess.Popen([RUNNER], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
sensor = subprocess.Popen([HID, "250"], stdout=subprocess.PIPE, text=True, bufsize=1)


def chip():
    """one sample: the mean of the die sensors"""
    line = sensor.stdout.readline()
    vals = [float(kv.rsplit("=", 1)[1]) for kv in line.strip().split("\t") if kv.startswith("PMU tdie")]
    return sum(vals) / len(vals) if vals else None


def ask(vals):
    xs = [i * 10 for i in EX]; ys = [int(round(vals[i] * 100)) for i in EX]; qs = [i * 10 for i in HOLD]
    brain.stdin.write(f"w {len(xs)} {' '.join(map(str, xs))} {' '.join(map(str, ys))} 2 {' '.join(map(str, qs))}\n")
    brain.stdin.flush()
    d = dict(kv.split("=", 1) for kv in brain.stdout.readline().split()[1:])
    truth = [vals[i] for i in HOLD]
    if d["by"] == "CANNOT":
        return d["by"], None
    a = [int(v) / 100 for v in d["ans"].split(",")]
    return d["by"], max(abs(a[0] - truth[0]), abs(a[1] - truth[1]))


readings, parts_of, log = [], [], []


def save():
    json.dump(dict(cores=CORES, parts=PARTS, readings=readings, part_of_reading=parts_of, answers=log), open(OUT, "w"), indent=1)


def finish(*_):
    work.set(0); sensor.kill(); brain.kill(); save(); sys.exit(0)


signal.signal(signal.SIGINT, finish)
print(f"edai's brain on this Mac's chip temperature: {int(sum(s for _, s in PARTS) / 60 + 0.5)} minutes, a reading every second, "
      f"the brain asked every 2 s\n")
print(f"{'time':>5}  {'part':12}  {'chip temp':>9}   brain")
t0 = time.time()
try:
    for name, secs in PARTS:
        end = time.time() + secs; flip = 0
        work.set({"heavy work": CORES, "half work": CORES // 2}.get(name, 0))
        while time.time() < end:
            if name == "random work" and time.time() >= flip:
                work.set(CORES if rng.random() < 0.5 else 0); flip = time.time() + rng.uniform(1, 4)
            s = [v for v in (chip() for _ in range(4)) if v is not None]
            if not s:
                continue
            readings.append(round(sum(s) / len(s), 3)); parts_of.append(name)
            if len(readings) >= W and len(readings) % 2 == 0:
                win = readings[-W:]; wp = parts_of[-W:]
                by, miss = ask(win)
                part = wp[0] if all(p == wp[0] for p in wp) else "change"
                log.append(dict(t=len(readings), part=part, by=by, miss=miss))
                verdict = f"{by:8s}" + (f" missed by {miss:.2f} C" if miss is not None else " no pattern it can trust")
                print(f"{len(readings):5d}  {name:12}  {readings[-1]:7.2f} C   {verdict}", flush=True)
finally:
    work.set(0)
    save()

print("\nWhat the brain did (answered = an exact rule or a checked estimate; the rest CANNOT):")
for name in [n for n, _ in PARTS] + ["change"]:
    rows = [a for a in log if a["part"] == name]
    if not rows:
        continue
    got = sorted(a["miss"] for a in rows if a["by"] != "CANNOT")
    med = f", typical miss {got[len(got) // 2]:.2f} C" if got else ""
    span = [readings[i] for i, p in enumerate(parts_of) if p == name]
    rng_txt = f"  (chip {min(span):.1f} to {max(span):.1f} C)" if span else ""
    print(f"  {name:12} answered {len(got):3d} of {len(rows):3d}{med}{rng_txt}")
print(f"\nsaved {OUT}")
sensor.kill(); brain.kill()
