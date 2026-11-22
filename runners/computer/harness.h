/* The 300 problems, shared by the computer runner (run300.c) and the Pico program (../pico/src/main_pico.c),
   so that both print exactly the same line for every problem.  Only the SDK's public header is used. */
#ifndef HARNESS_H
#define HARNESS_H
#include <stdint.h>
#include "aa.h"
#include "stream_data.h"

/* solves problem i with the brain; returns the SDK's return code (AA_OK = 0) */
int32_t h_solve(int i, aa_brain *brain, aa_answer *a);
/* 1 if the answer is right: a rule both answers exact, a trend both within its tolerance, noise CANNOT */
int h_right(int i, const aa_answer *a);
/* the line printed for problem i (no newline): the same on every device, timing not included */
void h_line(char *buf, int i, int32_t rc, const aa_answer *a, int right);
/* a 64-bit integer as text, without the C library (some small printf versions cannot print 64-bit numbers) */
char *h_i64(char *p, int64_t v);
#endif
