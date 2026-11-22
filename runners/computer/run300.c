/* The 300 problems on a computer, through the compiled brain (../../brain/lib/<target>/libaa.a): one line per problem,
   then the score.  make && ./run300   (or: make TARGET=linux-x86_64) */
#include <stdio.h>
#include "harness.h"

static aa_brain brain;

int main(void)
{
    char line[160]; int right = 0, by_family[3] = { 0, 0, 0 };
    aa_info_t info; aa_info(&info);
    if (aa_brain_init(&brain) != AA_OK) { printf("brain did not start\n"); return 1; }
    for (int i = 0; i < N_PROBLEMS; i++) {
        aa_answer a; int32_t rc = h_solve(i, &brain, &a); int ok = rc == AA_OK && h_right(i, &a);
        right += ok; by_family[PROBLEMS[i].family] += ok;
        h_line(line, i, rc, &a, ok); printf("%s\n", line);
    }
    printf("SUMMARY right %d/%d rules %d/120 trends %d/120 noise %d/60 (library %s)\n", right, N_PROBLEMS, by_family[0], by_family[1], by_family[2], info.target);
    return 0;
}
