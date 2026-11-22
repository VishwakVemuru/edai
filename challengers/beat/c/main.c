/* Runs the 300 problems through beat.c and prints one line per problem, in the form score.py reads.
   ./beat [first [count]] */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "beat.h"
#include "problems.h"
int bt_solve(const int64_t *x, const int64_t *y, int n, const int64_t *q, int nq, bt_answer *out);

static void cents(char *b, int64_t v) { snprintf(b, 32, "%s%lld.%02lld", v < 0 ? "-" : "", (long long)(llabs(v) / 100), (long long)(llabs(v) % 100)); }

int main(int argc, char **argv)
{
    int first = argc > 1 ? atoi(argv[1]) : 0, count = argc > 2 ? atoi(argv[2]) : BT_NPROBLEMS;
    static const char *BY[] = {"CANNOT", "RULE", "PIECES", "SEQ", "TREND"};
    uint32_t maxq = 0;
    for (int i = first; i < first + count && i < BT_NPROBLEMS; i++) {
        const bt_problem *p = &BT_PROBLEMS[i];
        int64_t x[16], y[16], q[2]; bt_answer a;
        for (int j = 0; j < p->n; j++) { x[j] = p->x[j]; y[j] = p->y[j]; }
        q[0] = p->q[0]; q[1] = p->q[1];
        clock_t t0 = clock();
        int rc = p->seq ? bt_solve_sequence(x, p->n, &a) : bt_solve(x, y, p->n, q, p->nq, &a);
        double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
        if (!p->seq && (bt_last_queries() & 0x7fffffff) > maxq) maxq = bt_last_queries() & 0x7fffffff;
        char s0[32], s1[32];
        if (rc) printf("%s ERROR\n", p->id);
        else if (a.by == BT_CANNOT) printf("%s CANNOT  # %s bits=%d ms=%.0f q=%u\n", p->id, BY[a.by], a.bits, ms, bt_last_queries());
        else if (a.by == BT_TREND) { cents(s0, a.value[0]); cents(s1, a.value[1]); printf("%s %s,%s  # %s s=%d ms=%.0f\n", p->id, s0, s1, BY[a.by], a.bits, ms); }
        else printf("%s %lld,%lld  # %s bits=%d ms=%.0f q=%u\n", p->id, (long long)a.value[0], (long long)a.value[1], BY[a.by], a.bits, ms, bt_last_queries());
        fflush(stdout);
    }
    fprintf(stderr, "static RAM %u bytes, most pieces in one problem %u\n", bt_ram_bytes(), maxq);
    return 0;
}
