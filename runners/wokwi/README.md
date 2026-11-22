# The 300 problems on a simulated Raspberry Pi Pico 2, in the browser

No hardware needed: [Wokwi](https://wokwi.com) simulates the Pico 2 (RP2350, Cortex-M33 at 150 MHz) in a web browser.

| File | What it runs | How long in the simulator |
|---|---|---|
| `edai-wokwi-pico2-quick.uf2` | 9 problems: 3 rules, 3 trends, 3 noise | a few minutes |
| `edai-wokwi-pico2-full.uf2` | all 300 problems | more than an hour |

1. Open https://wokwi.com/projects/new/pi-pico-2. Change nothing: its board already sends its text output (GP0 and
   GP1) to the serial monitor.
2. Click into the code editor, press **F1**, choose **Upload Firmware and Start Simulation...**, and pick a `.uf2` file.
3. The serial monitor shows one line per problem, then the score. The quick set ends with `SUMMARY right 9/9`, the
   full set with `SUMMARY right 274/300`.
4. Copy the serial monitor's text into a file, then run `python3 check_log.py that-file.txt`. It compares every line with
   the reference answers in `expected.txt`.

In these programs one core does everything and the times are the simulator's. The real-board programs are in
`../pico`.
