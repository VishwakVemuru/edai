/* Runs problems through the native edai brain (macOS arm64 libaa.a). One problem per input line:
     R n x1..xn y1..yn nq q1..qnq      a rule (x -> y)
     S n t1..tn                        a sequence
   One output line per problem:  rc by v0 v1 learned check_thousandths
   A fresh brain per problem unless REUSE=1 in the environment. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "aa.h"
#include <time.h>
int main(void) {
    static aa_brain brain; aa_answer a; char kind[4];
    int reuse = getenv("REUSE") && atoi(getenv("REUSE"));
    if (aa_brain_init(&brain) != AA_OK) { fprintf(stderr, "init failed\n"); return 2; }
    while (scanf("%3s", kind) == 1) {
        int n, nq = 0; int64_t x[64], y[64], q[8]; int32_t rc; double ms = 0;
        if (scanf("%d", &n) != 1 || n < 0 || n > 64) return 3;
        if (kind[0] == 'R') {
            for (int i = 0; i < n; i++) scanf("%" SCNd64, &x[i]);
            for (int i = 0; i < n; i++) scanf("%" SCNd64, &y[i]);
            scanf("%d", &nq); if (nq < 0 || nq > 8) return 3;
            for (int i = 0; i < nq; i++) scanf("%" SCNd64, &q[i]);
            if (!reuse) aa_brain_init(&brain);
            memset(&a, 0, sizeof a);
            { struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0); rc = aa_solve_rule(&brain, x, y, n, q, nq, &a); clock_gettime(CLOCK_MONOTONIC, &t1); ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6; }
        } else {
            for (int i = 0; i < n; i++) scanf("%" SCNd64, &x[i]);
            if (!reuse) aa_brain_init(&brain);
            memset(&a, 0, sizeof a);
            { struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0); rc = aa_solve_sequence(&brain, x, n, &a); clock_gettime(CLOCK_MONOTONIC, &t1); ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6; }
        }
        printf("%.2f %d %d %" PRId64 " %" PRId64 " %d %d\n", ms, rc, a.by, a.value[0], a.value[1], a.learned,
               a.learned ? (int)(a.self_check * 1000.0f + 0.5f) : -1);
        fflush(stdout);
    }
    return 0;
}
