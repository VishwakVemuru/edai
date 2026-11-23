/* Probes edai's documented contract: every call should return AA_OK (0) or a negative AA_ERR_ code
   (-1 NULL, -2 RANGE, -3 INIT, -4 VERSION), never crash or hang. Each case runs in its own child process. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <inttypes.h>
#include "aa.h"

static int64_t X[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}, Y[16], Q[2] = {20, 21};
static aa_brain B; static aa_answer A;
static int cases, bad;

static void run(const char *name, int expect, int (*fn)(void))
{
    fflush(stdout);
    pid_t p = fork();
    if (p == 0) { alarm(20); int rc = fn(); _exit(rc < 0 ? 200 + (-rc) : rc == 0 ? 100 : 150); }
    int st; waitpid(p, &st, 0); cases++;
    char got[64];
    int rc = 999;
    if (WIFSIGNALED(st)) snprintf(got, sizeof got, "CRASH signal %d%s", WTERMSIG(st), WTERMSIG(st) == SIGALRM ? " (hang)" : "");
    else { int c = WEXITSTATUS(st); rc = c == 100 ? 0 : c >= 200 ? -(c - 200) : c == 150 ? 1 : c; snprintf(got, sizeof got, "rc=%d", rc); }
    int ok = !WIFSIGNALED(st) && (expect == 99 ? (rc <= 0) : rc == expect);
    if (!ok) bad++;
    printf("%-58s expect %-4s got %-24s %s\n", name, expect == 99 ? "<=0" : (expect == -1 ? "-1" : expect == -2 ? "-2" : expect == -3 ? "-3" : expect == -4 ? "-4" : "0"), got, ok ? "ok" : "<<< VIOLATION");
}
#define EXP(e) (e)

/* ---- cases ---- */
static int c_null_brain(void) { aa_answer a; return aa_solve_rule(NULL, X, Y, 8, Q, 2, &a); }
static int c_null_x(void) { aa_brain_init(&B); return aa_solve_rule(&B, NULL, Y, 8, Q, 2, &A); }
static int c_null_y(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, NULL, 8, Q, 2, &A); }
static int c_null_q(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 8, NULL, 2, &A); }
static int c_null_ans(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 8, Q, 2, NULL); }
static int c_n0(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 0, Q, 2, &A); }
static int c_nneg(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, -1, Q, 2, &A); }
static int c_n17(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 17, Q, 2, &A); }
static int c_nmax(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 2147483647, Q, 2, &A); }
static int c_nq0(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 8, Q, 0, &A); }
static int c_nq3(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 8, Q, 3, &A); }
static int c_nqneg(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 8, Q, -1, &A); }
static int c_seq2(void) { aa_brain_init(&B); return aa_solve_sequence(&B, X, 2, &A); }
static int c_seq17(void) { aa_brain_init(&B); return aa_solve_sequence(&B, X, 17, &A); }
static int c_seqnull(void) { aa_brain_init(&B); return aa_solve_sequence(&B, NULL, 8, &A); }
static int c_uninit_zero(void) { memset(&B, 0, sizeof B); return aa_solve_rule(&B, X, Y, 8, Q, 2, &A); }
static int c_uninit_garbage(void) { memset(&B, 0x5A, sizeof B); return aa_solve_rule(&B, X, Y, 8, Q, 2, &A); }
static int c_bad_version(void) { return aa_brain_init_(&B, 0x00090000u, (uint32_t)sizeof B); }
static int c_bad_size(void) { return aa_brain_init_(&B, AA_VERSION, 100); }
static int c_bad_size_big(void) { return aa_brain_init_(&B, AA_VERSION, (uint32_t)sizeof B + 8); }
static int c_init_null(void) { return aa_brain_init_(NULL, AA_VERSION, (uint32_t)sizeof B); }
static int c_info_null(void) { return aa_info(NULL); }
static int c_alias_ans_in_brain(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, Y, 8, Q, 2, (aa_answer *)&B); }
static int c_alias_xy(void) { aa_brain_init(&B); return aa_solve_rule(&B, X, X, 8, Q, 2, &A); }
static int c_alias_ans_over_x(void) { static int64_t buf[64]; for (int i = 0; i < 16; i++) buf[i] = i; aa_brain_init(&B); return aa_solve_rule(&B, buf, buf + 16, 8, buf + 32, 2, (aa_answer *)buf); }

static int corrupt_off;
static int c_corrupt(void) { aa_brain_init(&B); ((unsigned char *)&B)[corrupt_off] ^= 0xFF; return aa_solve_rule(&B, X, Y, 8, Q, 2, &A); }

/* stream API */
static aa_stream S; static aa_stream_config C;
static int c_stream_null_cfg(void) { return aa_stream_init_(&S, NULL, AA_VERSION, (uint32_t)sizeof S); }
static int c_stream_defaults_null(void) { return aa_stream_defaults(NULL); }
static float nanf_(void) { return nanf(""); }
static int stream_with(float lo0, float hi0, float olo, float ohi, float rate) {
    aa_stream_defaults(&C); C.in_lo[0] = lo0; C.in_hi[0] = hi0; C.out_lo = olo; C.out_hi = ohi; C.rate = rate;
    return aa_stream_init(&S, &C);
}
static int c_stream_inlo_gt_hi(void) { return stream_with(10, -10, 0, 1, 0.01f); }
static int c_stream_out_equal(void) { return stream_with(0, 1, 5, 5, 0.01f); }
static int c_stream_rate_nan(void) { return stream_with(0, 1, 0, 1, nanf_()); }
static int c_stream_rate_neg(void) { return stream_with(0, 1, 0, 1, -1); }
static int c_stream_rate_huge(void) { return stream_with(0, 1, 0, 1, 1e30f); }
static int c_stream_range_nan(void) { return stream_with(nanf_(), 1, 0, 1, 0.01f); }
static int c_stream_range_inf(void) { return stream_with(-INFINITY, INFINITY, 0, 1, 0.01f); }
static int observe_many(float a, float b, float t, int n, int check_nan) {
    aa_stream_defaults(&C); int rc = aa_stream_init(&S, &C); if (rc) return rc;
    float before = 0, pr = 0;
    for (int i = 0; i < n; i++) { rc = aa_observe(&S, a, b, t, &before); if (rc) return rc; }
    rc = aa_predict(&S, 0.5f, 0.5f, &pr); if (rc) return rc;
    if (check_nan && (isnan(pr) || isinf(pr))) return 7;            /* success code but a NaN/Inf prediction */
    return 0;
}
static int c_obs_nan_in(void) { return observe_many(nanf_(), 0.5f, 0.3f, 100, 1); }
static int c_obs_nan_target(void) { return observe_many(0.5f, 0.5f, nanf_(), 100, 1); }
static int c_obs_inf(void) { return observe_many(INFINITY, 0.5f, 0.3f, 100, 1); }
static int c_obs_huge(void) { return observe_many(1e38f, -1e38f, 1e38f, 1000, 1); }
static int c_obs_denormal(void) { return observe_many(1e-45f, 1e-45f, 1e-45f, 1000, 1); }
static int c_obs_null_before(void) { aa_stream_defaults(&C); aa_stream_init(&S, &C); return aa_observe(&S, 0.1f, 0.2f, 0.3f, NULL); }
static int c_predict_null(void) { aa_stream_defaults(&C); aa_stream_init(&S, &C); return aa_predict(&S, 0.1f, 0.2f, NULL); }
static int c_stream_uninit(void) { float p; memset(&S, 0x33, sizeof S); return aa_predict(&S, 0.1f, 0.2f, &p); }
static int c_obs_million(void) {
    aa_stream_defaults(&C); aa_stream_init(&S, &C); float before;
    for (int i = 0; i < 3000000; i++) { int rc = aa_observe(&S, (float)(i % 100) / 100.0f, 0.5f, (float)(i % 7) / 7.0f, &before); if (rc) return rc; }
    uint32_t n = 0; aa_stream_count(&S, &n); return n == 3000000u ? 0 : 8;
}

int main(void) {
    for (int i = 0; i < 16; i++) Y[i] = 3 * X[i] + 1;
    printf("NULLs\n");
    run("aa_solve_rule(brain=NULL)", -1, c_null_brain); run("aa_solve_rule(x=NULL)", -1, c_null_x);
    run("aa_solve_rule(y=NULL)", -1, c_null_y); run("aa_solve_rule(q=NULL)", -1, c_null_q);
    run("aa_solve_rule(answer=NULL)", -1, c_null_ans); run("aa_solve_sequence(terms=NULL)", -1, c_seqnull);
    run("aa_brain_init_(NULL)", -1, c_init_null); run("aa_info(NULL)", -1, c_info_null);
    run("aa_stream_init_(config=NULL) (1.1: means defaults)", 0, c_stream_null_cfg); run("aa_stream_defaults(NULL)", -1, c_stream_defaults_null);
    run("aa_observe(before=NULL)", 99, c_obs_null_before); run("aa_predict(prediction=NULL)", -1, c_predict_null);
    printf("ranges\n");
    run("rule n=0", -2, c_n0); run("rule n=-1", -2, c_nneg); run("rule n=17", -2, c_n17); run("rule n=INT32_MAX", -2, c_nmax);
    run("rule nq=0", -2, c_nq0); run("rule nq=3", -2, c_nq3); run("rule nq=-1", -2, c_nqneg);
    run("sequence n=2", -2, c_seq2); run("sequence n=17", -2, c_seq17);
    printf("initialisation and versions\n");
    run("solve with a zeroed (never initialised) brain", -3, c_uninit_zero);
    run("solve with a garbage brain", -3, c_uninit_garbage);
    run("aa_brain_init_ with an old header version", -4, c_bad_version);
    run("aa_brain_init_ with a too-small size", -4, c_bad_size);
    run("aa_brain_init_ with a larger size", 99, c_bad_size_big);
    run("stream: uninitialised object", -3, c_stream_uninit);
    printf("aliasing\n");
    run("answer written into the brain object", 99, c_alias_ans_in_brain);
    run("x and y are the same array", 99, c_alias_xy);
    run("answer overlaps the inputs", 99, c_alias_ans_over_x);
    printf("stream configuration and data\n");
    run("stream in_lo > in_hi", 99, c_stream_inlo_gt_hi); run("stream out_lo == out_hi", 99, c_stream_out_equal);
    run("stream rate = NaN", 99, c_stream_rate_nan); run("stream rate < 0", 99, c_stream_rate_neg);
    run("stream rate = 1e30", 99, c_stream_rate_huge); run("stream range NaN", 99, c_stream_range_nan);
    run("stream range +/-Inf", 99, c_stream_range_inf);
    run("observe NaN input, then predict (NaN result = violation)", 99, c_obs_nan_in);
    run("observe NaN target, then predict", 99, c_obs_nan_target);
    run("observe +Inf input, then predict", 99, c_obs_inf);
    run("observe +/-1e38, then predict", 99, c_obs_huge);
    run("observe denormals, then predict", 99, c_obs_denormal);
    run("3,000,000 observations, count must be exact", 0, c_obs_million);
    printf("brain corrupted one byte at a time after init (offsets 0..5215 step 8)\n");
    int crashes = 0, oks = 0, inits = 0, others = 0;
    for (corrupt_off = 0; corrupt_off < (int)sizeof B; corrupt_off += 8) {
        pid_t p = fork();
        if (p == 0) { alarm(20); int rc = c_corrupt(); _exit(rc < 0 ? 200 + (-rc) : rc == 0 ? 100 : 150); }
        int st; waitpid(p, &st, 0);
        if (WIFSIGNALED(st)) { crashes++; if (crashes <= 5) printf("  offset %d: CRASH signal %d\n", corrupt_off, WTERMSIG(st)); }
        else { int c = WEXITSTATUS(st); if (c == 100) oks++; else if (c == 203) inits++; else others++; }
    }
    printf("  %d offsets: %d returned OK, %d returned AA_ERR_INIT, %d other codes, %d crashed\n", (int)sizeof B / 8, oks, inits, others, crashes);
    printf("\n%d cases, %d violations of the documented contract\n", cases, bad);
    return 0;
}
