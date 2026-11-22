/* edai brain, Uno edition (experimental): the entry points.  Plain C.  It answers x -> y problems (rules, trends
   and noise given as examples); the full brain's sequence laws are left out to fit the Uno. */
#ifndef EDAI_UNO_H
#define EDAI_UNO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define EU_MAX_EXAMPLES 12

typedef struct {
    int8_t by;            /* 0 CANNOT, 1 AM (an exact rule), 2 AM's splitter (an exact rule in pieces), 3 the net */
    uint8_t learned;      /* 1 if the net ran */
    int16_t check;        /* the net's self-check in thousandths, if it ran */
    int64_t value[2];     /* the two answers (0 for CANNOT) */
} eu_answer;

/* n examples of a hidden rule y = f(x) (1 to 12) and nq questions (1 or 2).  Returns 0, or -2 if a count is out of range. */
int eu_solve_rule(const int32_t *x, const int32_t *y, int n, const int32_t *q, int nq, eu_answer *a);
/* the bytes the brain keeps: the problem in hand and the net */
unsigned eu_brain_bytes(void);

#ifdef __cplusplus
}
#endif
#endif
