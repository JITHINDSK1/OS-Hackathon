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

## Sample Output (Real Terminal Runs)

See full logs in [`docs/SAMPLE_OUTPUT.md`](file:///d:/OS%20PROJECT/memleak-detector/docs/SAMPLE_OUTPUT.md).

### demo_leak (`make run-leak`)

```text
$ make run-leak

=== Running demo_leak ===
./build/demo_leak
=== demo_leak: intentional memory problems ===

Allocated 'name' at 0x55a1004162a0 (will be leaked)
Allocated 'numbers' at 0x55a100416300 (will be leaked)
Allocated 'buf' at 0x55a100416340 (will be leaked)

--- Triggering double free on 'name' ---
[memtrack] WARNING: Invalid/double free of pointer 0x55a1004162a0 at examples/demo_leak.c:43
--- Triggering invalid free of a stack variable ---
[memtrack] WARNING: Invalid/double free of pointer 0x7ffd5102abac at examples/demo_leak.c:48

(Program ending – memtrack_report will fire via atexit)

╔══════════════════════════════════════════════════════════════╗
║          MEMORY LEAK DETECTION REPORT (memtrack)            ║
╚══════════════════════════════════════════════════════════════╝

  ✗  LEAKED BLOCKS DETECTED:

  Address             Bytes     Allocated at
  ------------------  --------  ----------------------------
  0x55a100416340      256       examples/demo_leak.c:36
  0x55a100416300      40        examples/demo_leak.c:30

┌─────────────────────────────────────────────┐
│                   SUMMARY                   │
├─────────────────────────────────────────────┤
│  Total allocations  : 3                     │
│  Total frees        : 1                     │
│  Leaked blocks      : 2                     │
│  Leaked bytes       : 296                   │
│  Peak memory usage  : 360    bytes          │
└─────────────────────────────────────────────┘

  [Report also saved to: leak_report.txt]
```

### demo_clean (`make run-clean`)

```text
$ make run-clean

=== Running demo_clean ===
./build/demo_clean
=== demo_clean: correct memory management ===

msg = "memtrack is watching..."
'msg' freed correctly.
scores[0..4] = 1.5 3.0 4.5 6.0 7.5
'scores' freed correctly.
arr initial size: 4 ints
arr after grow: 8 ints
arr after shrink: 2 ints = 10, 20
'arr' freed correctly.

(Program ending – memtrack_report will fire via atexit)

╔══════════════════════════════════════════════════════════════╗
║          MEMORY LEAK DETECTION REPORT (memtrack)            ║
╚══════════════════════════════════════════════════════════════╝

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

  [Report also saved to: leak_report.txt]
```

### unit tests (`make test`)

```text
=== Running unit tests ===
./build/test_memtrack

╔══════════════════════════════════════════════╗
║        memtrack – Unit Test Suite            ║
╚══════════════════════════════════════════════╝

  [PASS] malloc_then_free
  [PASS] malloc_no_free_one_leak
  [PASS] calloc_zero_initialisation
  [PASS] realloc_grow_preserves_data
  [PASS] realloc_shrink_updates_tracking
[memtrack] WARNING: Invalid/double free of pointer 0x55a1004164a0 at tests/test_memtrack.c:166
  [PASS] double_free_intercepted
[memtrack] WARNING: Invalid/double free of pointer 0x7ffd5102abac at tests/test_memtrack.c:185
  [PASS] invalid_free_intercepted
  [PASS] peak_memory_high_water_mark

┌──────────────────────────────────────────────┐
│  Results: 8 PASSED, 0 FAILED                 │
└──────────────────────────────────────────────┘
```

---

## Interactive Web Simulator & GitHub Pages

A companion interactive web dashboard and live simulator is located in the `/website` folder.

### Enabling GitHub Pages:
1. Navigate to your repository on GitHub: `https://github.com/JITHINDSK1/OS-Hackathon`.
2. Go to **Settings** > **Pages** (under Code and automation).
3. Under **Build and deployment** > **Branch**:
   - Select branch: `main`.
   - Select folder: `/website` (or root `/` if deployed via custom workflow).
4. Click **Save**. Your site will be published at `https://jithindsk1.github.io/OS-Hackathon/`.

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
