# The microcontroller libraries on four emulated Arm cores

A partner ran the brain's microcontroller libraries, **version 1.0**, in the QEMU emulator on 22 November 2026. No
real board was connected: QEMU runs a chip's instructions on a PC, but not its real timing or power.

These runs are of version 1.0, so they are checked against 1.0's reference, `../expected-1.0.txt`. Version 1.1 adds
AM's proof and changes 3 of the 300 answers (p159, p177, p236); against 1.1's reference the logs match on the other
297 lines, line for line.

| Library | Emulated core (QEMU board) | Problems | Same as the reference | Score | Rules · trends · noise | Emulator time |
|---|---|---:|---:|---:|---|---:|
| `cortex-m4f` | Cortex-M4 with FPU (`mps2-an386`) | 300 | **300 of 300** | **275** | 97 · 119 · 59 | 437 s |
| `cortex-m7` | Cortex-M7 (`mps2-an500`) | 300 | **300 of 300** | **275** | 97 · 119 · 59 | 377 s |
| `cortex-m33` | Cortex-M33 (`mps2-an505`) | 300 | **300 of 300** | **275** | 97 · 119 · 59 | 462 s |
| `cortex-m0plus` | Cortex-M0, no FPU (`microbit`) | 300 | **300 of 300** | **275** | 97 · 119 · 59 | 1,800 s |

- **All 1,200 lines are byte-for-byte the same as 1.0's reference** (`../expected-1.0.txt`): who answered,
  both answers, the self-check, right or wrong. Who answered is the same on every core too: AM 80, AM's splitter 29,
  the net 127, CANNOT 64.
- On every core the brain reported 5,216 bytes of state. Its stack bound was 2,960 bytes on the M4F and the M33,
  2,944 on the M7 and 2,984 on the M0.
- The M0 has no floating-point unit, so all of its floating-point maths runs in software, and it still gives the same
  answers.
- The times are the emulator's on a PC, not the chips' speed.
- The libraries linked with the standard GNU Arm toolchain on Windows (arm-none-eabi-gcc 13.3.0 with newlib, through
  MSYS2), and ran in QEMU 9.2.1.

Check the logs yourself:

```bash
python3 runners/wokwi/check_log.py results/qemu/logs/m4f.log results/expected-1.0.txt
python3 score.py results/qemu/logs/m4f.log
```

## Files

| Path | What it is |
|---|---|
| `logs/m4f.log`, `m7.log`, `m33.log`, `m0.log` | each core's output, as the emulator printed it |
| `times.txt` | how long each run took in the emulator |
| `src/main_qemu.c` | the test program: the same loop as `runners/computer/run300.c`, printing through Arm semihosting |
| `src/mps2.ld`, `src/an505.ld`, `src/microbit.ld` | a linker script for each emulated board |
| `src/startup_cortex_m.c` | a minimal Cortex-M start-up: the vector table, the FPU switched on, then `main()` |

## Run it yourself

From the top of this repository, with arm-none-eabi-gcc and QEMU. This is the M4F:

```bash
arm-none-eabi-gcc -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Os -ffreestanding -nostartfiles \
  -Wl,--gc-sections -Wl,--no-enum-size-warning -Ibrain/include -Irunners/computer -T results/qemu/src/mps2.ld \
  results/qemu/src/main_qemu.c runners/computer/harness.c results/qemu/src/startup_cortex_m.c \
  brain/lib/cortex-m4f/libaa.a -lc -lgcc -o m4f.elf
qemu-system-arm -M mps2-an386 -nographic -monitor none -serial none -semihosting-config enable=on,target=native -kernel m4f.elf
```

| Core | Compiler flags | Linker script | QEMU board |
|---|---|---|---|
| M7 | `-mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard`, library `cortex-m7` | `mps2.ld` | `mps2-an500` |
| M33 | `-mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard`, library `cortex-m33` | `an505.ld` | `mps2-an505` |
| M0 | `-mcpu=cortex-m0`, no FPU flags, library `cortex-m0plus` | `microbit.ld` | `microbit` |

Without `-Wl,--no-enum-size-warning`, the GNU linker warns that the libraries use 32-bit enums. That warning is
harmless here, because the brain's interface has no enums.
