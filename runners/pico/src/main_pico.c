/* The brain's 300-problem test on a Raspberry Pi Pico 2 (or the original Pico), with the SDK's own library for that
   chip (the one a customer would link).
     On a real board: core 1 runs the brain on a 16 KB stack of its own; core 0 talks to the computer over USB.  The test
       starts when the computer opens the serial port (read.py does), prints one line per problem (the same line the
       Mac prints, plus the time it took), then a summary: the score, the time per kind of problem, and how much stack
       the brain used.
     In the Wokwi simulator (AA_WOKWI=1): one core does everything, the text goes to UART0 (the simulator's serial
       monitor), and the test starts at once.  AA_QUICK=1 runs only the quick set of 9 problems. */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "harness.h"

#ifndef AA_WOKWI
#define AA_WOKWI 0
#endif
#ifndef AA_QUICK
#define AA_QUICK 0
#endif
#if AA_QUICK
#define N_RUN N_QUICK
#define IDX(j) ((int)QUICK_IDX[j])
#else
#define N_RUN N_PROBLEMS
#define IDX(j) (j)
#endif

/* the Cortex-M33's own cycle counter (DWT), on the core that reads it; not on the RP2040's Cortex-M0+, not in a simulator */
#if PICO_RP2350 && !AA_WOKWI
#define HAVE_CYCLES 1
#define DEMCR      (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define READ_CYCLES() DWT_CYCCNT
#else
#define HAVE_CYCLES 0
#define READ_CYCLES() 0u
#endif

#define STACK_WORDS (16 * 1024 / 4)
#define PAINT 0xA5A5A5A5u

typedef struct { int32_t rc; aa_answer a; int right; uint32_t us; uint32_t cycles; } result_t;

static uint32_t core1_stack[STACK_WORDS];
static aa_brain brain;
static result_t results[N_RUN];
static volatile uint32_t done = 0;
static volatile int32_t init_rc = 1;

/* what the summary needs, kept out of the (small) main stack */
static uint32_t rule_us[N_RUN], trend_us[N_RUN], noise_us[N_RUN];
static int nr, nt, nn, right_total, fam_right[3], who[5];
static uint64_t total_us;
static char line[192];

static void solve_one(int j)
{
    const int i = IDX(j);
    result_t r;
    uint64_t t0 = time_us_64(); uint32_t c0 = READ_CYCLES();
    r.rc = h_solve(i, &brain, &r.a);
    uint32_t c1 = READ_CYCLES(); uint64_t t1 = time_us_64();
    r.us = (uint32_t)(t1 - t0); r.cycles = c1 - c0;          /* the cycle count wraps after about 28 s at 150 MHz */
    r.right = r.rc == AA_OK && h_right(i, &r.a);
    results[j] = r;
}

static void brain_core(void)                                  /* core 1, on a real board */
{
#if HAVE_CYCLES
    DEMCR |= 1u << 24; DWT_CYCCNT = 0; DWT_CTRL |= 1u;
#endif
    init_rc = aa_brain_init(&brain);
    if (init_rc != AA_OK) return;
    for (int j = 0; j < N_RUN; j++) { solve_one(j); __dmb(); done = (uint32_t)(j + 1); }
}

static void ms_text(char *b, uint32_t us) { snprintf(b, 24, "%lu.%03lu", (unsigned long)(us / 1000), (unsigned long)(us % 1000)); }

static void report(int j)
{
    const result_t *r = &results[j];
    const int pi = IDX(j);
    char t[24];
    h_line(line, pi, r->rc, &r->a, r->right);
    ms_text(t, r->us);
    if (HAVE_CYCLES) printf("%s ms=%s cycles=%lu\n", line, t, (unsigned long)r->cycles);
    else printf("%s ms=%s cycles=none\n", line, t);
    right_total += r->right; fam_right[PROBLEMS[pi].family] += r->right; total_us += r->us;
    if (r->rc == AA_OK && r->a.by >= 0 && r->a.by <= 4) who[r->a.by]++;
    if (PROBLEMS[pi].family == 0) rule_us[nr++] = r->us; else if (PROBLEMS[pi].family == 1) trend_us[nt++] = r->us; else noise_us[nn++] = r->us;
    gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN);
}

static void sort_u32(uint32_t *v, int n)
{
    for (int i = 1; i < n; i++) { uint32_t k = v[i]; int j = i - 1; while (j >= 0 && v[j] > k) { v[j + 1] = v[j]; j--; } v[j + 1] = k; }
}

int main(void)
{
    stdio_init_all();
    gpio_init(PICO_DEFAULT_LED_PIN); gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#if AA_WOKWI
    sleep_ms(300);                     /* no USB in the simulator: its serial monitor reads UART0 from the start */
#else
    while (!stdio_usb_connected()) { gpio_xor_mask(1u << PICO_DEFAULT_LED_PIN); sleep_ms(150); }   /* fast blink: waiting for the Mac */
    sleep_ms(800);
#endif

    aa_info_t info; aa_info(&info);
#if PICO_RP2350
    const char *board = "Raspberry Pi Pico 2 (RP2350, Cortex-M33 with a floating-point unit)";
#else
    const char *board = "Raspberry Pi Pico (RP2040, Cortex-M0+, no floating-point unit)";
#endif
    printf("EDAI PICO TEST\n");
    printf("BOARD %s at %lu MHz, %s\n", board, (unsigned long)(clock_get_hz(clk_sys) / 1000000),
           AA_WOKWI ? "simulated in Wokwi, one core" : "one core for the brain");
    printf("LIBRARY %s, version %lu.%lu.%lu, brain %lu bytes, stack bound %lu bytes\n", info.target,
           (unsigned long)(info.version >> 16), (unsigned long)((info.version >> 8) & 255), (unsigned long)(info.version & 255),
           (unsigned long)info.brain_bytes, (unsigned long)info.stack_bytes);
    printf("START %d problems%s\n", N_RUN, AA_WOKWI ? " (times are the simulator's)" : "");

#if AA_WOKWI
    init_rc = aa_brain_init(&brain);
    if (init_rc != AA_OK) { printf("ERROR the brain did not start: %ld\n", (long)init_rc); for (;;) sleep_ms(1000); }
    for (int j = 0; j < N_RUN; j++) { solve_one(j); report(j); }
#else
    for (int i = 0; i < STACK_WORDS; i++) core1_stack[i] = PAINT;
    multicore_launch_core1_with_stack(brain_core, core1_stack, sizeof core1_stack);
    uint32_t printed = 0;
    while (printed < N_RUN) {
        if (init_rc != AA_OK && init_rc != 1) { printf("ERROR the brain did not start: %ld\n", (long)init_rc); break; }
        if (done > printed) { __dmb(); report((int)printed); printed++; }
        else sleep_ms(2);
    }
#endif

    sort_u32(rule_us, nr); sort_u32(trend_us, nt); sort_u32(noise_us, nn);
    printf("SUMMARY right %d/%d rules %d/%d trends %d/%d noise %d/%d\n", right_total, N_RUN, fam_right[0], nr, fam_right[1], nt, fam_right[2], nn);
    printf("WHO AM %d SPLITTER %d AM_EST %d NET %d CANNOT %d\n", who[1], who[2], who[4], who[3], who[0]);
    char a[24], b[24];
    if (nr) { ms_text(a, rule_us[nr / 2]); ms_text(b, rule_us[nr - 1]); printf("TIME rules median %s ms, longest %s ms\n", a, b); }
    if (nt) { ms_text(a, trend_us[nt / 2]); ms_text(b, trend_us[nt - 1]); printf("TIME trends median %s ms, longest %s ms\n", a, b); }
    if (nn) { ms_text(a, noise_us[nn / 2]); ms_text(b, noise_us[nn - 1]); printf("TIME noise median %s ms, longest %s ms\n", a, b); }
    printf("TIME all %lu.%01lu s\n", (unsigned long)(total_us / 1000000), (unsigned long)(total_us / 100000 % 10));
#if AA_WOKWI
    printf("STACK not measured in the simulator\n");
#else
    uint32_t untouched = 0;
    while (untouched < STACK_WORDS && core1_stack[untouched] == PAINT) untouched++;
    printf("STACK the brain used %lu of %lu bytes\n", (unsigned long)((STACK_WORDS - untouched) * 4), (unsigned long)(STACK_WORDS * 4));
#endif
    printf("DONE\n");
    for (;;) { gpio_put(PICO_DEFAULT_LED_PIN, 1); sleep_ms(1000); gpio_put(PICO_DEFAULT_LED_PIN, 0); sleep_ms(1000); }   /* slow blink: finished */
}
