#include "harness.h"

int32_t h_solve(int i, aa_brain *brain, aa_answer *a)
{
    const problem_t *p = &PROBLEMS[i];
    if (p->kind == 0) return aa_solve_rule(brain, p->x, p->y, p->n, p->q, 2, a);
    return aa_solve_sequence(brain, p->y, p->n, a);
}

int h_right(int i, const aa_answer *a)
{
    const problem_t *p = &PROBLEMS[i];
    if (p->family == 2) return a->by == AA_CANNOT;
    if (a->by == AA_CANNOT) return 0;
    if (p->family == 0) return (double)a->value[0] == p->truth[0] && (double)a->value[1] == p->truth[1];
    for (int k = 0; k < 2; k++) {
        double d = (double)a->value[k] - p->truth[k];
        if (d < 0) d = -d;
        if (d > p->tol) return 0;
    }
    return 1;
}

char *h_i64(char *p, int64_t v)
{
    char tmp[24]; int n = 0;
    uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
    do { tmp[n++] = (char)('0' + (int)(u % 10)); u /= 10; } while (u);
    if (v < 0) *p++ = '-';
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

static char *put(char *p, const char *s) { while (*s) *p++ = *s++; *p = 0; return p; }

void h_line(char *buf, int i, int32_t rc, const aa_answer *a, int right)
{
    static const char *FAM[3] = { "rule ", "trend", "noise" };
    static const char *BY[5] = { "CANNOT", "AM", "SPLITTER", "NET", "AM_EST" };
    char *p = buf;
    *p++ = 'p'; *p++ = (char)('0' + i / 100); *p++ = (char)('0' + i / 10 % 10); *p++ = (char)('0' + i % 10); *p++ = ' ';
    p = put(p, FAM[PROBLEMS[i].family]);
    if (rc != AA_OK) { p = put(p, " error="); p = h_i64(p, rc); return; }
    p = put(p, " by="); p = put(p, (a->by >= 0 && a->by <= 4) ? BY[a->by] : "?");
    p = put(p, " ans=");
    if (a->by == AA_CANNOT) p = put(p, "CANNOT");
    else { p = h_i64(p, a->value[0]); *p++ = ','; p = h_i64(p, a->value[1]); }
    if (a->learned) { p = put(p, " check="); p = h_i64(p, (int64_t)(a->self_check * 1000.0f + 0.5f)); }   /* in thousandths */
    p = put(p, right ? " right=1" : " right=0");
}
