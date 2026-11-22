# The 300 problems on a Raspberry Pi Pico 2 (or the original Pico)

- `edai-pico2.uf2` is for a Pico 2 (RP2350). `edai-original-pico.uf2` is for the original Pico (RP2040).
- The brain runs on the chip's second core. The first core reports each problem's line over USB, and `read.py`
  checks every line against `expected.txt`.

**On a Mac:** hold BOOTSEL, plug the board in, then run `bash run.sh`.

**On Linux:**
1. Hold BOOTSEL and plug the board in.
2. Copy the matching `.uf2` file onto the drive that appears.
3. Run `python3 read.py`.

The LED blinks fast while it waits for the computer, flickers once per problem, and blinks slowly when it's done.
The run ends with `SUMMARY right 275/300` if the board gives the same answers as the reference.

`src/main_pico.c` is the program's source, for reference. It uses only the brain's public header. It was built with
the Raspberry Pi Pico SDK 2.3.1 against `../../brain/lib/cortex-m33/libaa.a` (hard float) and
`../../brain/lib/cortex-m0plus/libaa.a`, together with `../computer/harness.c` and `stream_data.h`. The simulator
programs in `../wokwi` are the same source built with `AA_WOKWI=1`, and `AA_QUICK=1` for the quick set.
