/* beat.c -- see beat.h. Portable C99 plus the GCC/Clang builtins __builtin_*_overflow and __builtin_ctz. */
#include "beat.h"
#pragma STDC FP_CONTRACT OFF

typedef int64_t i64;
typedef uint64_t u64;

#define MAXP   (BT_MAX_EXAMPLES + BT_MAX_QUESTIONS)
#define LIMIT  1000000000000LL           /* a formula whose value passes 1e12 anywhere is dropped */
#define ONE    65536                     /* one bit, in the Q16 fixed point every length is kept in */
#ifndef BT_MAXQ
#define BT_MAXQ 1024                     /* the most pieces (leaves) one rule problem may examine */
#endif

/* ================================================================ fixed-point lengths */

/* floor(log2(n) * 65536) for n >= 1, by integer squaring: the same on every machine */
static int32_t lg(u64 n)
{
    int ip = 0;
    while (ip < 63 && (n >> (ip + 1)) != 0) ip++;
    u64 m = ip >= 30 ? n >> (ip - 30) : n << (30 - ip);          /* Q30, in [1, 2) */
    int32_t f = 0;
    for (int i = 0; i < 16; i++) {
        m = (m * m) >> 30; f <<= 1;
        if (m >= (1ULL << 31)) { m >>= 1; f |= 1; }
    }
    return ip * ONE + f;
}

static u64 uabs(i64 v) { return v < 0 ? (u64)0 - (u64)v : (u64)v; }

/* length of a rational p/q (q > 0): 1 + 2 log2(|p| + 1) + 2 log2(q) bits */
static int32_t numbits(i64 p, i64 q) { return ONE + 2 * lg(uabs(p) + 1) + 2 * lg((u64)q); }

/* how many formulas of cost <= c there are (a fixed table: the median over the problems) */
static const u64 FTABLE[7] = {1, 41, 554, 1842, 17512, 61017, 85089};
static int32_t fbits[8];

/* the largest formula cost a piece may use, given how many examples it has beyond its coefficients */
static int budget(int checks) { return checks < 1 ? -1 : checks == 1 ? 2 : checks == 2 ? 4 : 99; }

static u64 gcd(u64 a, u64 b) { while (b) { u64 t = a % b; a = b; b = t; } return a; }

/* ================================================================ the formula library */

enum { U_ABS, U_SQ, U_CUBE, U_DIGSUM, U_REV, U_POPC, U_NDIG, U_SIGN, U_NEG,
       U_MOD, U_CMOD, U_FDIV, U_CDIV, U_AND, U_OR, U_XOR, U_SHR, U_SHL };
typedef struct { uint8_t kind, cost, inner, wide; int16_t k; } uop;
#define NUOP 300
#define NL1 160
#ifndef BT_SQLIN
#define BT_SQLIN 0
#endif
#ifndef BT_SQOP
#define BT_SQOP 0
#endif
#ifndef BT_WIDE
#define BT_WIDE 1
#endif
static uop UOPS[NUOP];
static int nuop;

static void add_uop(int kind, int cost, int k)
{
    uop u; u.kind = (uint8_t)kind; u.cost = (uint8_t)cost; u.k = (int16_t)k;
    u.wide = 0;
    u.inner = !(kind == U_DIGSUM || kind == U_REV || kind == U_POPC || kind == U_NDIG || kind == U_SIGN ||
                kind == U_NEG || kind == U_SHL);
    UOPS[nuop++] = u;
}

static void init_uops(void)
{
    static const int ks[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16, 20, 25, 32, 50, 64, 100};
    static const int kb[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 31, 63, 127, 255};
    if (nuop) return;
    add_uop(U_ABS, 1, 0); add_uop(U_SQ, 1, 0); add_uop(U_CUBE, 2, 0); add_uop(U_DIGSUM, 2, 0);
    add_uop(U_REV, 2, 0); add_uop(U_POPC, 2, 0); add_uop(U_NDIG, 2, 0); add_uop(U_SIGN, 1, 0); add_uop(U_NEG, 1, 0);
    for (int i = 0; i < 18; i++) {
        add_uop(U_MOD, 1, ks[i]); add_uop(U_CMOD, 2, ks[i]); add_uop(U_FDIV, 1, ks[i]); add_uop(U_CDIV, 2, ks[i]);
    }
    for (int i = 0; i < 19; i++) { add_uop(U_AND, 2, kb[i]); add_uop(U_OR, 2, kb[i]); add_uop(U_XOR, 2, kb[i]); }
    for (int k = 1; k <= 4; k++) { add_uop(U_SHR, 2, k); add_uop(U_SHL, 2, k); }
    /* bit masks with any constant 16..63 (cost 3): used on x, on a*x+b and on one-step formulas, but not built on */
    if (BT_WIDE) for (int k = 16; k <= 63; k++) {
        if (k == 31 || k == 63) continue;
        for (int op = U_AND; op <= U_XOR; op++) { add_uop(op, 3, k); UOPS[nuop - 1].wide = 1; }
    }
    for (int c = 0; c < 8; c++) fbits[c] = lg(FTABLE[c < 6 ? c : 6]);
}

static i64 fdiv(i64 a, i64 k) { i64 q = a / k; if (a % k != 0 && a < 0) q--; return q; }
static i64 fmodp(i64 a, i64 k) { i64 r = a % k; return r < 0 ? r + k : r; }

/* one unary operation on one value; returns 0 when the result does not fit */
static int apply_u(const uop *u, i64 v, i64 *out)
{
    i64 r, t; u64 a;
    switch (u->kind) {
    case U_ABS: r = v < 0 ? -v : v; break;
    case U_SQ: if (__builtin_mul_overflow(v, v, &r)) return 0; break;
    case U_CUBE: if (__builtin_mul_overflow(v, v, &t) || __builtin_mul_overflow(t, v, &r)) return 0; break;
    case U_DIGSUM: a = uabs(v); r = 0; do { r += (i64)(a % 10); a /= 10; } while (a); break;
    case U_REV: a = uabs(v); r = 0;
        do { if (__builtin_mul_overflow(r, (i64)10, &r) || __builtin_add_overflow(r, (i64)(a % 10), &r)) return 0; a /= 10; } while (a);
        if (v < 0) r = -r; break;
    case U_POPC: { uint32_t w = (uint32_t)(u64)v; r = 0; while (w) { r += w & 1u; w >>= 1; } } break;
    case U_NDIG: a = uabs(v); r = 0; do { r++; a /= 10; } while (a); break;
    case U_SIGN: r = (v > 0) - (v < 0); break;
    case U_NEG: r = -v; break;
    case U_MOD: if (v == (int32_t)v) { int32_t w = (int32_t)v % u->k; r = w < 0 ? w + u->k : w; } else r = fmodp(v, u->k); break;
    case U_CMOD: r = v == (int32_t)v ? (i64)((int32_t)v % u->k) : v % u->k; break;
    case U_FDIV: if (v == (int32_t)v) { int32_t w = (int32_t)v / u->k; if ((int32_t)v % u->k != 0 && v < 0) w--; r = w; } else r = fdiv(v, u->k); break;
    case U_CDIV: r = v == (int32_t)v ? (i64)((int32_t)v / u->k) : v / u->k; break;
    case U_AND: r = (i64)((u64)v & (u64)u->k); break;
    case U_OR: r = (i64)((u64)v | (u64)u->k); break;
    case U_XOR: r = (i64)((u64)v ^ (u64)u->k); break;
    case U_SHR: r = v >= 0 ? (i64)((u64)v >> u->k) : -(i64)(((u64)(-(v + 1))) >> u->k) - 1; break;
    case U_SHL: if (__builtin_mul_overflow(v, (i64)1 << u->k, &r)) return 0; break;
    default: return 0;
    }
    *out = r; return 1;
}

enum { B_ADD, B_SUB, B_MUL, B_MIN, B_MAX, B_AND, B_OR, B_XOR, NBOP };
static int apply_b(int op, i64 a, i64 b, i64 *out)
{
    switch (op) {
    case B_ADD: return !__builtin_add_overflow(a, b, out);
    case B_SUB: return !__builtin_sub_overflow(a, b, out);
    case B_MUL: return !__builtin_mul_overflow(a, b, out);
    case B_MIN: *out = a < b ? a : b; return 1;
    case B_MAX: *out = a > b ? a : b; return 1;
    case B_AND: *out = (i64)((u64)a & (u64)b); return 1;
    case B_OR: *out = (i64)((u64)a | (u64)b); return 1;
    default: *out = (i64)((u64)a ^ (u64)b); return 1;
    }
}

/* ================================================================ the problem being solved */

static int NP, NE, NQ;                  /* points = examples then questions */
static i64 X[MAXP], Y[BT_MAX_EXAMPLES];
static i64 R[BT_MAX_EXAMPLES];           /* sqrt(y) when y is a perfect square, else -1 */
#ifndef BT_PEEL
#define BT_PEEL 1
#endif
#define PEEL 0x80                        /* maxcost flag: the piece's outputs are squares of a formula */
static i64 L1[NL1][MAXP];               /* the one-step formulas of x, without duplicates */
static uint8_t L1cost[NL1];
static int nl1;

static int fits(const i64 *v) { for (int i = 0; i < NP; i++) if (v[i] > LIMIT || v[i] < -LIMIT) return 0; return 1; }
static int eqv(const i64 *a, const i64 *b) { for (int i = 0; i < NP; i++) if (a[i] != b[i]) return 0; return 1; }

static int vec_u(const uop *u, const i64 *src, i64 *dst)
{
    for (int i = 0; i < NP; i++) if (!apply_u(u, src[i], &dst[i])) return 0;
    return fits(dst);
}

static void build_l1(void)
{
    i64 v[MAXP];
    nl1 = 0;
    for (int o = 0; o < nuop; o++) {
        if (UOPS[o].wide || !vec_u(&UOPS[o], X, v)) continue;
        if (eqv(v, X)) continue;
        int dup = -1;
        for (int j = 0; j < nl1; j++) if (eqv(v, L1[j])) { dup = j; break; }
        if (dup >= 0) { if (UOPS[o].cost < L1cost[dup]) L1cost[dup] = UOPS[o].cost; continue; }
        for (int i = 0; i < NP; i++) L1[nl1][i] = v[i];
        L1cost[nl1++] = UOPS[o].cost;
    }
}

/* ================================================================ pieces (leaves) and the search over formulas */

typedef struct {
    uint32_t emask;                     /* which examples */
    uint8_t qmask, maxcost, k, lim;     /* which questions; formula cost cap; examples; usable cost */
    int32_t c;                          /* best length so far (Q16) */
    int8_t fc, found, small, pad;
    uint8_t pts[BT_MAX_EXAMPLES];       /* the examples, in order */
    i64 pred[2];
} query;

static query Q[BT_MAXQ];
static int nq_, qover;
static uint16_t HT[2 * BT_MAXQ];        /* hash table of query indices + 1 */
static uint32_t last_queries;

static uint32_t qhash(uint32_t e, int qm, int mc) { uint32_t h = e * 2654435761u ^ (uint32_t)(qm * 97 + mc * 7919); return (h ^ (h >> 15)) & (2 * BT_MAXQ - 1); }

static query *qfind(uint32_t e, int qm, int mc, int create)
{
    uint32_t h = qhash(e, qm, mc);
    while (HT[h]) {
        query *q = &Q[HT[h] - 1];
        if (q->emask == e && q->qmask == qm && q->maxcost == mc) return q;
        h = (h + 1) & (2 * BT_MAXQ - 1);
    }
    if (!create) return 0;
    if (nq_ >= BT_MAXQ) { qover = 1; return 0; }
    query *q = &Q[nq_]; HT[h] = (uint16_t)(++nq_);
    q->emask = e; q->qmask = (uint8_t)qm; q->maxcost = (uint8_t)mc;
    int k = __builtin_popcount(e); q->k = (uint8_t)k;
    int lim = budget(k); if (mc < lim) lim = mc; q->lim = (uint8_t)(lim < 0 ? 255 : lim);
    q->found = 0; q->fc = 127; q->c = 0x7fffffff;
    /* a constant piece */
    int first = __builtin_ctz(e); int same = 1;
    for (uint32_t m = e; m; m &= m - 1) if (Y[__builtin_ctz(m)] != Y[first]) same = 0;
    if (same && k >= 2) { q->found = 1; q->fc = -1; q->c = ONE + numbits(Y[first], 1); q->pred[0] = q->pred[1] = Y[first]; }
    if (lim < 0 || k < 2) q->lim = 255;
    if (mc & PEEL) {                    /* y = F^2: every output a square, and not all 0 or 1 */
        q->found = 0; q->fc = 127; q->c = 0x7fffffff;
        int big = 0, okp = 1, n4 = 0;
        for (uint32_t m = e; m; m &= m - 1) { i64 r = R[__builtin_ctz(m)]; if (r < 0) okp = 0; if (r >= 4) n4++; if (r >= 10) big = 1; }
        if (n4 < 2) big = 0;
        lim = budget(k); if ((mc & ~PEEL) < lim) lim = mc & ~PEEL;
        q->lim = (uint8_t)((okp && big && k >= 2 && lim >= 1) ? lim - 1 : 255);
    }
    i64 lo = Y[first], hi = Y[first];
    for (uint32_t m = e; m; m &= m - 1) { i64 y = Y[__builtin_ctz(m)]; if (y < lo) lo = y; if (y > hi) hi = y; }
    q->small = (lo > -(1LL << 21) && hi < (1LL << 21));        /* cross products then fit in 64 bits */
    int np = 0; for (uint32_t m = e; m; m &= m - 1) q->pts[np++] = (uint8_t)__builtin_ctz(m);
    return q;
}

/* does y = a F + b hold exactly on this piece's examples, and is it this piece's shortest formula so far? */
#ifdef BT_COUNT
unsigned long long bt_ntest, bt_nlive, bt_nvisit, bt_nfit;
#define CNT(x) (x)++
#else
#define CNT(x)
#endif
static void test_peel(query *q, const i64 *F, int fc)
{
    for (int s = 0; s < q->k; s++) { int j = q->pts[s]; if ((F[j] < 0 ? -F[j] : F[j]) != R[j]) return; }
    int32_t c = fbits[fc + 1 < 7 ? fc + 1 : 7] + ONE;
    if (q->found && !(c < q->c || (c == q->c && fc < q->fc))) return;
    i64 pr[2] = {0, 0};
    for (int s = 0; s < NQ; s++) if (q->qmask >> s & 1) { if (__builtin_mul_overflow(F[NE + s], F[NE + s], &pr[s])) return; }
    q->found = 1; q->c = c; q->fc = (int8_t)fc; q->pred[0] = pr[0]; q->pred[1] = pr[1];
}

static void test(query *q, const i64 *F, int fc)
{
    CNT(bt_nlive);
    const uint8_t *pt = q->pts;
    int k = q->k;
    int i0 = pt[0];
    i64 F0 = F[i0], y0 = Y[i0], dF = 0, dy = 0;
    if (k == 2 && fc > 2 && (F0 != y0 || F[pt[1]] != Y[pt[1]])) return;   /* two examples: only y = F itself may cost more than 2 */
    int have = 0;
    for (int s = 1; s < k; s++) {
        int j = pt[s];
        if (!have) {
            if (F[j] == F0) { if (Y[j] != y0) return; }
            else { dF = F[j] - F0; dy = Y[j] - y0; have = 1; }
        } else if (q->small) {
            if ((Y[j] - y0) * dF != dy * (F[j] - F0)) return;
        } else {
            i64 l, r;
            if (__builtin_mul_overflow(Y[j] - y0, dF, &l) || __builtin_mul_overflow(dy, F[j] - F0, &r) || l != r) return;
        }
    }
    if (!have) return;
    CNT(bt_nfit);
    u64 g = gcd(uabs(dy), uabs(dF));
    i64 p = dy / (i64)g, d = dF / (i64)g;
    if (d < 0) { p = -p; d = -d; }
    if (d > 12) return;
    i64 B, t;                                    /* b = B / d */
    if (__builtin_mul_overflow(p, F0, &t) || __builtin_mul_overflow(d, y0, &B) || __builtin_sub_overflow(B, t, &B)) return;
    int npar = !(p == 1 && d == 1) + (B != 0);
    if (fc > budget(q->k - npar)) return;
    int32_t c = fbits[fc] + ONE;
    if (!(p == 1 && d == 1)) c += numbits(p, d);
    if (B != 0) { i64 gb = (i64)gcd(uabs(B), (u64)d); c += numbits(B / gb, d / gb); }
    if (q->found && !(c < q->c || (c == q->c && fc < q->fc))) return;
    i64 pr[2] = {0, 0};
    for (int s = 0; s < NQ; s++) {
        if (!(q->qmask >> s & 1)) continue;
        i64 num;
        if (__builtin_mul_overflow(p, F[NE + s], &num) || __builtin_add_overflow(num, B, &num) || num % d) return;
        pr[s] = num / d;
    }
    q->found = 1; q->c = c; q->fc = (int8_t)fc; q->pred[0] = pr[0]; q->pred[1] = pr[1];
}


/* Formulas that give the same values as one already tested at no higher cost are skipped: the result cannot
   change. A fixed table of 64-bit fingerprints, overwritten on collision, so it only ever skips true repeats. */
#ifndef BT_SEEN
#define BT_SEEN 4096
#endif
static u64 SEEN[BT_SEEN];
static void seen_clear(void) { for (int i = 0; i < BT_SEEN; i++) SEEN[i] = 0; }
static int seen_before(const i64 *v, int cost)
{
    u64 h = 1469598103934665603ULL;
    for (int i = 0; i < NP; i++) { h ^= (u64)v[i]; h *= 1099511628211ULL; h ^= h >> 29; }
    h |= 1;                                              /* never 0, the empty mark */
    u64 key = h & ~(u64)0xFF, *slot = &SEEN[(h >> 8) % BT_SEEN];
    if ((*slot & ~(u64)0xFF) == key && (int)(*slot & 0xFF) <= cost) return 1;
    *slot = key | (u64)cost;
    return 0;
}

/* Pieces are grouped by their first three examples (two, for a two-example piece): a formula that is not affine on
   those cannot fit any piece of the group, and nearly every formula fails right there. */
typedef struct { uint8_t p[3], k, small; int16_t head; } group;
static group GR[4][BT_MAXQ];
static int ngr[4];
static int16_t QNEXT[BT_MAXQ];

static void group_add(int which, int qi)
{
    query *q = &Q[qi];
    int k = q->k < 3 ? q->k : 3;
    for (int g = 0; g < ngr[which]; g++) {
        group *G = &GR[which][g];
        if (G->k == k && G->p[0] == q->pts[0] && G->p[1] == q->pts[1] && (k < 3 || G->p[2] == q->pts[2])) {
            QNEXT[qi] = G->head; G->head = (int16_t)qi; return;
        }
    }
    group *G = &GR[which][ngr[which]++];
    G->k = (uint8_t)k; G->p[0] = q->pts[0]; G->p[1] = q->pts[1]; G->p[2] = k == 3 ? q->pts[2] : 0;
    G->small = 1;
    for (int s = 0; s < k; s++) if (Y[G->p[s]] <= -(1LL << 21) || Y[G->p[s]] >= (1LL << 21)) G->small = 0;
    if (which >= 2) G->k = (uint8_t)k;
    QNEXT[qi] = -1; G->head = (int16_t)qi;
}

static int group_ok(const group *G, const i64 *F)
{
    int a = G->p[0], b = G->p[1];
    i64 F0 = F[a], y0 = Y[a];
    if (G->k == 2) return F[b] != F0;                    /* equal values: no slope (or a contradiction) */
    int c = G->p[2];
    if (F[b] == F0) {
        if (Y[b] != y0) return 0;
        return F[c] != F0 || Y[c] == y0;
    }
    i64 dF = F[b] - F0, dy = Y[b] - y0;
    if (G->small) return (Y[c] - y0) * dF == dy * (F[c] - F0);
    i64 l, r;
    return !(__builtin_mul_overflow(Y[c] - y0, dF, &l) || __builtin_mul_overflow(dy, F[c] - F0, &r)) && l == r;
}

static int group_ok_peel(const group *G, const i64 *F)
{
    for (int s = 0; s < G->k; s++) { int j = G->p[s]; if ((F[j] < 0 ? -F[j] : F[j]) != R[j]) return 0; }
    return 1;
}

static void visit(const i64 *v, int cost)
{
    CNT(bt_nvisit);
    if (seen_before(v, cost)) return;
    for (int w = 0; w < (cost <= 4 ? 2 : 1); w++)
        for (int g = 0; g < ngr[w]; g++) {
            if (!group_ok(&GR[w][g], v)) continue;
            for (int qi = GR[w][g].head; qi >= 0; qi = QNEXT[qi]) test(&Q[qi], v, cost);
        }
    for (int w = 2; w < (cost <= 3 ? 4 : 3); w++)
        for (int g = 0; g < ngr[w]; g++) {
            if (!group_ok_peel(&GR[w][g], v)) continue;
            for (int qi = GR[w][g].head; qi >= 0; qi = QNEXT[qi]) if (cost <= Q[qi].lim) test_peel(&Q[qi], v, cost);
        }
}

/* every formula in the library, in a fixed order, at most maxcost */
static void enumerate(int maxcost)
{
    static const int AL[14] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, -1, -2, -3};
    i64 v[MAXP], lin[MAXP];
    visit(X, 0);
    for (int o = 0; o < nuop; o++) if (UOPS[o].cost <= maxcost && vec_u(&UOPS[o], X, v)) visit(v, UOPS[o].cost);
    if (maxcost >= 2) for (int k = -20; k <= 20; k++) {
        for (int i = 0; i < NP; i++) v[i] = X[i] > k ? X[i] : k;
        visit(v, 2);
        for (int i = 0; i < NP; i++) v[i] = X[i] < k ? X[i] : k;
        visit(v, 2);
    }
    for (int ai = 0; ai < 14; ai++) for (int b = -12; b <= 12; b++) {
        for (int i = 0; i < NP; i++) lin[i] = AL[ai] * X[i] + b;
        if (b && maxcost >= 2) visit(lin, 2);
        for (int o = 0; o < nuop; o++) {
            int c = UOPS[o].cost + 2 + (b != 0);
            if (!UOPS[o].inner || c > maxcost) continue;
            if (vec_u(&UOPS[o], lin, v)) visit(v, c);
        }
    }
    for (int j = 0; j < nl1; j++) for (int o = 0; o < nuop; o++) {
        int c = L1cost[j] + UOPS[o].cost + 1;
        if (c <= maxcost && vec_u(&UOPS[o], L1[j], v)) visit(v, c);
    }
    for (int i = -1; i < nl1; i++) {
        const i64 *a = i < 0 ? X : L1[i]; int ca = i < 0 ? 0 : L1cost[i];
        if (ca > 2) continue;
        for (int j = i; j < nl1; j++) {
            const i64 *b = j < 0 ? X : L1[j]; int cb = j < 0 ? 0 : L1cost[j];
            if (cb > 2 || ca + cb + 2 > maxcost) continue;
            for (int op = 0; op < NBOP; op++) {
                int ok = 1;
                for (int p = 0; p < NP && ok; p++) ok = apply_b(op, a[p], b[p], &v[p]);
                if (ok && fits(v)) visit(v, ca + cb + 2);
            }
        }
    }
#if BT_SQLIN
    /* an operation on a*x^2 + b or a*|x| + b, such as (11 x^2) // 9 */
    for (int base = 0; base < 2; base++) {
        int cf = 1;
        i64 F[MAXP]; for (int i = 0; i < NP; i++) F[i] = base == 0 ? X[i] * X[i] : (X[i] < 0 ? -X[i] : X[i]);
        for (int ai = 0; ai < 14; ai++) for (int b = -12; b <= 12; b++) {
            for (int i = 0; i < NP; i++) lin[i] = AL[ai] * F[i] + b;
            if (b && cf + 2 <= maxcost) visit(lin, cf + 2);
            for (int o = 0; o < nuop; o++) {
                int c = cf + UOPS[o].cost + 2 + (b != 0);
                if (!UOPS[o].inner || c > maxcost) continue;
                if (vec_u(&UOPS[o], lin, v)) visit(v, c);
            }
        }
    }
#endif
#if BT_SQOP
    /* the square of an operation on a*x + b, such as ((5x) | 2)^2 */
    for (int ai = -1; ai < 14; ai++) for (int b = -12; b <= 12; b++) {
        if (ai < 0 && b) continue;
        for (int i = 0; i < NP; i++) lin[i] = (ai < 0 ? 1 : AL[ai]) * X[i] + b;
        for (int o = 0; o < nuop; o++) {
            int c = UOPS[o].cost + 2 + (b != 0) + 1 + (ai >= 0);
            if (!UOPS[o].inner || UOPS[o].kind == U_SQ || UOPS[o].kind == U_CUBE || UOPS[o].kind == U_ABS || c > maxcost) continue;
            if (!vec_u(&UOPS[o], lin, v)) continue;
            int ok = 1; for (int i = 0; i < NP && ok; i++) ok = !__builtin_mul_overflow(v[i], v[i], &v[i]);
            if (ok && fits(v)) visit(v, c);
        }
    }
#endif
}

/* ================================================================ the search over pieces */

static uint32_t CM[256];                 /* each condition's mask over all points */
static int ncond;
static int32_t LGM;
static int reg;                          /* 1: only collect the pieces needed; 0: use their results */

typedef struct { int ok; int32_t c; i64 pred[2]; } res;

static res leaf(uint32_t e, int qm, int mc)
{
    res r; r.ok = 0; r.c = 0; r.pred[0] = r.pred[1] = 0;
    query *q = qfind(e, qm, mc, reg);
    query *p = BT_PEEL ? qfind(e, qm, mc | PEEL, reg) : 0;
    if (reg) { r.ok = 1; return r; }
    if (q && q->found) { r.ok = 1; r.c = q->c; r.pred[0] = q->pred[0]; r.pred[1] = q->pred[1]; }
    if (p && p->found && (!r.ok || p->c < r.c)) { r.ok = 1; r.c = p->c; r.pred[0] = p->pred[0]; r.pred[1] = p->pred[1]; }
    return r;
}

static res join(res l, res r, int lq, int rq)
{
    res o; o.ok = 1; o.c = l.c + r.c + LGM + ONE;
    for (int s = 0; s < 2; s++) o.pred[s] = (lq >> s & 1) ? l.pred[s] : (rq >> s & 1) ? r.pred[s] : 0;
    return o;
}

#define EMASK(m) ((m) & ((1u << NE) - 1))
#define QMASK(m) ((int)(((m) >> NE) & 3u))

/* a piece of up to two parts with cost-4 formulas (the inner level of the deepest tree) */
static res sub(uint32_t e, int qm)
{
    res best = leaf(e, qm, 4);
    uint32_t seen[256]; int ns = 0;
    for (int ci = 0; ci < ncond; ci++) {
        uint32_t L = e & CM[ci], R = e & ~CM[ci];
        if (__builtin_popcount(L) < 2 || __builtin_popcount(R) < 2) continue;
        int dup = 0; for (int s = 0; s < ns; s++) if (seen[s] == L) { dup = 1; break; }
        if (dup) continue;
        seen[ns++] = L;
        int lq = qm & QMASK(CM[ci]), rq = qm & ~QMASK(CM[ci]);
        res l = leaf(L, lq, 4); if (!l.ok) continue;
        res r = leaf(R, rq, 4); if (!r.ok) continue;
        if (reg) continue;
        res t = join(l, r, lq, rq);
        if (best.ok && t.c >= best.c) continue;
        best = t;
    }
    return best;
}

static res cbest; static int cby;
static void cand(res t, int by) { if (t.ok && (!cbest.ok || t.c < cbest.c)) { cbest = t; cby = by; } }

static void walk(void)
{
    uint32_t all = (1u << NE) - 1; int qa = (1 << NQ) - 1;
    uint32_t seen[256]; int ns = 0;
    for (int ci = 0; ci < ncond; ci++) {                          /* two pieces, any formula each */
        uint32_t L = all & CM[ci], R = all & ~CM[ci];
        if (__builtin_popcount(L) < 2 || __builtin_popcount(R) < 2) continue;
        int dup = 0; for (int s = 0; s < ns; s++) if (seen[s] == L) { dup = 1; break; }
        if (dup) continue;
        seen[ns++] = L;
        int lq = qa & QMASK(CM[ci]), rq = qa & ~QMASK(CM[ci]);
        res l = leaf(L, lq, 99); if (!l.ok) continue;
        res r = leaf(R, rq, 99); if (!r.ok) continue;
        if (!reg) cand(join(l, r, lq, rq), BT_PIECES);
    }
    if (NE < 8) return;
    ns = 0;
    for (int ci = 0; ci < ncond; ci++) {                          /* up to three or four pieces, cheap formulas */
        uint32_t L = all & CM[ci], R = all & ~CM[ci];
        if (__builtin_popcount(L) < 2 || __builtin_popcount(R) < 2) continue;
        int dup = 0; for (int s = 0; s < ns; s++) if (seen[s] == L) { dup = 1; break; }
        if (dup) continue;
        seen[ns++] = L;
        int lq = qa & QMASK(CM[ci]), rq = qa & ~QMASK(CM[ci]);
        res l = sub(L, lq); if (!l.ok) continue;
        res r = sub(R, rq); if (!r.ok) continue;
        if (!reg) cand(join(l, r, lq, rq), BT_PIECES);
    }
}

static void run_queries(int from)
{
    ngr[0] = ngr[1] = ngr[2] = ngr[3] = 0;
    for (int i = from; i < nq_; i++) {
        if (Q[i].lim == 255) continue;
        if (Q[i].maxcost & PEEL) group_add(Q[i].lim > 3 ? 2 : 3, i);
        else group_add(Q[i].lim > 4 ? 0 : 1, i);
    }
    if (ngr[0] + ngr[1] + ngr[2] + ngr[3] == 0) return;
    seen_clear();
    enumerate(ngr[0] + ngr[2] ? 99 : 4);
}

#ifdef BT_DEEP
#include <stdlib.h>
#include "deep.c"
#endif

int bt_solve_rule(const int64_t *x, const int64_t *y, int n, const int64_t *q, int nq, bt_answer *out)
{
    if (!x || !y || !out || n < 2 || n > BT_MAX_EXAMPLES || nq < 1 || nq > BT_MAX_QUESTIONS || !q) return -1;
    init_uops();
    out->by = BT_CANNOT; out->bits = 0; out->value[0] = out->value[1] = 0; out->est[0] = out->est[1] = 0;
    NE = n; NQ = nq; NP = n + nq;
    for (int i = 0; i < n; i++) { X[i] = x[i]; Y[i] = y[i]; }
    for (int s = 0; s < nq; s++) X[n + s] = q[s];
    build_l1();
    nq_ = 0; qover = 0;
    for (int i = 0; i < 2 * BT_MAXQ; i++) HT[i] = 0;

    /* one formula for everything */
    uint32_t all = (1u << n) - 1; int qa = (1 << nq) - 1;
    for (int i = 0; i < n; i++) {
        R[i] = -1;
        if (Y[i] >= 0) { i64 r = 0; while ((r + 1) * (r + 1) <= Y[i] && r < 3037000499LL) { i64 step = 1; while ((r + 2 * step) * (r + 2 * step) <= Y[i] && r + 2 * step < 3037000499LL) step *= 2; r += step; } if (r * r == Y[i]) R[i] = r; }
    }
    query *g = qfind(all, qa, 99, 1);
    query *gp = BT_PEEL ? qfind(all, qa, 99 | PEEL, 1) : 0;
    int nglobal = nq_;
    run_queries(0);
    cbest.ok = 0; cby = BT_CANNOT;
    if (gp && gp->found && (!g->found || gp->c < g->c)) g = gp;
    if (g->found) { res t; t.ok = 1; t.c = g->c; t.pred[0] = g->pred[0]; t.pred[1] = g->pred[1]; cand(t, BT_RULE); }
    if (!(g->found && g->c <= 8 * ONE)) {
        /* conditions: x % m == r, then x below each gap between the examples (at its middle, or at zero) */
        ncond = 0;
        for (int m = 2; m <= 10; m++) for (int r = 0; r < m; r++) {
            uint32_t mk = 0; for (int i = 0; i < NP; i++) if (fmodp(X[i], m) == r) mk |= 1u << i;
            CM[ncond++] = mk;
        }
        i64 sx[BT_MAX_EXAMPLES]; int ns = 0;
        for (int i = 0; i < n; i++) { int d = 0; for (int j = 0; j < ns; j++) if (sx[j] == x[i]) d = 1; if (!d) sx[ns++] = x[i]; }
        for (int i = 1; i < ns; i++) { i64 t = sx[i]; int j = i - 1; while (j >= 0 && sx[j] > t) { sx[j + 1] = sx[j]; j--; } sx[j + 1] = t; }
        for (int i = 0; i + 1 < ns; i++) {
            i64 t2 = (sx[i] < 0 && sx[i + 1] >= 0) ? -1 : sx[i] + sx[i + 1];
            uint32_t mk = 0; for (int p = 0; p < NP; p++) if (2 * X[p] <= t2) mk |= 1u << p;
            CM[ncond++] = mk;
        }
        LGM = lg((u64)ncond);
        reg = 1; walk();
        run_queries(nglobal);
        reg = 0; walk();
    }
    last_queries = (uint32_t)nq_ | (qover ? 0x80000000u : 0);
#ifndef BT_DEEP
    if (!cbest.ok) return 0;
#endif
    i64 lo = y[0], hi = y[0];
    for (int i = 1; i < n; i++) { if (y[i] < lo) lo = y[i]; if (y[i] > hi) hi = y[i]; }
    u64 span = (u64)(hi - lo) + 2; if (span < 32) span = 32;
    int64_t limit = (int64_t)(n - 2) * lg(span);
    if (cby == BT_RULE && cbest.c <= 8 * ONE) limit = cbest.c;          /* a very short formula needs no check */
#ifdef BT_DEEP
#ifndef BT_DEEP_ALWAYS
#define BT_DEEP_ALWAYS 0
#endif
    if (!cbest.ok || cbest.c > limit || (BT_DEEP_ALWAYS && !(cby == BT_RULE && cbest.c <= 8 * ONE))) {   /* the deep search */
        /* targets: everything; each side of x<0, of x odd, and each class of x % 3 */
        uint32_t alle = (1u << n) - 1; int allq = (1 << nq) - 1;
        uint32_t neg = 0, odd = 0, m3[3] = {0, 0, 0};
        for (int p = 0; p < NP; p++) { if (X[p] < 0) neg |= 1u << p; if (fmodp(X[p], 2)) odd |= 1u << p; m3[fmodp(X[p], 3)] |= 1u << p; }
        uint32_t masks[7] = {neg, ~neg, odd, ~odd, m3[0], m3[1], m3[2]};
        int ti[7];
        ndt = 0; dt_add(alle, allq);
        for (int i = 0; i < 7; i++) {
            uint32_t e = masks[i] & alle;
            ti[i] = __builtin_popcount(e) >= 3 && e != alle ? dt_add(e, allq & (int)((masks[i] >> n) & 3u)) : -1;
        }
        deep_run();
        res best = cbest; int bby = cby; int okb = cbest.ok && cbest.c <= limit;
        int64_t lim2 = (int64_t)(n - 2) * lg(span);
        #define OFFER(T, BY) do { res _t = (T); if (_t.ok && _t.c <= lim2 && (!okb || _t.c < best.c)) { best = _t; bby = (BY); okb = 1; } } while (0)
        if (DT[0].best.ok) { res t; t.ok = 1; t.c = DT[0].best.c; t.pred[0] = DT[0].best.pred[0]; t.pred[1] = DT[0].best.pred[1]; OFFER(t, BT_RULE); }
        /* a side: the shorter of its deep chain and its ordinary formula */
        res side[7];
        for (int i = 0; i < 7; i++) {
            uint32_t e = masks[i] & alle; int qm = allq & (int)((masks[i] >> n) & 3u);
            side[i].ok = 0; side[i].c = 0x7fffffff; side[i].pred[0] = side[i].pred[1] = 0;
            if (__builtin_popcount(e) < 2 || e == alle) continue;
            reg = 0; res l = leaf(e, qm, 99);
            if (l.ok) side[i] = l;
            if (ti[i] >= 0 && DT[ti[i]].best.ok && DT[ti[i]].best.c < side[i].c) {
                side[i].ok = 1; side[i].c = DT[ti[i]].best.c; side[i].pred[0] = DT[ti[i]].best.pred[0]; side[i].pred[1] = DT[ti[i]].best.pred[1];
            }
        }
        for (int i = 0; i < 4; i += 2) if (side[i].ok && side[i + 1].ok) {
            int lq = allq & (int)((masks[i] >> n) & 3u), rq = allq & (int)((masks[i + 1] >> n) & 3u);
            OFFER(join(side[i], side[i + 1], lq, rq), BT_PIECES);
        }
        if (side[4].ok && side[5].ok && side[6].ok) {
            int q0 = allq & (int)((masks[4] >> n) & 3u), q1 = allq & (int)((masks[5] >> n) & 3u), q2 = allq & (int)((masks[6] >> n) & 3u);
            res t = join(join(side[4], side[5], q0, q1), side[6], q0 | q1, q2);
            OFFER(t, BT_PIECES);
        }
        #undef OFFER
        if (okb) { cbest = best; cby = bby; limit = best.c; }
    }
#endif
    if (!cbest.ok || cbest.c > limit) return 0;
    out->by = cby; out->bits = cbest.c; out->value[0] = cbest.pred[0]; out->value[1] = cbest.pred[1];
    return 0;
}

uint32_t bt_last_queries(void) { return last_queries; }
uint32_t bt_ram_bytes(void)
{
    return (uint32_t)(sizeof Q + sizeof HT + sizeof SEEN + sizeof GR + sizeof QNEXT + sizeof L1 + sizeof L1cost + sizeof X + sizeof Y + sizeof CM + sizeof UOPS + sizeof fbits);
}

/* ================================================================ estimates (trends) and noise */

static double dsqrt(double x)
{
    if (!(x > 0)) return 0;
    union { double d; u64 u; } v; v.d = x;
    v.u = (v.u >> 1) + 0x1FF8000000000000ULL;                      /* a first guess from the exponent */
    double r = v.d;
    for (int i = 0; i < 6; i++) r = 0.5 * (r + x / r);
    return r;
}

static double dexp(double t)
{
    if (t < -700) return 0;
    if (t > 700) t = 700;
    const double LN2HI = 6.93147180369123816490e-01, LN2LO = 1.90821492927058770002e-10, INVLN2 = 1.44269504088896338700e+00;
    double kf = t * INVLN2;
    int k = (int)(kf < 0 ? kf - 0.5 : kf + 0.5);
    double r = (t - k * LN2HI) - k * LN2LO;                       /* |r| <= 0.35 */
    double s = 1.0;
    for (int i = 16; i >= 1; i--) s = 1.0 + s * r / i;           /* Taylor series, Horner form */
    union { double d; u64 u; } p; p.u = (u64)(k + 1023) << 52;
    return s * p.d;
}

static double mean(const double *a, int n) { double s = 0; for (int i = 0; i < n; i++) s += a[i]; return s / n; }
static double pstd(const double *a, int n)
{
    double m = mean(a, n), s = 0;
    for (int i = 0; i < n; i++) s += (a[i] - m) * (a[i] - m);
    return dsqrt(s / n);
}

/* solve A z = b (size m, row-major, overwritten), partial pivoting; returns 0 if singular */
static int gauss(double *A, double *b, int m)
{
    for (int c = 0; c < m; c++) {
        int p = c; double best = A[c * m + c] < 0 ? -A[c * m + c] : A[c * m + c];
        for (int r = c + 1; r < m; r++) { double v = A[r * m + c] < 0 ? -A[r * m + c] : A[r * m + c]; if (v > best) { best = v; p = r; } }
        if (best == 0) return 0;
        if (p != c) { for (int j = 0; j < m; j++) { double t = A[c * m + j]; A[c * m + j] = A[p * m + j]; A[p * m + j] = t; } double t = b[c]; b[c] = b[p]; b[p] = t; }
        for (int r = c + 1; r < m; r++) {
            double f = A[r * m + c] / A[c * m + c];
            if (f == 0) continue;
            for (int j = c; j < m; j++) A[r * m + j] -= f * A[c * m + j];
            b[r] -= f * b[c];
        }
    }
    for (int c = m - 1; c >= 0; c--) {
        double s = b[c];
        for (int j = c + 1; j < m; j++) s -= A[c * m + j] * b[j];
        b[c] = s / A[c * m + c];
    }
    return 1;
}

#define NMODEL 24
static const double GLS[5] = {15, 25, 40, 60, 100}, GNZ[4] = {0.05, 0.2, 0.5, 1.0};

/* model mi fitted to (x, y) of size n, predicted at the nq points q */
static void model(int mi, const double *x, const double *y, int n, const double *q, int nq, double *out)
{
    if (mi < 4) {                                                  /* polynomial of degree 1..4 in scaled x */
        int d = mi + 1, m = d + 1;
        double mu = mean(x, n), s = pstd(x, n); if (s == 0) s = 1;
        double A[25], b[5], z[BT_MAX_EXAMPLES];
        for (int i = 0; i < n; i++) z[i] = (x[i] - mu) / s;
        for (int r = 0; r < m; r++) {
            b[r] = 0; for (int i = 0; i < n; i++) { double p = 1; for (int e = 0; e < r; e++) p *= z[i]; b[r] += p * y[i]; }
            for (int c = 0; c < m; c++) { double t = 0; for (int i = 0; i < n; i++) { double p = 1; for (int e = 0; e < r + c; e++) p *= z[i]; t += p; } A[r * m + c] = t; }
        }
        if (!gauss(A, b, m)) { for (int j = 0; j < nq; j++) out[j] = mean(y, n); return; }
        for (int j = 0; j < nq; j++) { double zq = (q[j] - mu) / s, v = 0; for (int r = m - 1; r >= 0; r--) v = v * zq + b[r]; out[j] = v; }
        return;
    }
    double ls = GLS[(mi - 4) / 4], nz = GNZ[(mi - 4) % 4];          /* Gaussian process around a straight line */
    double mx = mean(x, n), my = mean(y, n), sxx = 0, sxy = 0;
    for (int i = 0; i < n; i++) { sxx += (x[i] - mx) * (x[i] - mx); sxy += (x[i] - mx) * (y[i] - my); }
    double sl = sxx > 0 ? sxy / sxx : 0, ic = my - sl * mx;
    double r[BT_MAX_EXAMPLES], K[BT_MAX_EXAMPLES * BT_MAX_EXAMPLES];
    for (int i = 0; i < n; i++) r[i] = y[i] - (sl * x[i] + ic);
    double sc = pstd(r, n); if (sc == 0) sc = 1;
    for (int i = 0; i < n; i++) {
        r[i] /= sc;
        for (int j = 0; j < n; j++) { double d = (x[i] - x[j]) / ls; K[i * n + j] = dexp(-0.5 * d * d) + (i == j ? nz : 0); }
    }
    int ok = gauss(K, r, n);
    for (int j = 0; j < nq; j++) {
        double v = 0;
        if (ok) for (int i = 0; i < n; i++) { double d = (q[j] - x[i]) / ls; v += dexp(-0.5 * d * d) * r[i]; }
        out[j] = sl * q[j] + ic + sc * v;
    }
}

/* leave-one-out error of model mi over all examples; *inner gets the same over the interior examples only (all but
   one example at the smallest x and one at the largest), where predicting a hidden example is interpolation */
static double loo(int mi, const double *x, const double *y, int n, double *inner)
{
    double xs[BT_MAX_EXAMPLES], ys[BT_MAX_EXAMPLES], s = 0, si = 0, p;
    int imin = 0, imax = 0, ni = 0;
    for (int i = 1; i < n; i++) { if (x[i] < x[imin]) imin = i; if (x[i] >= x[imax]) imax = i; }
    for (int i = 0; i < n; i++) {
        int k = 0;
        for (int j = 0; j < n; j++) if (j != i) { xs[k] = x[j]; ys[k] = y[j]; k++; }
        model(mi, xs, ys, n - 1, &x[i], 1, &p);
        s += (p - y[i]) * (p - y[i]);
        if (i != imin && i != imax) { si += (p - y[i]) * (p - y[i]); ni++; }
    }
    *inner = ni ? dsqrt(si / ni) : dsqrt(s / n);
    return dsqrt(s / n);
}

/* the most interior error, over the spread of the readings, that a real trend shows: the middle of the gap between
   trends and noise measured on edai's six sets of problems (8 examples: 0.445 vs 0.452; 12: 0.307 vs 0.537) */
static double inner_cut(int n)
{
    if (n <= 8) return 0.4485;
    if (n >= 12) return 0.406;
    return 0.4485 + (n - 8) * (0.406 - 0.4485) / 4;
}

static void trend(const int64_t *xi, const int64_t *yi, int n, const int64_t *qi, int nq, bt_answer *out)
{
    double x[BT_MAX_EXAMPLES], y[BT_MAX_EXAMPLES], q[2], sc[NMODEL], best_inner = 1e300; int id[NMODEL];
    for (int i = 0; i < n; i++) { x[i] = (double)xi[i]; y[i] = (double)yi[i]; }
    for (int j = 0; j < nq; j++) q[j] = (double)qi[j];
    for (int m = 0; m < NMODEL; m++) {
        double inner;
        sc[m] = loo(m, x, y, n, &inner); id[m] = m;
        if (inner < best_inner) best_inner = inner;
        for (int j = m; j > 0 && (sc[j] < sc[j - 1] || (sc[j] == sc[j - 1] && id[j] < id[j - 1])); j--) {
            double t = sc[j]; sc[j] = sc[j - 1]; sc[j - 1] = t; int u = id[j]; id[j] = id[j - 1]; id[j - 1] = u;
        }
    }
    double sy = pstd(y, n); if (sy == 0) sy = 1;
    /* a trend or noise? With 5 or more examples, judge by the interior error alone: the questions lie between the
       examples, and leaving out an end example asks a model to extrapolate, which inflates the error of real curves */
    double score = n >= 5 ? best_inner / sy : sc[0] / sy;
    out->bits = (int32_t)(score * 1000 + 0.5);
    if (!(score < (n >= 5 ? inner_cut(n) : 0.6))) return;
    double num[2] = {0, 0}, den = 0, p[2];
    for (int t = 0; t < 10; t++) {
        double w = 1.0 / (sc[t] * sc[t] + 1e-9);
        model(id[t], x, y, n, q, nq, p);
        for (int j = 0; j < nq; j++) num[j] += w * p[j];
        den += w;
    }
    out->by = BT_TREND;
    for (int j = 0; j < nq; j++) {
        double e = num[j] / den; out->est[j] = e;
        double c = e * 100; out->value[j] = (int64_t)(c < 0 ? -(-c + 0.5) : c + 0.5);
    }
}

/* rule first; if none, an estimate or CANNOT */
int bt_solve(const int64_t *x, const int64_t *y, int n, const int64_t *q, int nq, bt_answer *out);
int bt_solve(const int64_t *x, const int64_t *y, int n, const int64_t *q, int nq, bt_answer *out)
{
    int rc = bt_solve_rule(x, y, n, q, nq, out);
    if (rc || out->by != BT_CANNOT) return rc;
    trend(x, y, n, q, nq, out);
    return 0;
}

/* ================================================================ sequences (exact rationals) */

typedef struct { i64 n, d; } rat;
static int ovf;
static rat mk(i64 n, i64 d)
{
    rat r; if (d < 0) { n = -n; d = -d; }
    i64 g = (i64)gcd(uabs(n), (u64)d); if (g > 1) { n /= g; d /= g; }
    if (n == 0) d = 1;
    r.n = n; r.d = d; return r;
}
static rat rI(i64 v) { rat r; r.n = v; r.d = 1; return r; }
static rat rmul(rat a, rat b)
{
    i64 g1 = (i64)gcd(uabs(a.n), (u64)b.d), g2 = (i64)gcd(uabs(b.n), (u64)a.d), n, d;
    if (g1 < 1) g1 = 1; if (g2 < 1) g2 = 1;
    if (__builtin_mul_overflow(a.n / g1, b.n / g2, &n) || __builtin_mul_overflow(a.d / g2, b.d / g1, &d)) { ovf = 1; return rI(0); }
    return mk(n, d);
}
static rat radd(rat a, rat b)
{
    i64 g = (i64)gcd((u64)a.d, (u64)b.d), l, x, y, n;
    if (__builtin_mul_overflow(a.d / g, b.d, &l) || __builtin_mul_overflow(a.n, b.d / g, &x) ||
        __builtin_mul_overflow(b.n, a.d / g, &y) || __builtin_add_overflow(x, y, &n)) { ovf = 1; return rI(0); }
    return mk(n, l);
}
static rat rneg(rat a) { a.n = -a.n; return a; }
static rat rdiv(rat a, rat b) { rat i = mk(b.d, b.n); return rmul(a, i); }

/* Gauss-Jordan over the rationals: rows x ncol unknowns; 0 if inconsistent or under-determined */
static int solve_exact(rat M[][8], int rows, int ncol, rat *sol)
{
    int piv[8], r = 0;
    for (int c = 0; c < ncol; c++) {
        int p = -1; for (int i = r; i < rows; i++) if (M[i][c].n != 0) { p = i; break; }
        if (p < 0) continue;
        for (int j = 0; j <= ncol; j++) { rat t = M[r][j]; M[r][j] = M[p][j]; M[p][j] = t; }
        rat inv = M[r][c];
        for (int j = 0; j <= ncol; j++) M[r][j] = rdiv(M[r][j], inv);
        for (int i = 0; i < rows; i++) if (i != r && M[i][c].n != 0) {
            rat f = M[i][c];
            for (int j = 0; j <= ncol; j++) M[i][j] = radd(M[i][j], rneg(rmul(f, M[r][j])));
        }
        piv[r++] = c;
        if (ovf) return 0;
    }
    for (int i = r; i < rows; i++) if (M[i][ncol].n != 0) return 0;
    if (r < ncol) return 0;
    for (int i = 0; i < ncol; i++) sol[i] = rI(0);
    for (int i = 0; i < r; i++) sol[piv[i]] = M[i][ncol];
    return !ovf;
}

int bt_solve_sequence(const int64_t *t, int n, bt_answer *out)
{
    if (!t || !out || n < 3 || n > BT_MAX_EXAMPLES) return -1;
    out->by = BT_CANNOT; out->bits = 0; out->value[0] = out->value[1] = 0; out->est[0] = out->est[1] = 0;
    int bestc = 1 << 30; i64 bv[2] = {0, 0};
    for (int order = 0; order < 4; order++) for (int npoly = 0; npoly < 4; npoly++) {
        int npar = order + npoly, eqs = n - order;
        if (npar == 0 || eqs - npar < 2) continue;
        rat M[BT_MAX_EXAMPLES][8], sol[8];
        int rows = 0; ovf = 0;
        for (int k = order; k < n; k++, rows++) {
            for (int i = 1; i <= order; i++) M[rows][i - 1] = rI(t[k - i]);
            i64 pw = 1; for (int d = 0; d < npoly; d++) { M[rows][order + d] = rI(pw); pw *= k; }
            M[rows][npar] = rI(t[k]);
        }
        if (!solve_exact(M, rows, npar, sol)) continue;
        rat seq[BT_MAX_EXAMPLES + 2];
        for (int i = 0; i < n; i++) seq[i] = rI(t[i]);
        for (int k = n; k < n + 2; k++) {
            rat v = rI(0);
            for (int i = 1; i <= order; i++) v = radd(v, rmul(sol[i - 1], seq[k - i]));
            i64 pw = 1; for (int d = 0; d < npoly; d++) { v = radd(v, rmul(sol[order + d], rI(pw))); pw *= k; }
            seq[k] = v;
        }
        if (ovf || seq[n].d != 1 || seq[n + 1].d != 1) continue;
        int cost = 2 * npar; for (int i = 0; i < npar; i++) if (sol[i].d != 1) cost += 2;
        if (cost < bestc) { bestc = cost; bv[0] = seq[n].n; bv[1] = seq[n + 1].n; }
    }
    for (int per = 2; per <= 3; per++) {                           /* alternating steps a*t + b */
        rat A[3], Bb[3]; int ok = 1; ovf = 0;
        static const i64 an[14] = {-5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 1, -1, 1}, ad[14] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3};
        for (int ph = 0; ph < per && ok; ph++) {
            int ks[BT_MAX_EXAMPLES], np = 0;
            for (int k = 1; k < n; k++) if (k % per == ph) ks[np++] = k;
            if (np < 2) { ok = 0; break; }
            int found = 0;
            for (int ai = 0; ai < 14 && !found; ai++) {
                rat a = mk(an[ai], ad[ai]);
                rat b = radd(rI(t[ks[0]]), rneg(rmul(a, rI(t[ks[0] - 1]))));
                int all = 1;
                for (int s = 0; s < np && all; s++) { rat v = radd(rmul(a, rI(t[ks[s] - 1])), b); all = v.n == t[ks[s]] && v.d == 1; }
                if (all && !ovf) { A[ph] = a; Bb[ph] = b; found = 1; }
            }
            if (!found) ok = 0;
        }
        if (!ok) continue;
        rat cur = rI(t[n - 1]), nx[2];
        for (int k = n; k < n + 2; k++) { cur = radd(rmul(A[k % per], cur), Bb[k % per]); nx[k - n] = cur; }
        if (ovf || nx[0].d != 1 || nx[1].d != 1) continue;
        if (3 * per < bestc) { bestc = 3 * per; bv[0] = nx[0].n; bv[1] = nx[1].n; }
    }
    if (bestc < (1 << 30)) { out->by = BT_SEQ; out->bits = bestc; out->value[0] = bv[0]; out->value[1] = bv[1]; }
    return 0;
}
