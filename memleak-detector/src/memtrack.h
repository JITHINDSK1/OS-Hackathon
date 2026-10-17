/*
 * memtrack.h
 * ----------
 * Public header for the Memory Leak Detection library.
 *
 * HOW IT WORKS (the macro trick):
 *   Every call to malloc/calloc/realloc/free in user code is transparently
 *   replaced by our tracked_ variants via C preprocessor macros.
 *   Each tracked variant records the file name and line number of the call,
 *   so the final leak report tells you *exactly* where a leaked block was
 *   allocated.
 *
 *   Inside memtrack.c we #undef these macros so the library itself can
 *   still call the real libc functions without infinite recursion.
 *
 * USAGE:
 *   Simply #include "memtrack.h" at the top of any .c file you want to track.
 *   The report is printed automatically at program exit (via atexit).
 *   You can also call memtrack_report() manually at any point.
 */

#ifndef MEMTRACK_H
#define MEMTRACK_H

#include <stddef.h>   /* size_t */

/* ------------------------------------------------------------------ */
/*  Tracked allocation functions – declared here, defined in           */
/*  memtrack.c.  User code should NOT call these directly; use the     */
/*  macros below so that __FILE__ and __LINE__ are captured correctly. */
/* ------------------------------------------------------------------ */

void *tracked_malloc (size_t size,          const char *file, int line);
void *tracked_calloc (size_t nmemb, size_t size, const char *file, int line);
void *tracked_realloc(void *ptr,  size_t size, const char *file, int line);
void  tracked_free   (void *ptr,             const char *file, int line);

/* ------------------------------------------------------------------ */
/*  Macro redefinitions                                                 */
/*  These replace every malloc/calloc/realloc/free in the including    */
/*  translation unit with their tracked counterparts.                  */
/* ------------------------------------------------------------------ */

#define malloc(size)          tracked_malloc ((size),         __FILE__, __LINE__)
#define calloc(nmemb, size)   tracked_calloc ((nmemb), (size),__FILE__, __LINE__)
#define realloc(ptr, size)    tracked_realloc((ptr),   (size),__FILE__, __LINE__)
#define free(ptr)             tracked_free   ((ptr),          __FILE__, __LINE__)

/* ------------------------------------------------------------------ */
/*  Manual report trigger                                               */
/*  Call this if you want the report before program exit, or from a    */
/*  signal handler.  It is also called automatically via atexit().     */
/* ------------------------------------------------------------------ */

void memtrack_report(void);

/* ------------------------------------------------------------------ */
/*  Test/diagnostic stats accessor                                      */
/*  Fills the four output pointers with a live snapshot of the         */
/*  internal allocation counters.  Primarily for the unit test suite.  */
/* ------------------------------------------------------------------ */

void memtrack_get_stats(size_t *allocs, size_t *frees,
                        size_t *current, size_t *peak);

#endif /* MEMTRACK_H */
