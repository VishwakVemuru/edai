/* The open-source challenger (challengers/beat, portable build) on the same line format as native_solve.
   Output per line: rc by v0 v1 -1 -1, with by mapped to edai's codes: 0 CANNOT, 1 one exact formula, 2 formula in
   pieces, 3 estimate (beat's TREND), sequences report 1. */
#include <stdio.h>
#include <inttypes.h>
#include "beat.h"
int bt_solve(const int64_t *x, const int64_t *y, int n, const int64_t *q, int nq, bt_answer *out);
int main(void) {
    char kind[4];
    while (scanf("%3s", kind) == 1) {
        int n, nq = 0; int64_t x[64], y[64], q[8]; bt_answer a; int rc;
        if (scanf("%d", &n) != 1) return 3;
        if (kind[0] == 'R') {
            for (int i = 0; i < n; i++) scanf("%" SCNd64, &x[i]);
            for (int i = 0; i < n; i++) scanf("%" SCNd64, &y[i]);
            scanf("%d", &nq); for (int i = 0; i < nq; i++) scanf("%" SCNd64, &q[i]);
            rc = bt_solve(x, y, n, q, nq, &a);
        } else { for (int i = 0; i < n; i++) scanf("%" SCNd64, &x[i]); rc = bt_solve_sequence(x, n, &a); }
        int by = a.by == BT_CANNOT ? 0 : a.by == BT_RULE || a.by == BT_SEQ ? 1 : a.by == BT_PIECES ? 2 : 3;
        printf("%d %d %" PRId64 " %" PRId64 " -1 -1\n", rc, rc ? -1 : by, a.value[0], a.value[1]);
        fflush(stdout);
    }
    return 0;
}
