/* beat.h -- a small solver for the edai 300-problem challenge, in portable C.

   It answers a problem from its own examples only:
     - a hidden integer rule x -> y: the shortest exact formula (or formula in pieces) that fits every example,
       accepted only when it is much shorter than the examples it explains;
     - a sequence: an exact linear recurrence (with a polynomial term) or an alternation of two or three steps;
     - otherwise an estimate from an ensemble of smooth curve fits, or CANNOT when no curve predicts the examples it
       did not see better than their average.

   No allocation, no C library (not even <math.h>), no operating system. All working memory is static inside
   beat.c (see BT_RAM_BYTES via bt_ram_bytes()). Rules are found with integer arithmetic only; the estimates use
   doubles with only + - * / and the file's own exp and sqrt, so every IEEE-754 machine gives the same answers,
   provided the compiler does not fuse multiply-adds (-ffp-contract=off). Not reentrant: one problem at a time. */
#ifndef BEAT_H
#define BEAT_H
#include <stdint.h>

#define BT_MAX_EXAMPLES 16
#define BT_MAX_QUESTIONS 2

#define BT_CANNOT 0
#define BT_RULE   1      /* one exact formula */
#define BT_PIECES 2      /* an exact formula in pieces */
#define BT_SEQ    3      /* an exact sequence rule */
#define BT_TREND  4      /* an estimate (est[] holds it, value[] holds it rounded to cents) */

typedef struct {
    int32_t by;          /* BT_RULE, BT_PIECES, BT_SEQ, BT_TREND or BT_CANNOT */
    int32_t bits;        /* rule length in 1/65536 bits, or the trend score x 1000 */
    int64_t value[2];    /* exact answers; for a trend, the estimate x 100, rounded */
    double est[2];       /* a trend's estimate */
} bt_answer;

/* returns 0, or -1 for bad sizes */
int bt_solve_rule(const int64_t *x, const int64_t *y, int n, const int64_t *q, int nq, bt_answer *out);
int bt_solve_sequence(const int64_t *t, int n, bt_answer *out);
uint32_t bt_ram_bytes(void);        /* the static working memory the solver uses */
uint32_t bt_last_queries(void);     /* pieces examined in the last rule problem (for sizing) */

#endif
