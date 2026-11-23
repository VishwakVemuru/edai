/* test problems in edai's own format, for edai's own runner */
#ifndef STREAM_DATA_H
#define STREAM_DATA_H
#include <stdint.h>
#define N_PROBLEMS 6
#define MAX_EXAMPLES 16
typedef struct { uint8_t kind; uint8_t family; uint8_t n; int64_t x[MAX_EXAMPLES]; int64_t y[MAX_EXAMPLES]; int64_t q[2]; double truth[2]; double tol; } problem_t;
static const problem_t PROBLEMS[N_PROBLEMS] = {
    { 0, 0, 6, { 83LL, 2LL, 73LL, -35LL, 95LL, 59LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 80LL, 4LL, 70LL, -38LL, 92LL, 56LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 95LL, -35LL }, { 92.0, -38.0 }, 0.0 },
    { 0, 2, 8, { 5LL, 5LL, 10LL, 20LL, 30LL, 40LL, 50LL, 60LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 1LL, 2LL, 20LL, 40LL, 60LL, 80LL, 100LL, 120LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 70LL, 80LL }, { 0.0, 0.0 }, 0.0 },
    { 0, 0, 8, { 88LL, 5LL, -31LL, -12LL, 40LL, 17LL, 63LL, -2LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { -380LL, 35LL, 215LL, 120LL, -140LL, -25LL, -255LL, 70LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 30LL, 100LL }, { -90.0, -440.0 }, 0.0 },
    { 0, 0, 8, { 88LL, 5LL, -31LL, -12LL, 40LL, 17LL, 63LL, -2LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 3916LL, 15LL, 465LL, 66LL, 820LL, 153LL, 2016LL, 1LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 30LL, 100LL }, { 465.0, 5050.0 }, 0.0 },
    { 0, 0, 8, { 88LL, 5LL, -31LL, -12LL, 40LL, 17LL, 63LL, -2LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 7482LL, 12LL, 1056LL, 182LL, 1482LL, 240LL, 3782LL, 12LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 30LL, 100LL }, { 812.0, 9702.0 }, 0.0 },
    { 0, 0, 8, { 88LL, 5LL, -31LL, -12LL, 40LL, 17LL, 63LL, -2LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 183LL, 17LL, -55LL, -17LL, 87LL, 41LL, 133LL, 3LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL, 0LL }, { 30LL, 100LL }, { 67.0, 207.0 }, 0.0 },
};
#endif
