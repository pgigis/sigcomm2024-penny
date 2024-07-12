#include <stdio.h>
#include <math.h>
#include <random>
#include <cassert>

#define MAX_N 400

// beware - there's a truncation effect that happens just before
// MAX_DUPS, so set MAX_DUPS to be greater than the maximum duplicates
// that you care about evaluating.
#define MAX_DUPS 50

#define MAX_DUP_THRESH 0.15 
#define DROP_FRAC 0.05
#define PROB_LEGIT_SRC 0.95
#define H1_H2_RATIO 0.01

double rnd() {
    double r = random() / double(RAND_MAX);
    return r;
}

int main() {
    // two dimensional arrays to hold the results for different
    // combinations of N and D (where D is number of dups sent by the malicious user)
    int undecided[MAX_N][MAX_DUPS];
    int bidir[MAX_N][MAX_DUPS];
    int non_bidir[MAX_N][MAX_DUPS];
    int max_dups[MAX_N][MAX_DUPS];
    int totalresults[MAX_N][MAX_DUPS];

    for (int n = 0; n < MAX_N; n++) {
        for (int dups = 0; dups < MAX_DUPS; dups++) {
            undecided[n][dups] = 0;
            bidir[n][dups] = 0;
            non_bidir[n][dups] = 0;
            max_dups[n][dups] = 0;
            totalresults[n][dups] = 0;
        }
    }

    for (int run = 0; run < 1000000; run++) {
        for (int dups = 2; dups < MAX_DUPS; dups++) {
            int dupcount = 0;  // as seen at monitor
            int origdupcount = 0; // as sent by attacker
            int dropcount = 0;
            int correctcount = 0;
            for (int n = 1; n < MAX_N; n++) {
                bool dropped = rnd() <= DROP_FRAC;
                bool duplicated = false;
                if (n <= dups) duplicated = true;

                if (dropped && duplicated) {
                    correctcount++;
                    dropcount++;
                    origdupcount++;
                } else if (dropped) {
                    dropcount++;
                } else if (duplicated) {
                    dupcount++;
                    origdupcount++;
                }
                assert(origdupcount < MAX_DUPS);
                if (n < dups) {
                    continue;
                }

                totalresults[n][dupcount]++;
                if (n != dropped && (double)dupcount / (n - dropped) > MAX_DUP_THRESH) {
                    max_dups[n][origdupcount]++;
                } else {
                    double h1 = pow(1 - PROB_LEGIT_SRC, (double)(dropcount - correctcount));
                    double h2;
                    if (dupcount == 0) {
                        h2 = pow(1.0 / n, (double)correctcount);
                    } else {
                        h2 = pow((double)dupcount / n, (double)correctcount);
                    }
                    double p_bidir = h1 / (h1 + h2);
                    if (p_bidir > (1 - H1_H2_RATIO)) {
                        bidir[n][origdupcount]++;
                    } else if (p_bidir < H1_H2_RATIO) {
                        non_bidir[n][origdupcount]++;
                    } else {
                        undecided[n][origdupcount]++;
                    }
                }
            }
        }
    }

    for (int n = 1; n < MAX_N; n++) {
        for (int d = 0; d < MAX_DUPS; d++) {
            if (totalresults[n][d] > 0) {
                printf("n %d d %d tot %d dups %f bid %f nobid %f undec %f sum %f\n",
                    n, d, totalresults[n][d],
                    (double)max_dups[n][d] / totalresults[n][d],
                    (double)bidir[n][d] / totalresults[n][d],
                    (double)non_bidir[n][d] / totalresults[n][d],
                    (double)undecided[n][d] / totalresults[n][d],
                    (double)max_dups[n][d] / totalresults[n][d] +
                    (double)bidir[n][d] / totalresults[n][d] +
                    (double)non_bidir[n][d] / totalresults[n][d] +
                    (double)undecided[n][d] / totalresults[n][d]);
            }
        }
    }
}
