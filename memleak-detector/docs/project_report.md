# Project Report: Memory Leak Detection and Reporting Tool

**Course / Event:** Operating Systems Hackathon  
**Language:** C (C11) | **Build:** GCC + GNU Make | **Platform:** Linux / WSL / macOS

---

## Abstract

Dynamic memory mismanagement is one of the most common and expensive classes of bugs in systems programming. This project presents **memtrack**, a lightweight compile-time memory leak detector for C programs. By exploiting the C preprocessor macro system, memtrack transparently intercepts all calls to `malloc`, `calloc`, `realloc`, and `free`, records metadata for every live allocation in a linked list, and produces a detailed diagnostic report at program exit — all without requiring any external tools, source annotation, or runtime modification.

---

## 1. Problem Statement

Memory leaks occur when a program allocates heap memory but never releases it. Over time this causes:

- Gradual increase in resident memory, eventually exhausting available RAM.
- Performance degradation as the OS swaps pages.
- Crashes in long-running servers or embedded systems.
- Security vulnerabilities via resource exhaustion.

Existing tools (Valgrind, AddressSanitizer) are powerful but heavyweight: they require separate installation, significantly slow execution, or rely on compiler instrumentation unavailable in constrained environments.

**Goal:** Build a minimal, portable, zero-dependency library that provides useful leak diagnostics with near-zero overhead and no external tooling.

---

## 2. Objectives

1. Intercept `malloc`, `calloc`, `realloc`, and `free` without modifying call sites beyond including one header.
2. Record per-allocation metadata: address, size, source file, and line number.
3. Detect and warn about double-free and invalid-free at runtime.
4. Produce a human-readable report at program exit showing all un-freed blocks.
5. Maintain thread safety using `pthread` mutexes.
6. Keep the implementation under 350 lines of C.

---

## 3. System Design

### 3.1 Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                      User Program                           │
│  #include "memtrack.h"                                      │
│                                                             │
│  malloc(64)  ──macro──►  tracked_malloc(64, file, line)     │
│  free(ptr)   ──macro──►  tracked_free(ptr, file, line)      │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                     memtrack.c                              │
│                                                             │
│  pthread_mutex_lock(&g_lock)                                │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │            Allocation Linked List                   │   │
│  │  [ptr|size|file|line] → [ptr|size|file|line] → NULL│   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  Statistics:  total_allocs  total_frees                     │
│               current_bytes peak_bytes                      │
│                                                             │
│  pthread_mutex_unlock(&g_lock)                              │
│                                                             │
│  atexit(memtrack_report) ──► fires at program exit          │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
          ┌────────────────────────────┐
          │      memtrack_report()     │
          │                           │
          │  Walk list → print leaks  │
          │  Print statistics table   │
          │  Save to leak_report.txt  │
          └────────────────────────────┘
```

### 3.2 Data Structure

```c
typedef struct AllocRecord {
    void              *ptr;   /* heap address              */
    size_t             size;  /* bytes requested            */
    const char        *file;  /* __FILE__ of allocation    */
    int                line;  /* __LINE__ of allocation    */
    struct AllocRecord *next; /* intrusive singly-linked   */
} AllocRecord;
```

A **singly-linked list** was chosen because:
- Insertions are O(1) (prepend to head).
- The list is walked only at program exit (O(n) is acceptable).
- Memory footprint is minimal: one 32-byte node per live allocation.

---

## 4. Implementation Details

### 4.1 The Macro Trick

`memtrack.h` uses `#define` to rename the four standard allocator symbols:

```c
#define malloc(size)  tracked_malloc((size), __FILE__, __LINE__)
```

`__FILE__` and `__LINE__` are **compiler-provided macros** that expand to the current source file name and line number at the point of each macro expansion. This gives us precise allocation provenance without any AST transformation or DWARF parsing.

Inside `memtrack.c`, the macros are `#undef`-ed before including `<stdlib.h>` so the implementation can call the real libc functions:

```c
#undef malloc
#undef calloc
#undef realloc
#undef free
#include <stdlib.h>
```

This prevents infinite recursion: `tracked_malloc` calling `malloc` would otherwise call `tracked_malloc` again.

### 4.2 Thread Safety

A single global `pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER` guards all accesses to the linked list and statistics counters. Every `tracked_*` function and `memtrack_report` acquires this lock for the duration of their list operation.

### 4.3 atexit Registration

On the **first** tracked allocation, `atexit(memtrack_report)` is called once (guarded by a `g_atexit_set` flag). This ensures the report fires automatically at normal program exit without requiring any cleanup code in `main()`.

### 4.4 Double-Free and Invalid-Free Detection

When `tracked_free` is called:
1. It calls `list_remove(ptr)` which searches the linked list.
2. If `ptr` is **not found** in the list, the pointer was either already freed or was never heap-allocated.
3. Instead of calling `free()` on an unknown pointer (which would be undefined behaviour), memtrack prints a `WARNING` to `stderr` and returns safely.

### 4.5 realloc Tracking

`tracked_realloc` handles three cases:
| Input | Behaviour |
|-------|-----------|
| `ptr == NULL` | Delegates to `tracked_malloc` |
| `size == 0` | Delegates to `tracked_free` |
| Normal case | Removes old record, calls real `realloc`, inserts new record with updated address and size |

---

## 5. Testing and Results

### 5.1 Unit Tests (`tests/test_memtrack.c`)

| Test | What it verifies | Result |
|------|-----------------|--------|
| `malloc_then_free` | alloc count +1, free count +1, current_bytes unchanged | PASS |
| `malloc_no_free_one_leak` | alloc count +1, free count +0, current_bytes +128 | PASS |
| `calloc_zero_initialisation` | all bytes are zero after calloc | PASS |
| `realloc_grow_preserves_data` | first 4 elements intact after grow | PASS |
| `realloc_shrink_updates_tracking` | current_bytes reflects 64, not 512 | PASS |
| `double_free_intercepted` | only 1 free counted, no crash | PASS |
| `invalid_free_intercepted` | stats unchanged, no crash | PASS |
| `peak_memory_high_water_mark` | peak ≥ c0+3000, never decreases | PASS |

### 5.2 Demo Results

**demo_leak:** Reports 3 leaked blocks with exact source locations, plus 2 WARNING lines for the double-free and invalid-free.

**demo_clean:** Reports "No memory leaks detected. All allocations were freed."

---

## 6. Limitations

- Only works when the program is recompiled with `memtrack.h` included.
- Cannot track allocations inside pre-built shared libraries.
- No stack unwinding (only the immediate call-site file:line is recorded).
- `atexit` is not invoked on `_exit()`, `abort()`, or fatal signals.
- A single global mutex may become a bottleneck under very high allocation rates in multi-threaded programs.

---

## 7. Conclusion

memtrack demonstrates that useful memory diagnostics can be implemented in under 350 lines of portable C11 using only language-standard facilities (preprocessor macros, `atexit`, POSIX threads). It is practical for student projects, embedded systems, and any environment where Valgrind or ASan is unavailable. The clean separation between the macro-based intercept layer (`.h`) and the implementation (`.c`) makes the design easy to understand, extend, and present.

---

## References

- ISO/IEC 9899:2011 (C11 Standard)
- `man 3 malloc`, `man 3 atexit`, `man 3 pthread_mutex_lock`
- Valgrind memcheck documentation (for conceptual comparison)
