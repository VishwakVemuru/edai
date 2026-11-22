/* answer.c -- the brain through its public API, one problem per line on standard input:
     <id> <n> x1 .. xn y1 .. yn <nq> q1 [q2]
   prints  <id> <AM | SPLITTER | NET | CANNOT> <a,b>   (0,0 for CANNOT) */
#include <inttypes.h>
#include <stdio.h>
#include "aa.h"

int main(void)
{
    static aa_brain brain;
    static const char *NAME[4] = { "CANNOT", "AM", "SPLITTER", "NET" };
    if (aa_brain_init(&brain) != AA_OK) { fprintf(stderr, "the header and the library do not match\n"); return 2; }
    char id[32]; int n, nq;
    while (scanf("%31s %d", id, &n) == 2) {
        int64_t x[AA_MAX_EXAMPLES], y[AA_MAX_EXAMPLES], q[AA_MAX_QUESTIONS] = { 0, 0 };
        if (n < 1 || n > AA_MAX_EXAMPLES) return 2;
        for (int i = 0; i < n; i++) if (scanf("%" SCNd64, &x[i]) != 1) return 2;
        for (int i = 0; i < n; i++) if (scanf("%" SCNd64, &y[i]) != 1) return 2;
        if (scanf("%d", &nq) != 1 || nq < 1 || nq > AA_MAX_QUESTIONS) return 2;
        for (int k = 0; k < nq; k++) if (scanf("%" SCNd64, &q[k]) != 1) return 2;
        aa_answer a;
        if (aa_solve_rule(&brain, x, y, n, q, nq, &a) != AA_OK) return 2;
        printf("%s %s %" PRId64 ",%" PRId64 "\n", id, NAME[a.by], a.value[0], a.value[1]);
    }
    return 0;
}
