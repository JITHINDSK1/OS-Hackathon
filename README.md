# Memory Leak Detection and Reporting Tool

A lightweight, zero-dependency C library that wraps the standard allocator calls (`malloc`, `calloc`, `realloc`, `free`) at compile time and reports every un-freed memory block when the program exits.

---

## Features

| Feature | Detail |
|---------|--------|
| **Leak detection** | Every un-freed block is reported with its address, size, and the exact file + line where it was allocated |
| **Double-free detection** | Warns instead of crashing when the same pointer is freed twice |
| **Invalid-free detection** | Catches attempts to free stack variables or other non-heap pointers |
| **realloc tracking** | Correctly updates the allocation record when a block is grown or shrunk |
| **Statistics** | Total allocations, total frees, leaked blocks/bytes, peak memory usage |
| **Dual output** | Report is printed to `stdout` **and** saved to `leak_report.txt` |
| **Thread safety** | A single `pthread_mutex` guards the allocation list |
| **Zero dependencies** | Only POSIX / libc; no Valgrind, ASan, or external libraries |
| **Auto-report** | `memtrack_report()` is registered with `atexit()` — no source changes needed beyond including the header |

---

## Folder Structure

```
memleak-detector/
├── src/
│   ├── memtrack.h          ← Public header (include this in your code)
│   └── memtrack.c          ← Library implementation
├── examples/
│   ├── demo_leak.c         ← Intentional leaks, double-free, invalid-free
│   └── demo_clean.c        ← All allocations correctly freed
├── tests/
│   └── test_memtrack.c     ← Unit tests (no framework, just asserts)
├── docs/
│   ├── project_report.md   ← Full academic-style project report
│   ├── PPT_outline.md      ← Slide-by-slide presentation outline
│   └── DEMO_SCRIPT.md      ← 3-minute live demo script
├── Makefile
├── README.md               ← This file
└── .gitignore
```

---

## Setup & Build

### Prerequisites
- GCC (any version supporting C11)
- GNU Make
- POSIX threads (`libpthread`, standard on Linux/macOS/WSL)

### Build everything
```bash
cd memleak-detector
make
```

### Run the demos
```bash
make run-leak     # Shows 3 leaks + 2 warnings
make run-clean    # Shows "No leaks detected"
```

### Run the unit tests
```bash
make test
```

### Clean build artefacts
```bash
make clean
```

---

## How It Works

### 1. The Macro Trick

`memtrack.h` redefines the four standard allocator names using C preprocessor macros:

```c
#define malloc(size)        tracked_malloc((size),         __FILE__, __LINE__)
#define calloc(nmemb, size) tracked_calloc((nmemb), (size),__FILE__, __LINE__)
#define realloc(ptr, size)  tracked_realloc((ptr),  (size),__FILE__, __LINE__)
#define free(ptr)           tracked_free((ptr),             __FILE__, __LINE__)
```

Any file that `#include "memtrack.h"` will automatically have every allocator call routed through the tracking layer — **no source changes required beyond that one include**.

Inside `memtrack.c`, all four macros are `#undef`-ed before including standard headers, so the library can call the **real** libc functions without infinite recursion.

### 2. The Linked List

Every live allocation is stored as a node in a singly-linked list:

```
[ptr | size | file | line | next] → [ptr | size | file | line | next] → NULL
```

- `tracked_malloc/calloc` → push a new node  
- `tracked_free`          → pop & free the node (or warn if not found)  
- `tracked_realloc`       → pop old node, push new node with updated address/size  

### 3. Automatic Report via atexit()

On the **first** tracked allocation, `memtrack_report()` is registered with `atexit()`. When the program exits normally, the C runtime calls this function, which walks the list and prints every node that still exists (= every leaked block).

---

## Sample Output

### demo_leak (3 leaks)

```
=== demo_leak: intentional memory problems ===

Allocated 'name'    at 0x55a1b2c3d010 (will be leaked)
Allocated 'numbers' at 0x55a1b2c3d080 (will be leaked)
Allocated 'buf'     at 0x55a1b2c3d0c0 (will be leaked)

--- Triggering double free on 'name' ---
[memtrack] WARNING: Invalid/double free of pointer 0x55a1b2c3d010 at demo_leak.c:43
--- Triggering invalid free of a stack variable ---
[memtrack] WARNING: Invalid/double free of pointer 0x7ffc... at demo_leak.c:48

╔══════════════════════════════════════════════════════════════╗
║          MEMORY LEAK DETECTION REPORT (memtrack)            ║
╚══════════════════════════════════════════════════════════════╝

  ✗  LEAKED BLOCKS DETECTED:

  Address             Bytes     Allocated at
  ------------------  --------  ----------------------------
  0x55a1b2c3d0c0      256       demo_leak.c:36
  0x55a1b2c3d080      40        demo_leak.c:30
  0x55a1b2c3d010      64        demo_leak.c:24   ← already freed once

┌─────────────────────────────────────────────┐
│                   SUMMARY                   │
├─────────────────────────────────────────────┤
│  Total allocations  : 3                     │
│  Total frees        : 1                     │
│  Leaked blocks      : 3                     │
│  Leaked bytes       : 360                   │
│  Peak memory usage  : 360    bytes          │
└─────────────────────────────────────────────┘

  [Report also saved to: leak_report.txt]
```

### demo_clean (no leaks)

```
  ✓  No memory leaks detected. All allocations were freed.

┌─────────────────────────────────────────────┐
│                   SUMMARY                   │
├─────────────────────────────────────────────┤
│  Total allocations  : 3                     │
│  Total frees        : 3                     │
│  Leaked blocks      : 0                     │
│  Leaked bytes       : 0                     │
│  Peak memory usage  : 640    bytes          │
└─────────────────────────────────────────────┘
```

---

## Team Roles

| Member | Role |
|--------|------|
| [Member 1 – Name] | Core library (`memtrack.c`, `memtrack.h`), architecture design |
| [Member 2 – Name] | Demo programs, Makefile, build system |
| [Member 3 – Name] | Unit tests (`test_memtrack.c`), validation |
| [Member 4 – Name] | Documentation (`README.md`, `project_report.md`, `PPT_outline.md`) |
| [Member 5 – Name] | Presentation, demo script, results analysis |

---

## File-by-File Explanation (for Presentations)

| File | What it does – in simple words |
|------|-------------------------------|
| `memtrack.h` | Think of it as a "spy" that intercepts every `malloc`/`free` call by renaming them via macros. |
| `memtrack.c` | The actual spy logic: keeps a list of all live allocations and checks if you forgot to free any. |
| `demo_leak.c` | A program that intentionally makes mistakes so we can prove our tool catches them. |
| `demo_clean.c` | A well-written program that proves our tool gives a clean bill of health when code is correct. |
| `test_memtrack.c` | Automated checks that run through every feature and print PASS/FAIL. |
| `Makefile` | A single command (`make`) that builds and runs everything. |

---

## Limitations

- Works only when source is **recompiled** with `memtrack.h` included (not transparent to pre-built binaries).
- Cannot capture allocations inside third-party shared libraries.
- No stack traces (only file:line of the allocation call site).
- `atexit` report fires only on normal exit — not after `_exit()`, `abort()`, or signals.

---

## Future Work

- **Stack traces** using `backtrace()` / `libunwind`
- **LD_PRELOAD mode** for transparent interception without recompilation
- **JSON/HTML report export** for CI integration
- **GUI dashboard** showing allocation timelines
- **Per-thread tracking** with coloured output per thread ID
