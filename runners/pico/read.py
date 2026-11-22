"""Reads the Pico's 300-problem run over USB (a Mac or Linux computer), saves it, and checks it against the reference.

    python3 read.py                    waits for the board's serial port, starts the test by opening it, shows every
                                       line live, and saves everything to out/run-<time>.log
    python3 read.py --check <log>      checks a saved log again

The check: every problem's line must be the very same as the reference line in expected.txt (who answered, both answers,
the self-check, right or wrong); only the time the board took is left out.  The report also records the
SHA-256 of the program that was copied onto the board and of expected.txt, so anyone can repeat the test exactly.
Needs nothing beyond Python 3."""
import glob
import hashlib
import os
import re
import sys
import termios
import time
import tty

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out")


def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest() if os.path.exists(path) else "missing"


def find_port(wait_s=180):
    t0 = time.time()
    while time.time() - t0 < wait_s:
        ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
        if ports:
            return ports[0]
        time.sleep(0.5)
    sys.exit("No Pico found on USB. Is the board plugged in with a data cable?")


def read(port, log_path, timeout_s=12 * 3600):
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd); attrs[6][termios.VMIN] = 0; attrs[6][termios.VTIME] = 5   # reads return after 0.5 s
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    lines, buf, t0, last = [], b"", time.time(), time.time()
    with open(log_path, "w") as log:
        while time.time() - t0 < timeout_s:
            try:
                chunk = os.read(fd, 4096)
            except OSError:
                print("\nThe board went away (unplugged or restarted)."); break
            if chunk:
                last = time.time(); buf += chunk
                while b"\n" in buf:
                    raw, buf = buf.split(b"\n", 1)
                    s = raw.decode("utf-8", "replace").rstrip("\r")
                    lines.append(s); log.write(s + "\n"); log.flush(); print(s, flush=True)
                    if s == "DONE":
                        os.close(fd); return lines
            elif time.time() - last > 600:
                print("\nNo line from the board for 10 minutes; stopping."); break
    os.close(fd)
    return lines


def check(lines, log_path):
    want = [l for l in open(os.path.join(HERE, "expected.txt")).read().splitlines() if l.startswith("p")]
    got = [l for l in lines if re.match(r"^p\d{3} ", l)]
    strip = lambda l: re.sub(r" ms=\S+ cycles=\S+$", "", l)
    same = sum(1 for a, b in zip(want, got) if strip(b) == a)
    diffs = [(a, strip(b)) for a, b in zip(want, got) if strip(b) != a]
    ms = [float(m.group(1)) for l in got if (m := re.search(r" ms=([\d.]+)", l))]
    summary = [l for l in lines if l.split(" ")[0] in ("EDAI", "BOARD", "LIBRARY", "START", "SUMMARY", "WHO", "TIME", "STACK")]
    report = ["CHECK of " + os.path.basename(log_path),
              f"problems answered by the board: {len(got)} of {len(want)}",
              f"same line as the reference (who answered, both answers, self-check, right or wrong): {same} of {len(want)}",
              f"board time for all answered problems: {sum(ms) / 1000:.1f} s" if ms else "board time: none",
              "program for the Pico 2: edai-pico2.uf2 sha256 " + sha(os.path.join(HERE, "edai-pico2.uf2")),
              "program for the original Pico: edai-original-pico.uf2 sha256 " + sha(os.path.join(HERE, "edai-original-pico.uf2")),
              "reference: expected.txt sha256 " + sha(os.path.join(HERE, "expected.txt")), ""] + summary
    if diffs:
        report += ["", "lines that differ (reference | board):"] + [f"  {a}  |  {b}" for a, b in diffs[:20]]
    text = "\n".join(report)
    open(log_path.replace(".log", "-check.txt"), "w").write(text + "\n")
    print("\n" + text)
    return same == len(want) == len(got)


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[1] == "--check":
        ok = check(open(sys.argv[2]).read().splitlines(), sys.argv[2]); sys.exit(0 if ok else 1)
    port = sys.argv[2] if len(sys.argv) > 2 and sys.argv[1] == "--port" else None
    os.makedirs(OUT, exist_ok=True)
    log_path = os.path.join(OUT, time.strftime("run-%Y%m%d-%H%M%S.log"))
    port = port or find_port()
    print(f"Reading the board on {port}; the test starts now.  (Saving to {log_path})\n", flush=True)
    ok = check(read(port, log_path), log_path)
    sys.exit(0 if ok else 1)
