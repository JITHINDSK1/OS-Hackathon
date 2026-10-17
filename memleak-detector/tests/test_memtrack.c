/*
 * test_memtrack.c
 * ---------------
 * Simple assert-based unit tests for the memtrack library.
 * No external test framework is used – just standard C and our own
 * PASS/FAIL macros.
 *
 * Each test is an isolated function that exercises one feature and
 * prints PASS or FAIL.  A final summary shows total pass/fail counts.
 *
 * Run via:  make test
 *
 * IMPORTANT: memtrack registers atexit() on first allocation, which
 * will print the report at exit.  That is expected behaviour.
 * All allocations in the tests are freed before the test returns so
 * the final report should show 0 leaks.
 *
 * HOW TESTS WORK:
 *   Because the macro wrappers redirect every malloc/free call,
 *   we can inspect the internal counters and linked list by calling
 *   memtrack_get_stats() which we expose only for testing.
 */

/* Pull in the real stdlib first, then override with memtrack macros */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "memtrack.h"

/* ------------------------------------------------------------------ */
/*  Test infrastructure                                                 */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST_PASS(name) \
    do { printf("  [PASS] %s\n", (name)); g_pass++; } while (0)

#define TEST_FAIL(name, reason) \
    do { printf("  [FAIL] %s – %s\n", (name), (reason)); g_fail++; } while (0)

#define CHECK(name, cond) \
    do { if (cond) TEST_PASS(name); else TEST_FAIL(name, #cond " is false"); } while (0)

/* ------------------------------------------------------------------ */
/*  memtrack internal stats accessor                                    */
/*  We expose these via a thin struct to avoid coupling to internals.  */
/* ------------------------------------------------------------------ */

/* Declared in memtrack.c and accessible here for tests only */
void memtrack_get_stats(size_t *allocs, size_t *frees,
                        size_t *current, size_t *peak);

/* ------------------------------------------------------------------ */
/*  Individual tests                                                    */
/* ------------------------------------------------------------------ */

/*
 * test_malloc_free:
 *   A single malloc followed by a free should leave no leak.
 *   current_bytes should return to 0.
 */
static void test_malloc_free(void) {
    const char *name = "malloc_then_free";
    size_t a0, f0, c0, p0;
    memtrack_get_stats(&a0, &f0, &c0, &p0);

    void *p = malloc(64);
    if (!p) { TEST_FAIL(name, "malloc returned NULL"); return; }
    free(p);

    size_t a1, f1, c1, p1;
    memtrack_get_stats(&a1, &f1, &c1, &p1);

    /* Exactly one more alloc and one more free since baseline */
    CHECK(name, (a1 - a0) == 1 && (f1 - f0) == 1 && c1 == c0);
}

/*
 * test_malloc_no_free:
 *   malloc without free = 1 live block.
 *   We free it at the end to keep the final report clean.
 */
static void test_malloc_no_free(void) {
    const char *name = "malloc_no_free_one_leak";
    size_t a0, f0, c0, p0;
    memtrack_get_stats(&a0, &f0, &c0, &p0);

    void *p = malloc(128);
    if (!p) { TEST_FAIL(name, "malloc returned NULL"); return; }

    size_t a1, f1, c1, p1;
    memtrack_get_stats(&a1, &f1, &c1, &p1);

    /* One extra alloc, no extra free, 128 more bytes in use */
    int ok = (a1 - a0) == 1 && (f1 - f0) == 0 && (c1 - c0) == 128;
    if (ok) TEST_PASS(name); else TEST_FAIL(name, "stats mismatch");

    /* Clean up so we don't actually leak */
    free(p);
}

/*
 * test_calloc_zeroed:
 *   calloc must zero-initialise the memory.
 */
static void test_calloc_zeroed(void) {
    const char *name = "calloc_zero_initialisation";

    int *arr = calloc(16, sizeof(int));
    if (!arr) { TEST_FAIL(name, "calloc returned NULL"); return; }

    int all_zero = 1;
    for (int i = 0; i < 16; i++) {
        if (arr[i] != 0) { all_zero = 0; break; }
    }

    free(arr);
    CHECK(name, all_zero);
}

/*
 * test_realloc_grow:
 *   realloc to a larger size; old data must be preserved.
 */
static void test_realloc_grow(void) {
    const char *name = "realloc_grow_preserves_data";

    int *p = malloc(4 * sizeof(int));
    if (!p) { TEST_FAIL(name, "malloc returned NULL"); return; }
    p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 4;

    p = realloc(p, 8 * sizeof(int));
    if (!p) { TEST_FAIL(name, "realloc returned NULL"); return; }

    /* First 4 elements should still be intact */
    int ok = (p[0] == 1 && p[1] == 2 && p[2] == 3 && p[3] == 4);
    free(p);
    CHECK(name, ok);
}

/*
 * test_realloc_shrink:
 *   realloc to a smaller size; tracked size should decrease.
 */
static void test_realloc_shrink(void) {
    const char *name = "realloc_shrink_updates_tracking";

    size_t a0, f0, c0, p0;
    memtrack_get_stats(&a0, &f0, &c0, &p0);

    char *p = malloc(512);
    if (!p) { TEST_FAIL(name, "malloc returned NULL"); return; }

    p = realloc(p, 64);
    if (!p) { TEST_FAIL(name, "realloc returned NULL"); return; }

    size_t a1, f1, c1, p1;
    memtrack_get_stats(&a1, &f1, &c1, &p1);

    /* current_bytes should reflect the 64-byte block, not the 512-byte one */
    int ok = (c1 - c0) == 64;
    free(p);
    if (ok) TEST_PASS(name); else TEST_FAIL(name, "current_bytes not updated after shrink");
}

/*
 * test_double_free:
 *   Freeing the same pointer twice should NOT crash – memtrack must
 *   intercept the second call and print a warning instead.
 *   We verify by ensuring no crash occurs and the free count does not
 *   double-count.
 */
static void test_double_free(void) {
    const char *name = "double_free_intercepted";

    size_t a0, f0, c0, p0;
    memtrack_get_stats(&a0, &f0, &c0, &p0);

    void *p = malloc(32);
    if (!p) { TEST_FAIL(name, "malloc returned NULL"); return; }

    free(p);               /* valid free  */
    free(p);               /* double free – should warn, not crash */

    size_t a1, f1, c1, p1;
    memtrack_get_stats(&a1, &f1, &c1, &p1);

    /* Only one successful free should be counted */
    int ok = (a1 - a0) == 1 && (f1 - f0) == 1;
    if (ok) TEST_PASS(name);
    else    TEST_FAIL(name, "free count wrong after double free");
}

/*
 * test_invalid_free:
 *   Freeing a pointer that was never malloc'd (a stack address).
 *   memtrack must warn and not crash or corrupt state.
 */
static void test_invalid_free(void) {
    const char *name = "invalid_free_intercepted";

    size_t a0, f0, c0, p0;
    memtrack_get_stats(&a0, &f0, &c0, &p0);

    int stack_var = 99;
    free(&stack_var);  /* invalid – should warn, not crash */

    size_t a1, f1, c1, p1;
    memtrack_get_stats(&a1, &f1, &c1, &p1);

    /* Stats should be unchanged – no alloc, no free recorded */
    int ok = (a1 == a0) && (f1 == f0) && (c1 == c0);
    if (ok) TEST_PASS(name);
    else    TEST_FAIL(name, "stats changed after invalid free");
}

/*
 * test_peak_memory:
 *   Allocate two blocks, check peak, free one, check peak hasn't dropped.
 */
static void test_peak_memory(void) {
    const char *name = "peak_memory_high_water_mark";

    size_t a0, f0, c0, peak_before;
    memtrack_get_stats(&a0, &f0, &c0, &peak_before);

    void *p1 = malloc(1000);
    void *p2 = malloc(2000);

    size_t a1, f1, c1, peak_after_alloc;
    memtrack_get_stats(&a1, &f1, &c1, &peak_after_alloc);

    free(p1);

    size_t a2, f2, c2, peak_after_free;
    memtrack_get_stats(&a2, &f2, &c2, &peak_after_free);

    /* Peak should have grown to at least c0+3000, and must not shrink */
    int ok = (peak_after_alloc >= peak_before + 3000)
          && (peak_after_free  >= peak_after_alloc);
    free(p2);

    if (ok) TEST_PASS(name);
    else    TEST_FAIL(name, "peak_bytes not tracking high-water mark correctly");
}

/* ------------------------------------------------------------------ */
/*  Main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║        memtrack – Unit Test Suite            ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    test_malloc_free();
    test_malloc_no_free();
    test_calloc_zeroed();
    test_realloc_grow();
    test_realloc_shrink();
    test_double_free();
    test_invalid_free();
    test_peak_memory();

    printf("\n");
    printf("┌──────────────────────────────────────────────┐\n");
    printf("│  Results: %d PASSED, %d FAILED               \n",
           g_pass, g_fail);
    printf("└──────────────────────────────────────────────┘\n");

    return (g_fail == 0) ? 0 : 1;
}
