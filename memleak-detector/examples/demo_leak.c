/*
 * demo_leak.c
 * -----------
 * Demonstration program that INTENTIONALLY has memory problems:
 *
 *   1. Leaks 3 blocks (malloc'd but never freed).
 *   2. Does a double free (frees the same pointer twice).
 *   3. Does an invalid free (frees a stack variable address).
 *
 * Purpose: show that memtrack catches and reports all of these issues.
 *
 * Expected output:
 *   - 3 leaked blocks in the leak table.
 *   - 2 WARNING lines for double-free / invalid-free printed to stderr.
 *   - Summary shows leaked_blocks = 3.
 */

#include <stdio.h>
#include <string.h>
#include "memtrack.h"   /* <-- enables tracking; must come after stdio */

int main(void) {
    printf("=== demo_leak: intentional memory problems ===\n\n");

    /* ---- Leak 1: a plain malloc, never freed ---- */
    char *name = malloc(64);
    if (name) {
        strcpy(name, "Hello, memtrack!");
        printf("Allocated 'name' at %p (will be leaked)\n", (void *)name);
    }

    /* ---- Leak 2: calloc for an integer array, never freed ---- */
    int *numbers = calloc(10, sizeof(int));
    if (numbers) {
        numbers[0] = 42;
        printf("Allocated 'numbers' at %p (will be leaked)\n", (void *)numbers);
    }

    /* ---- Leak 3: malloc for a buffer, never freed ---- */
    void *buf = malloc(256);
    if (buf) {
        printf("Allocated 'buf' at %p (will be leaked)\n", (void *)buf);
    }

    /* ---- Double free: free 'name' twice ---- */
    printf("\n--- Triggering double free on 'name' ---\n");
    free(name);           /* first free  – valid  */
    free(name);           /* second free – double free! memtrack will warn */

    /* ---- Invalid free: try to free a stack variable ---- */
    printf("--- Triggering invalid free of a stack variable ---\n");
    int stack_var = 99;
    free(&stack_var);     /* NOT a heap pointer – memtrack will warn */

    printf("\n(Program ending – memtrack_report will fire via atexit)\n\n");
    return 0;
}
