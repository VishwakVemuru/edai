/* aa.h -- the edai brain's public header.  The brain itself is supplied as compiled libraries (../lib/<target>/libaa.a).

   The brain answers small problems from a handful of examples: exactly, with a rule that AM has found and then proved
   (an exact answer never contradicts an example), searching where its net points when its first look finds nothing;
   or, when there is no such rule, an estimate; or CANNOT.  It needs no allocation, no operating system and no C library, keeps all its state in the objects below,
   and gives the same answers on every target.

   Quick start:
       static aa_brain brain;
       aa_answer a;
       aa_brain_init(&brain);
       const int64_t x[] = {55, 68, 19, 29, 95, 4, 92, 13}, y[] = {440, 544, 152, 232, 760, 32, 736, 104}, q[] = {111, 118};
       aa_solve_rule(&brain, x, y, 8, q, 2, &a);   // a.by == AA_AM, a.value == {888, 944}

   Every function returns AA_OK (0) or a negative AA_ERR_ code. */
#ifndef AA_H
#define AA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AA_VERSION_MAJOR 1
#define AA_VERSION_MINOR 2
#define AA_VERSION_PATCH 0
#define AA_VERSION ((AA_VERSION_MAJOR << 16) | (AA_VERSION_MINOR << 8) | AA_VERSION_PATCH)
uint32_t aa_version(void);

#define AA_OK            0
#define AA_ERR_NULL     (-1)
#define AA_ERR_RANGE    (-2)
#define AA_ERR_INIT     (-3)
#define AA_ERR_VERSION  (-4)

/* who answered (aa_answer.by) */
#define AA_CANNOT    0         /* no answer */
#define AA_AM        1         /* an exact rule, proven by AM: one rule of its steps fits all the examples and gives
                                  this answer, or fits all but the one to three of smallest x and every question lies
                                  at or above the rest */
#define AA_SPLITTER  2         /* an exact rule in pieces that together cover every example */
#define AA_NET       3         /* a checked estimate of the net's */
#define AA_AM_ESTIMATE 4       /* an estimate from a rule of AM's that fits every example but the one or two the net
                                  doubted most (they disagree with it) */

#define AA_MAX_EXAMPLES   16
#define AA_MAX_QUESTIONS   2
#define AA_MIN_TERMS       3

#define AA_BRAIN_BYTES   5216u
#define AA_STREAM_BYTES  4960u
#define AA_STACK_BYTES   3072u    /* the most stack any call needs */

typedef struct {
    uint32_t version, brain_bytes, stream_bytes, stack_bytes;
    const char *target;
} aa_info_t;
int32_t aa_info(aa_info_t *info);

typedef struct aa_brain { uint64_t opaque_[AA_BRAIN_BYTES / 8]; } aa_brain;

typedef struct {
    int32_t by;                /* AA_AM, AA_SPLITTER, AA_NET or AA_CANNOT */
    int32_t learned;
    int64_t value[2];          /* the answers */
    int64_t estimate[2];
    float self_check;
    float fit;
} aa_answer;

#define aa_brain_init(brain) aa_brain_init_((brain), AA_VERSION, (uint32_t)sizeof(aa_brain))
int32_t aa_brain_init_(aa_brain *brain, uint32_t header_version, uint32_t brain_size);

/* a hidden rule y = f(x): n examples (1..16) and nq questions (1 or 2) */
int32_t aa_solve_rule(aa_brain *brain, const int64_t *x, const int64_t *y, int32_t n,
                      const int64_t *q, int32_t nq, aa_answer *answer);
/* the first n terms of a sequence (3..16): its next two terms */
int32_t aa_solve_sequence(aa_brain *brain, const int64_t *terms, int32_t n, aa_answer *answer);

/* online learning from a stream of examples: two inputs, one output */
typedef struct aa_stream { uint64_t opaque_[AA_STREAM_BYTES / 8]; } aa_stream;
typedef struct {
    float in_lo[2], in_hi[2];
    float out_lo, out_hi;
    float rate;
    uint32_t seed;
} aa_stream_config;
int32_t aa_stream_defaults(aa_stream_config *config);
/* config NULL: the defaults (those aa_stream_defaults gives) */
#define aa_stream_init(stream, config) aa_stream_init_((stream), (config), AA_VERSION, (uint32_t)sizeof(aa_stream))
int32_t aa_stream_init_(aa_stream *stream, const aa_stream_config *config, uint32_t header_version, uint32_t stream_size);
int32_t aa_observe(aa_stream *stream, float in0, float in1, float target, float *before);
int32_t aa_predict(aa_stream *stream, float in0, float in1, float *prediction);
int32_t aa_stream_count(const aa_stream *stream, uint32_t *count);

#ifdef __cplusplus
}
#endif
#endif /* AA_H */
