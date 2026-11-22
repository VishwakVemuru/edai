/* deep.c -- an optional, larger search for beat.c (computers only: it needs a few MB).
   Chains of up to three operations on x, each with its own constant -- such as ((x & 35) ^ 3) % 7 -- then a*f + b.
   Included by beat.c when BT_DEEP is defined. Python integer semantics: // and % floor, >> floors. */

enum { D_ADD, D_MUL, D_AND, D_OR, D_XOR, D_MOD, D_FDIV, D_SHR, D_SHL, D_SQ, D_ABS, D_NEG };
typedef struct { uint8_t kind; int16_t c; } dop;
#define NDOP 400
static dop DOPS[NDOP];
static int ndop;

static void deep_init(void)
{
    if (ndop) return;
    for (int c = -20; c <= 20; c++) if (c) { DOPS[ndop].kind = D_ADD; DOPS[ndop++].c = (int16_t)c; }
    static const int mc[] = {-3, -2, -1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    for (int i = 0; i < 14; i++) { DOPS[ndop].kind = D_MUL; DOPS[ndop++].c = (int16_t)mc[i]; }
    for (int c = 1; c <= 63; c++) for (int k = D_AND; k <= D_XOR; k++) { DOPS[ndop].kind = (uint8_t)k; DOPS[ndop++].c = (int16_t)c; }
    for (int c = 2; c <= 32; c++) { DOPS[ndop].kind = D_MOD; DOPS[ndop++].c = (int16_t)c; DOPS[ndop].kind = D_FDIV; DOPS[ndop++].c = (int16_t)c; }
    for (int c = 1; c <= 5; c++) { DOPS[ndop].kind = D_SHR; DOPS[ndop++].c = (int16_t)c; }
    for (int c = 1; c <= 4; c++) { DOPS[ndop].kind = D_SHL; DOPS[ndop++].c = (int16_t)c; }
    DOPS[ndop++].kind = D_SQ; DOPS[ndop++].kind = D_ABS; DOPS[ndop++].kind = D_NEG;
}

static int dapply(const dop *o, i64 v, i64 *r)
{
    i64 c = o->c;
    switch (o->kind) {
    case D_ADD: *r = v + c; break;
    case D_MUL: if (__builtin_mul_overflow(v, c, r)) return 0; break;
    case D_AND: *r = (i64)((u64)v & (u64)c); break;
    case D_OR: *r = (i64)((u64)v | (u64)c); break;
    case D_XOR: *r = (i64)((u64)v ^ (u64)c); break;
    case D_MOD: *r = fmodp(v, c); break;
    case D_FDIV: *r = fdiv(v, c); break;
    case D_SHR: *r = v >= 0 ? (i64)((u64)v >> c) : -(i64)(((u64)(-(v + 1))) >> c) - 1; break;
    case D_SHL: if (__builtin_mul_overflow(v, (i64)1 << c, r)) return 0; break;
    case D_SQ: if (__builtin_mul_overflow(v, v, r)) return 0; break;
    case D_ABS: *r = v < 0 ? -v : v; break;
    default: *r = -v; break;
    }
    return *r <= LIMIT && *r >= -LIMIT;
}

static int dvec(const dop *o, const i64 *src, i64 *dst)
{
    for (int i = 0; i < NP; i++) if (!dapply(o, src[i], &dst[i])) return 0;
    return 1;
}

/* the two-step formulas, without repeats */
#define DEEP_MAX2 (1 << 17)
static i64 (*D2)[MAXP];
static uint32_t *DH;                     /* hash table of D2 indices + 1, 2^18 slots */
static int nd2;

static u64 vhash(const i64 *v) { u64 h = 1469598103934665603ULL; for (int i = 0; i < NP; i++) { h ^= (u64)v[i]; h *= 1099511628211ULL; h ^= h >> 29; } return h; }

static int d2_add(const i64 *v)
{
    u64 h = vhash(v); uint32_t s = (uint32_t)(h & ((1u << 18) - 1));
    while (DH[s]) { if (eqv(D2[DH[s] - 1], v)) return 0; s = (s + 1) & ((1u << 18) - 1); }
    if (nd2 >= DEEP_MAX2) return 0;
    for (int i = 0; i < NP; i++) D2[nd2][i] = v[i];
    DH[s] = (uint32_t)(++nd2);
    return 1;
}

typedef struct { int ok; int32_t c; i64 pred[2]; } dres;
#define DT_MAX 8
typedef struct { uint32_t emask; int qmask; int k; uint8_t pts[BT_MAX_EXAMPLES]; dres best; } dtarget;
static dtarget DT[DT_MAX];
static int ndt;

/* y = a F + b exactly on a target's examples, a and b integers, a != 0; returns length or -1 */
static int32_t deep_fit_t(const dtarget *t, const i64 *F, i64 *pa, i64 *pb)
{
    int i0 = t->pts[0], i1 = -1;
    for (int s = 1; s < t->k; s++) if (F[t->pts[s]] != F[i0]) { i1 = t->pts[s]; break; }
    if (i1 < 0) return -1;
    i64 dF = F[i1] - F[i0], dy = Y[i1] - Y[i0];
    if (dy % dF) return -1;
    i64 a = dy / dF; if (a == 0) return -1;
    i64 b = Y[i0] - a * F[i0];
    for (int s = 0; s < t->k; s++) { int j = t->pts[s]; i64 u; if (__builtin_mul_overflow(a, F[j], &u) || u + b != Y[j]) return -1; }
    *pa = a; *pb = b;
    return (a == 1 ? 0 : numbits(a, 1)) + (b == 0 ? 0 : numbits(b, 1));
}

static void dt_offer(dtarget *t, const i64 *F, int depth, int32_t opbits, int full)
{
    i64 a, b;
    int32_t c = deep_fit_t(t, F, &a, &b);
    if (c < 0) return;
    c += depth * opbits + ONE;
    if (t->best.ok && c >= t->best.c) return;
    if (!full) return;                                   /* caller completes the question values first */
    t->best.ok = 1; t->best.c = c;
    for (int s = 0; s < NQ; s++) t->best.pred[s] = (t->qmask >> s & 1) ? a * F[NE + s] + b : 0;
}

static int32_t deep_fit(const i64 *F, i64 *pa, i64 *pb)
{
    int i0 = 0, i1 = -1;
    for (int j = 1; j < NE; j++) if (F[j] != F[i0]) { i1 = j; break; }
    if (i1 < 0) return -1;
    i64 dF = F[i1] - F[i0], dy = Y[i1] - Y[i0];
    if (dy % dF) return -1;
    i64 a = dy / dF; if (a == 0) return -1;
    i64 b = Y[i0] - a * F[i0];
    for (int j = 0; j < NE; j++) { i64 t; if (__builtin_mul_overflow(a, F[j], &t) || t + b != Y[j]) return -1; }
    *pa = a; *pb = b;
    return (a == 1 ? 0 : numbits(a, 1)) + (b == 0 ? 0 : numbits(b, 1));
}

static void deep_run(void)
{
    deep_init();
    for (int t = 0; t < ndt; t++) { DT[t].best.ok = 0; DT[t].best.c = 0x7fffffff; }
    if (!D2) { D2 = malloc(sizeof(i64[MAXP]) * DEEP_MAX2); DH = malloc(sizeof(uint32_t) << 18); if (!D2 || !DH) return; }
    for (uint32_t i = 0; i < (1u << 18); i++) DH[i] = 0;
    nd2 = 0;
    int32_t opbits = lg((u64)ndop);
    i64 v1[MAXP], v2[MAXP], v3[MAXP];
    for (int o = 0; o < ndop; o++) {
        if (!dvec(&DOPS[o], X, v1)) continue;
        for (int t = 0; t < ndt; t++) dt_offer(&DT[t], v1, 1, opbits, 1);
        d2_add(v1);
    }
    int n1 = nd2;
    for (int i = 0; i < n1; i++) for (int o = 0; o < ndop; o++) {
        if (!dvec(&DOPS[o], D2[i], v2) || !d2_add(v2)) continue;
        for (int t = 0; t < ndt; t++) dt_offer(&DT[t], v2, 2, opbits, 1);
    }
    int open_ = 0; for (int t = 0; t < ndt; t++) if (!DT[t].best.ok && DT[t].k >= 4) open_ = 1;
    if (!open_) return;
    for (int i = n1; i < nd2; i++) for (int o = 0; o < ndop; o++) {
        const dop *op = &DOPS[o];
        int ok = 1;
        for (int p = 0; p < NP && ok; p++) ok = dapply(op, D2[i][p], &v3[p]);
        if (!ok) continue;
        for (int t = 0; t < ndt; t++) if (!DT[t].best.ok && DT[t].k >= 4) dt_offer(&DT[t], v3, 3, opbits, 1);
    }
}

static int dt_add(uint32_t emask, int qmask)
{
    if (ndt >= DT_MAX) return -1;
    dtarget *t = &DT[ndt]; t->emask = emask; t->qmask = qmask; t->k = 0;
    for (uint32_t m = emask; m; m &= m - 1) t->pts[t->k++] = (uint8_t)__builtin_ctz(m);
    return ndt++;
}

static dres deep_search(void)
{
    ndt = 0; dt_add((1u << NE) - 1, (1 << NQ) - 1);
    deep_run();
    return DT[0].best;
}
