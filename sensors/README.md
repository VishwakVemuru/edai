# edai on real sensor data

The same brain, unchanged, on real sensors:
- lab recordings of temperature sensors that drift and fail;
- a motor that develops a bearing fault;
- a MacBook's chip temperature, read live;
- an iPhone's tilt sensor, with the brain running on the phone.

## The question it is asked

Every time it's the same kind of question as edai's noisy trends. It gets 14 readings in a row: 12 are given as examples
(x = when, y = the reading as a whole number), and the 4th and 9th are hidden and asked. The brain answers both (with
an exact rule, or with an estimate that passed its self-check) or it says CANNOT. A "miss" is the larger of its two
errors against the real hidden readings.

## 1. A drifting temperature sensor

These are Intel Berkeley Lab recordings from 2004: 54 temperature sensors in an office lab. When a sensor's battery runs
down, its reading creeps up from about 22 °C to 122 °C in roughly a day, then sticks at 122.15 °C. Six of these sensors
were tested, in windows of 5-minute steps (70 minutes per window):

| Phase | Windows | Answered | Typical miss | 90% of misses under |
|---|---|---|---|---|
| healthy | 1,800 | 87% | 0.03 °C | 0.12 °C |
| drifting (35 to 122 °C) | 1,129 | 84% | 0.21 °C | 0.61 °C |
| stuck at 122.15 °C | 504 | 99.8% | 0.003 °C | 0.003 °C |

It keeps estimating while the sensor drifts. On its own it can't tell that the sensor is wrong, because a reading stuck
at 122 °C is perfectly regular. With a second sensor it can.

**Against a healthy neighbour.** The brain learns how a neighbouring sensor's reading relates to this sensor's, from 12
hourly pairs on one healthy day. Then, every hour, it says what this sensor should read. An alarm goes off when the
gap leaves the band it kept in the calm week before.

| Sensor | Neighbour | Alarm, hours before the reading passed 35 °C | Hours before it stuck | It read then |
|---|---|---|---|---|
| 31 | 29 | 13 | 25 | 24.6 °C |
| 26 | 29 | 14 | 37 | 24.2 °C |
| 23 | 30 | 1 | 23 | 33.2 °C |
| 22 | 29 | −1 (an hour after) | 21 | 34.6 °C |

Sensors 31 and 26 set off the alarm while they still read a normal-looking 24 °C. Sensors 23 and 22 tracked their
neighbours only loosely, so their alarms came at about 35 °C, still nearly a day before they stuck.

## 2. A motor whose behaviour changes

This is from the Case Western Reserve University bearing data. A motor runs at 0 hp, and its drive-end vibration is
recorded at 12 kHz: 10 s of normal running (97.mat), then 10 s with an inner-race bearing fault (105.mat), joined at a
known point. The reading is the vibration level (RMS) of each 0.05 s. It rose from 0.073 g to 0.291 g.

| Windows | Count | Answered |
|---|---|---|
| steady, normal | 187 | 0.5% |
| across the change | 13 | 100% (typical miss 0.019 g) |
| steady, with the fault | 187 | 3.2% |

In steady running the level only jitters, so the brain says CANNOT: there's nothing to learn. It answered every window
that spanned the change, so its answers mark the moment the motor's behaviour changed.

## 3. A reading that suddenly follows no pattern

These are healthy lab readings 20 minutes apart, which swing by at least 1 °C. They were asked once as recorded, and
once with the same values scrambled in time:

| | Windows | Answered |
|---|---|---|
| as recorded | 400 | 90% (typical miss 0.09 °C) |
| scrambled (no pattern) | 400 | 3% |

## Live: a MacBook's chip temperature

`mac_live.py` puts the chip to work in five parts over 6 minutes. Meanwhile it reads the chip's die temperature
sensors through `hidtemp`, about once a second. The brain is asked every 2 readings.

| Part | Asked | Answered | Typical miss |
|---|---|---|---|
| idle | 16 | 10 | 0.14 °C |
| heavy work: the chip creeps up from 45 to 54 °C | 33 | 33 | 0.08 °C |
| half work: the behaviour changes | 17 | 16 | 0.07 °C |
| work switched on and off at random | 14 | 6 | 0.41 °C |
| cooling | 15 | 15 | 0.04 °C |
| windows across a change of part | 25 | 19 | 0.11 °C |

## Live: an iPhone's tilt, with the brain on the phone

`phone/index.html` holds the brain compiled to WebAssembly, the same copy as in `runners/browser`. On the phone it reads
the tilt from the accelerometer 10 times a second and asks the brain every 2 readings, while you follow the steps on the
screen. Each question took about 69 ms on the iPhone. Two runs:

| Step | Run 1 answered | Typical miss | Run 2 answered | Typical miss |
|---|---|---|---|---|
| hold still | 16% | 0.3° | 28% | 0.4° |
| tilt slowly | 75% | 0.7° | 85% | 1.0° |
| tilt faster: the motion changes | 97% | 2.1° | 98% | 1.4° |
| shake randomly: no pattern | 9% | 3.6° | 9% | 6.3° |
| hold still again | 13% | 2.9° | 19% | 4.9° |

## What it shows

- **A pattern, to the brain, is measured against the spread of the readings it's given.** Readings that only jitter
  around a steady level have nothing beyond the jitter, so it says CANNOT. Read that as "steady, nothing to learn". At
  a longer time scale, the same lab sensors were answered 87% of the time.
- **It follows drift and smooth change closely**, and picks up a new behaviour from the latest readings straight away.
- **It refuses readings that have no pattern:** 97% of the scrambled readings, and 91% of the shaking.
- **One sensor alone can't reveal its own drift.** Against a neighbour, the brain caught it up to 14 hours before the
  reading passed 35 °C.
- **On real sensor data every answer came from the net, or was CANNOT.** Such readings have no exact rule.

## Replicate it

**The lab recordings** need Python 3 with numpy and scipy. Building `sensor_run` takes a moment; the test then takes
about a minute on a 10-core laptop.

```bash
cd sensors && make
mkdir -p data/intel data/cwru-normal data/cwru-fault
curl -L -o data/intel/data.txt.gz http://db.csail.mit.edu/labdata/data.txt.gz
curl -L -o data/cwru-normal/97.mat https://engineering.case.edu/sites/default/files/97.mat
curl -L -o data/cwru-fault/105.mat https://engineering.case.edu/sites/default/files/105.mat
python3 -I lab.py ./sensor_run data/intel/data.txt.gz data/cwru-normal/97.mat data/cwru-fault/105.mat my-lab.json
```

The three files are 34 MB, 3.9 MB and 2.9 MB.

**The Mac test** runs on macOS on Apple silicon. It keeps every core busy for about 4 of its 6 minutes, so plug the
charger in.

```bash
make hidtemp
python3 mac_live.py ./sensor_run ./hidtemp out/mac-live.json
```

**The phone test** needs the `openssl` command on the computer, to make the page's certificate.

```bash
python3 phone/serve.py
```

1. Open the https address it prints on the phone, which must be on the same Wi-Fi as the computer.
2. Accept the certificate warning once; the certificate is made on your computer.
3. Tap **Start**, allow motion access, and follow the steps.

The run is saved on the computer in `out/phone/`, and `python3 phone_report.py out/phone/*.json` summarizes it. On a
computer, open the page with `?test` to try it with made-up readings.

## Files

| Path | What it holds |
|---|---|
| `sensor_run.c`, `Makefile` | the brain on windows of readings, one per line of standard input, through `../brain` |
| `lab.py` | the lab-recording tests: drift, the neighbour check, the motor, scrambled readings |
| `hidtemp.c`, `mac_live.py` | the live test on a Mac's chip temperature |
| `phone/index.html`, `phone/serve.py` | the phone page with the brain inside, and its small https server |
| `phone_report.py` | the step-by-step summary of saved phone runs |
| `results/` | the runs above: `lab.json`, `mac-chip-live.json`, `iphone-run-1.json`, `iphone-run-2.json` |

The data comes from the Intel Berkeley Research Lab (db.csail.mit.edu/labdata) and the Case Western Reserve University
Bearing Data Center (engineering.case.edu/bearingdatacenter). It isn't copied here; the commands above fetch it.
