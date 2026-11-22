/* The kit's 300-problem test on an emulated Cortex-M under QEMU: same harness as test-300/computer,
   output through Arm semihosting (bkpt 0xAB).  Prints one line per problem, then SUMMARY, then exits QEMU. */
#include <stdint.h>
#include "harness.h"
static aa_brain brain;
static int sh(int op, const void *arg) {
    register int r0 __asm__("r0") = op; register const void *r1 __asm__("r1") = arg;
    __asm__ volatile ("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory"); return r0;
}
static void puts_(const char *s) { sh(0x04, s); }
static char *cat(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }
int main(void) {
    char line[200], *p; int right = 0, fam[3] = {0,0,0};
    aa_info_t info; aa_info(&info);
    p = cat(line, "LIBRARY "); p = cat(p, info.target); p = cat(p, " brain "); p = h_i64(p, info.brain_bytes);
    p = cat(p, " stack bound "); p = h_i64(p, info.stack_bytes); cat(p, "\n"); puts_(line);
    if (aa_brain_init(&brain) != AA_OK) { puts_("brain did not start\n"); }
    else for (int i = 0; i < N_PROBLEMS; i++) {
        aa_answer a; int32_t rc = h_solve(i, &brain, &a); int ok = rc == AA_OK && h_right(i, &a);
        right += ok; fam[PROBLEMS[i].family] += ok;
        h_line(line, i, rc, &a, ok); p = line; while (*p) p++; cat(p, "\n"); puts_(line);
    }
    p = cat(line, "SUMMARY right "); p = h_i64(p, right); p = cat(p, "/300 rules "); p = h_i64(p, fam[0]);
    p = cat(p, "/120 trends "); p = h_i64(p, fam[1]); p = cat(p, "/120 noise "); p = h_i64(p, fam[2]); cat(p, "/60\n");
    puts_(line);
    static const uint32_t ex[2] = {0x20026, 0}; sh(0x18, (const void *)0x20026); sh(0x20, ex);
    for (;;) {}
}
