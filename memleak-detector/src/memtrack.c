/*
 * memtrack.c
 * ----------
 * Implementation of the Memory Leak Detection library.
 *
 * DESIGN:
 *   - A singly-linked list (g_head) stores one record per live allocation.
 *   - We #undef the malloc/calloc/realloc/free macros first so this file
 *     can call the REAL libc functions without recursing into itself.
 *   - A pthread mutex (g_lock) guards the list for thread safety.
 *   - On the very first tracked allocation, memtrack_report() is registered
 *     with atexit() so the report is always printed at program exit.
 *   - The report is written both to stdout and to "leak_report.txt".
 *
 * WHAT IS TRACKED PER RECORD:
 *   ptr   – the allocated address
 *   size  – bytes requested by the user
 *   file  – source file where the allocation happened (__FILE__)
 *   line  – source line number (__LINE__)
 *
 * STATISTICS:
 *   total_allocs   – number of tracked_malloc/calloc/realloc-new calls
 *   total_frees    – number of successful tracked_free calls
 *   current_bytes  – bytes currently in use (live allocations)
 *   peak_bytes     – high-water mark of current_bytes
 *   leaked_bytes   – bytes still allocated when report() is called
 */

/* ------------------------------------------------------------------ */
/*  IMPORTANT: undefine the macros from memtrack.h BEFORE including    */
/*  any standard headers so that our internal calls use real libc.     */
/* ------------------------------------------------------------------ */
#ifdef malloc
#  undef malloc
#endif
#ifdef calloc
#  undef calloc
#endif
#ifdef realloc
#  undef realloc
#endif
#ifdef free
#  undef free
#endif

/* ------------------------------------------------------------------ */
/*  Standard includes (using real libc symbols)                        */
/* ------------------------------------------------------------------ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Include our own header AFTER undefining macros so the function      */
/* declarations are visible but the macros don't affect this file.     */
#include "memtrack.h"

/* Re-undef after including the header in case the header was already  */
/* included transitively.                                               */
#undef malloc
#undef calloc
#undef realloc
#undef free

/* ------------------------------------------------------------------ */
/*  Internal data structures                                            */
/* ------------------------------------------------------------------ */

/* One node in the linked list of live allocations */
typedef struct AllocRecord {
    void              *ptr;   /* allocated address           */
    size_t             size;  /* bytes requested             */
    const char        *file;  /* source file of allocation   */
    int                line;  /* source line of allocation   */
    struct AllocRecord *next; /* next record in the list     */
} AllocRecord;

/* ------------------------------------------------------------------ */
/*  Global state                                                        */
/* ------------------------------------------------------------------ */

static AllocRecord  *g_head         = NULL;   /* head of linked list  */
static pthread_mutex_t g_lock       = PTHREAD_MUTEX_INITIALIZER;
static int           g_atexit_set   = 0;      /* atexit registered?   */

/* Statistics */
static size_t g_total_allocs  = 0;
static size_t g_total_frees   = 0;
static size_t g_current_bytes = 0;
static size_t g_peak_bytes    = 0;

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Update peak_bytes whenever current_bytes changes upward */
static void update_peak(void) {
    if (g_current_bytes > g_peak_bytes) {
        g_peak_bytes = g_current_bytes;
    }
}

/* Add a new record to the front of the list.
 * Caller must hold g_lock.                                             */
static void list_add(void *ptr, size_t size, const char *file, int line) {
    AllocRecord *rec = (AllocRecord *)malloc(sizeof(AllocRecord));
    if (!rec) {
        fprintf(stderr, "[memtrack] FATAL: cannot allocate record node\n");
        return;
    }
    rec->ptr  = ptr;
    rec->size = size;
    rec->file = file;
    rec->line = line;
    rec->next = g_head;
    g_head    = rec;

    g_total_allocs++;
    g_current_bytes += size;
    update_peak();
}

/* Remove the record for 'ptr' from the list.
 * Returns the freed record (caller must free it), or NULL if not found.
 * Caller must hold g_lock.                                             */
static AllocRecord *list_remove(void *ptr) {
    AllocRecord *prev = NULL;
    AllocRecord *cur  = g_head;

    while (cur) {
        if (cur->ptr == ptr) {
            if (prev) {
                prev->next = cur->next;
            } else {
                g_head = cur->next;
            }
            cur->next = NULL;
            return cur;
        }
        prev = cur;
        cur  = cur->next;
    }
    return NULL; /* not found */
}



/* ------------------------------------------------------------------ */
/*  Public API: tracked allocation wrappers                             */
/* ------------------------------------------------------------------ */

/*
 * tracked_malloc – wrapper around libc malloc.
 * Records the allocation in the list.
 */
void *tracked_malloc(size_t size, const char *file, int line) {
    void *ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "[memtrack] WARNING: malloc(%zu) returned NULL at %s:%d\n",
                size, file, line);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);

    /* Register atexit report on very first allocation */
    if (!g_atexit_set) {
        atexit(memtrack_report);
        g_atexit_set = 1;
    }

    list_add(ptr, size, file, line);
    pthread_mutex_unlock(&g_lock);
    return ptr;
}

/*
 * tracked_calloc – wrapper around libc calloc.
 * calloc zeroes the memory; we record it just like malloc.
 */
void *tracked_calloc(size_t nmemb, size_t size, const char *file, int line) {
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        fprintf(stderr, "[memtrack] WARNING: calloc(%zu, %zu) returned NULL at %s:%d\n",
                nmemb, size, file, line);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);

    if (!g_atexit_set) {
        atexit(memtrack_report);
        g_atexit_set = 1;
    }

    list_add(ptr, nmemb * size, file, line);
    pthread_mutex_unlock(&g_lock);
    return ptr;
}

/*
 * tracked_realloc – wrapper around libc realloc.
 * If ptr is NULL this behaves like malloc.
 * If size is 0 this behaves like free.
 * Otherwise the existing record is updated with the new address & size.
 */
void *tracked_realloc(void *ptr, size_t size, const char *file, int line) {
    /* realloc(NULL, size) == malloc(size) */
    if (ptr == NULL) {
        return tracked_malloc(size, file, line);
    }

    /* realloc(ptr, 0) == free(ptr) on most platforms */
    if (size == 0) {
        tracked_free(ptr, file, line);
        return NULL;
    }

    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        fprintf(stderr, "[memtrack] WARNING: realloc(%p, %zu) returned NULL at %s:%d\n",
                ptr, size, file, line);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);

    AllocRecord *rec = list_remove(ptr);
    if (!rec) {
        fprintf(stderr,
            "[memtrack] WARNING: realloc on untracked pointer %p at %s:%d\n",
            ptr, file, line);
    } else {
        /* Adjust statistics for size change */
        g_current_bytes -= rec->size;
        free(rec);  /* free the old record node */
    }

    /* Add a fresh record for the new pointer */
    list_add(new_ptr, size, file, line);
    /* list_add already incremented g_total_allocs; compensate so that a
       realloc counts as neither an extra alloc nor an extra free.        */
    g_total_allocs--;

    pthread_mutex_unlock(&g_lock);
    return new_ptr;
}

/*
 * tracked_free – wrapper around libc free.
 * Detects double-free and invalid-free (pointer never allocated).
 */
void tracked_free(void *ptr, const char *file, int line) {
    /* free(NULL) is always a no-op; that is correct per the C standard */
    if (ptr == NULL) {
        return;
    }

    pthread_mutex_lock(&g_lock);

    AllocRecord *rec = list_remove(ptr);
    if (!rec) {
        /* Pointer is not in the live list – could be double-free or invalid */
        fprintf(stderr,
            "[memtrack] WARNING: Invalid/double free of pointer %p at %s:%d\n",
            ptr, file, line);
        pthread_mutex_unlock(&g_lock);
        return;  /* do NOT call free(); the block may already be freed */
    }

    g_total_frees++;
    g_current_bytes -= rec->size;
    free(rec);  /* free the record node */

    pthread_mutex_unlock(&g_lock);
    free(ptr);  /* free the actual user block */
}

/* ------------------------------------------------------------------ */
/*  Stats accessor (for unit tests)                                     */
/* ------------------------------------------------------------------ */

/*
 * memtrack_get_stats – fills the four output pointers with a snapshot
 * of the current internal counters.  Intended for use by test code only.
 */
void memtrack_get_stats(size_t *allocs, size_t *frees,
                        size_t *current, size_t *peak) {
    pthread_mutex_lock(&g_lock);
    if (allocs)  *allocs  = g_total_allocs;
    if (frees)   *frees   = g_total_frees;
    if (current) *current = g_current_bytes;
    if (peak)    *peak    = g_peak_bytes;
    pthread_mutex_unlock(&g_lock);
}

/* ------------------------------------------------------------------ */
/*  Report                                                              */
/* ------------------------------------------------------------------ */

/*
 * memtrack_report – print a full leak report to stdout and leak_report.txt.
 * This is registered with atexit() and is also callable manually.
 *
 * OUTPUT FORMAT:
 *   A bordered table of leaked blocks (if any), followed by a summary
 *   section with allocation statistics.
 */
void memtrack_report(void) {
    pthread_mutex_lock(&g_lock);

    /* Count leaks */
    size_t leaked_blocks = 0;
    size_t leaked_bytes  = 0;
    AllocRecord *cur = g_head;
    while (cur) {
        leaked_blocks++;
        leaked_bytes += cur->size;
        cur = cur->next;
    }

    /* Open the report file */
    FILE *report_fp = fopen("leak_report.txt", "w");

    /* Helper lambda-like macro to print to both stdout and file */
#define RPRINT(fmt, ...) \
    do { \
        printf(fmt, ##__VA_ARGS__); \
        if (report_fp) fprintf(report_fp, fmt, ##__VA_ARGS__); \
    } while (0)

    RPRINT("\n");
    RPRINT("╔══════════════════════════════════════════════════════════════╗\n");
    RPRINT("║          MEMORY LEAK DETECTION REPORT (memtrack)            ║\n");
    RPRINT("╚══════════════════════════════════════════════════════════════╝\n");
    RPRINT("\n");

    if (leaked_blocks == 0) {
        RPRINT("  ✓  No memory leaks detected. All allocations were freed.\n");
    } else {
        RPRINT("  ✗  LEAKED BLOCKS DETECTED:\n");
        RPRINT("\n");
        RPRINT("  %-18s  %-8s  %s\n", "Address", "Bytes", "Allocated at");
        RPRINT("  %-18s  %-8s  %s\n",
               "------------------", "--------", "----------------------------");

        cur = g_head;
        while (cur) {
            RPRINT("  %-18p  %-8zu  %s:%d\n",
                   cur->ptr, cur->size, cur->file, cur->line);
            cur = cur->next;
        }
    }

    RPRINT("\n");
    RPRINT("┌─────────────────────────────────────────────┐\n");
    RPRINT("│                   SUMMARY                   │\n");
    RPRINT("├─────────────────────────────────────────────┤\n");
    RPRINT("│  Total allocations  : %-6zu                │\n", g_total_allocs);
    RPRINT("│  Total frees        : %-6zu                │\n", g_total_frees);
    RPRINT("│  Leaked blocks      : %-6zu                │\n", leaked_blocks);
    RPRINT("│  Leaked bytes       : %-6zu                │\n", leaked_bytes);
    RPRINT("│  Peak memory usage  : %-6zu bytes          │\n", g_peak_bytes);
    RPRINT("└─────────────────────────────────────────────┘\n");
    RPRINT("\n");

    if (report_fp) {
        fclose(report_fp);
        printf("  [Report also saved to: leak_report.txt]\n\n");
    }

#undef RPRINT

    pthread_mutex_unlock(&g_lock);
}
