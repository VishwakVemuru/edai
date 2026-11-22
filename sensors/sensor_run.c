/* edai's brain on windows of sensor readings, through the compiled library in ../brain (untouched).

   Standard input, one window per line:   <id> <n> x1 .. xn y1 .. yn <nq> q1 [q2]        (whole numbers)
   Standard output, one line per window:  <id> by=<AM|SPLITTER|NET|CANNOT> ans=<a,b|CANNOT> [check=<thousandths> est=<e1,e2>]
     by      who answered: AM (an exact rule), SPLITTER (an exact rule in pieces), NET (an estimate that passed the
             net's self-check), CANNOT (no rule, and the self-check failed)
     check   when the net ran: the share of its hidden examples it guessed, in thousandths
     est     when the net ran: its two estimates, even when it then said CANNOT
   Each line is flushed at once, so a live reader can send one window and read its answer straight away.

   make      (or: cc -std=c11 -O2 -I../brain/include sensor_run.c ../brain/lib/macos-arm64/libaa.a -o sensor_run) */
#include <inttypes.h>
#include <stdio.h>
#include "aa.h"

static aa_brain brain;

int main(void)
{
    static const char *BY[4] = { "CANNOT", "AM", "SPLITTER", "NET" };
    if (aa_brain_init(&brain) != AA_OK) { fprintf(stderr, "the brain did not start\n"); return 1; }
    char id[64]; int n;
    while (scanf("%63s %d", id, &n) == 2) {
        int64_t x[AA_MAX_EXAMPLES], y[AA_MAX_EXAMPLES], q[AA_MAX_QUESTIONS] = { 0, 0 };
        int nq = 0, ok = n >= 1 && n <= AA_MAX_EXAMPLES;
        for (int i = 0; ok && i < n; i++) ok = scanf("%" SCNd64, &x[i]) == 1;
        for (int i = 0; ok && i < n; i++) ok = scanf("%" SCNd64, &y[i]) == 1;
        ok = ok && scanf("%d", &nq) == 1 && nq >= 1 && nq <= AA_MAX_QUESTIONS;
        for (int j = 0; ok && j < nq; j++) ok = scanf("%" SCNd64, &q[j]) == 1;
        if (!ok) { printf("%s error=input\n", id); fflush(stdout); return 2; }
        aa_answer a;
        int32_t rc = aa_solve_rule(&brain, x, y, n, q, nq, &a);
        if (rc != AA_OK) { printf("%s error=%d\n", id, (int)rc); fflush(stdout); continue; }
        printf("%s by=%s", id, (a.by >= 0 && a.by <= 3) ? BY[a.by] : "?");
        if (a.by == AA_CANNOT) printf(" ans=CANNOT");
        else printf(" ans=%" PRId64 ",%" PRId64, a.value[0], a.value[1]);
        if (a.learned) printf(" check=%d est=%" PRId64 ",%" PRId64, (int)(a.self_check * 1000.0f + 0.5f), a.estimate[0], a.estimate[1]);
        printf("\n");
        fflush(stdout);
    }
    return 0;
}
