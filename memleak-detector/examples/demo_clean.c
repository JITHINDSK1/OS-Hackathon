/*
 * demo_clean.c
 * ------------
 * Demonstration program with ZERO memory problems.
 *
 * Every allocation is matched with a corresponding free.
 * The realloc path is also exercised.
 *
 * Expected output:
 *   - "No memory leaks detected. All allocations were freed."
 *   - Summary: leaked_blocks = 0, leaked_bytes = 0.
 */

#include <stdio.h>
#include <string.h>
#include "memtrack.h"   /* <-- enables tracking */

int main(void) {
    printf("=== demo_clean: correct memory management ===\n\n");

    /* ---- malloc + free ---- */
    char *msg = malloc(128);
    if (msg) {
        strcpy(msg, "memtrack is watching...");
        printf("msg = \"%s\"\n", msg);
        free(msg);
        printf("'msg' freed correctly.\n");
    }

    /* ---- calloc: array of doubles, zero-initialised ---- */
    double *scores = calloc(5, sizeof(double));
    if (scores) {
        for (int i = 0; i < 5; i++) {
            scores[i] = (double)(i + 1) * 1.5;
        }
        printf("scores[0..4] = %.1f %.1f %.1f %.1f %.1f\n",
               scores[0], scores[1], scores[2], scores[3], scores[4]);
        free(scores);
        printf("'scores' freed correctly.\n");
    }

    /* ---- realloc: grow then shrink ---- */
    int *arr = malloc(4 * sizeof(int));
    if (arr) {
        arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40;
        printf("arr initial size: 4 ints\n");

        /* grow to 8 ints */
        arr = realloc(arr, 8 * sizeof(int));
        if (arr) {
            arr[4] = 50; arr[5] = 60; arr[6] = 70; arr[7] = 80;
            printf("arr after grow: 8 ints\n");

            /* shrink back to 2 ints */
            arr = realloc(arr, 2 * sizeof(int));
            if (arr) {
                printf("arr after shrink: 2 ints = %d, %d\n", arr[0], arr[1]);
                free(arr);
                printf("'arr' freed correctly.\n");
            }
        }
    }

    printf("\n(Program ending – memtrack_report will fire via atexit)\n\n");
    return 0;
}
