# The brain on an Arduino Uno (the Uno edition)

The brain squeezed onto the Arduino Uno's ATmega328P, an 8-bit chip with 2 KB of RAM and 32 KB of flash, running at
16 MHz. It keeps the same design as the full brain:
- **AM** finds the exact rule behind the examples, and **its splitter** finds rules that hold in pieces.
- **A small neural net** learns the examples when there is no exact rule, and checks itself first.
- **Otherwise it says CANNOT.**

It runs entirely on the Uno, and the board answers each problem itself.

## On a real Uno

A partner ran it on a real Arduino Uno on 22 November 2026. Its log is in `../../results/uno/real-uno-run.txt`.
- **9 of 9 right** (3 rules, 3 trends, 3 noise), and **all 9 lines the same as the reference** in `expected.txt`: who
  answered, both answers, the self-check, right or wrong.
- **Memory, measured on the chip:** at its peak, the program used 1,901 of the 2,048 bytes of RAM. 147 bytes were never
  touched.
- **Time:** 92 minutes for the 9. An exact rule takes from under a second to 6 minutes; a problem that needs the full
  search and the net takes 10 to 16 minutes.
- **Power:** the partner's USB meter showed 0.25 to 0.5 W for the whole board. Most of that is the board's USB chip,
  regulator and LEDs, not the ATmega328P.

## How it differs from the full brain

| | Full brain | Uno edition |
|---|---|---|
| Problems | rules x → y, sequences, noisy trends, noise | rules x → y, noisy trends and noise; **no sequences** |
| Examples per problem | up to 16 | up to 12 (the edai problems have at most 12) |
| The net | 72 hidden units (289 numbers) | **8 hidden units (33 numbers)** |
| The brain's memory | 5,216 bytes, plus at most 3 KB of stack | **782 bytes**, plus the stack; the net borrows the memory AM's search has just given back |
| Program size | 17 to 27 KB of code | the whole program is 31.9 KB of the Uno's 32 KB |
| Arithmetic | single and double precision | single precision only (the Uno has no 64-bit floating point) |

AM and its splitter are the same code with smaller buffers: the 22 splitter pieces are all that 12 examples can need,
so their answers don't change.

**On a computer, with the same code,** the Uno edition gets **216 of the 242 x → y problems** among the main 300 right,
against **219** for the full brain on the same problems. **All 71 of its exact-rule answers are the same as the full
brain's;** it loses only 3 noisy trends, because its net is so much smaller.

## Run it

**With the Arduino IDE:**
1. Copy the folder `libraries/EdaiUnoBrain` into your Arduino libraries folder (`Documents/Arduino/libraries`).
2. Open `EdaiUno/EdaiUno.ino`.
3. Choose **Tools → Board → Arduino Uno** and the Uno's port, then press **Upload**.
4. Open the Serial Monitor at **115200 baud**, and leave it running for about an hour and a half, until `DONE`.
5. Copy the Serial Monitor's text into a file and check it with `python3 check_log.py that-file.txt`. Every answered
   line must be the same as `expected.txt`.

**The ready program, without compiling anything:**
- **With avrdude:**
  `avrdude -p atmega328p -c arduino -P <the Uno's port> -b 115200 -U flash:w:EdaiUno.ino.hex:i`
- **Without hardware, in the Wokwi simulator:** open https://wokwi.com/projects/new/arduino-uno, click into the code
  editor, press **F1**, choose **Upload Firmware and Start Simulation...**, and pick `EdaiUno.ino.hex`.

## Files

| Path | What it is |
|---|---|
| `libraries/EdaiUnoBrain/` | the brain, precompiled for the ATmega328P (an Arduino precompiled library) |
| `EdaiUno/EdaiUno.ino`, `EdaiUno/problems_uno.h` | the sketch, and the 9 problems of the quick set |
| `EdaiUno.ino.hex` | the ready program, built from them with Arduino's own compiler (AVR core 1.8.8) |
| `expected.txt` | the reference: the same code's answers on a computer |
| `check_log.py` | checks a saved run against `expected.txt` |
