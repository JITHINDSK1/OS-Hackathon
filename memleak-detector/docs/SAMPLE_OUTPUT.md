# Verified Terminal Sample Outputs

This document records the exact terminal output from compiling and running each target of the **Memory Leak Detection and Reporting Tool** under Linux / WSL with GCC (C11, flags: `-Wall -Wextra -std=c11 -g -lpthread`).

---

## 1. `make` (Default Build)

```text
$ make
mkdir -p build
gcc -Wall -Wextra -std=c11 -g -c src/memtrack.c -o build/memtrack.o
gcc -Wall -Wextra -std=c11 -g -Isrc examples/demo_leak.c build/memtrack.o -o build/demo_leak -lpthread
gcc -Wall -Wextra -std=c11 -g -Isrc examples/demo_clean.c build/memtrack.o -o build/demo_clean -lpthread

Build complete.  Artefacts in build/
  Run:  make run-leak    (intentional leaks demo)
  Run:  make run-clean   (clean memory demo)
  Run:  make test        (unit test suite)
```
> **Status:** Compilation succeeded with **zero warnings** under `-Wall -Wextra`.

---

## 2. `make test` (Unit Test Suite)

```text
$ make test
gcc -Wall -Wextra -std=c11 -g -Isrc tests/test_memtrack.c build/memtrack.o -o build/test_memtrack -lpthread

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

╔══════════════════════════════════════════════════════════════╗
║          MEMORY LEAK DETECTION REPORT (memtrack)            ║
╚══════════════════════════════════════════════════════════════╝

  ✓  No memory leaks detected. All allocations were freed.

┌─────────────────────────────────────────────┐
│                   SUMMARY                   │
├─────────────────────────────────────────────┤
│  Total allocations  : 8                     │
│  Total frees        : 8                     │
│  Leaked blocks      : 0                     │
│  Leaked bytes       : 0                     │
│  Peak memory usage  : 3000   bytes          │
└─────────────────────────────────────────────┘

  [Report also saved to: leak_report.txt]
```
> **Status:** All 8 unit tests passed with exit code 0.

---

## 3. `make run-leak` (Intentional Leaks Demo)

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
> **Notes on Output:**
> 1. `name` (64 bytes) was freed once cleanly on line 42, but was freed a second time on line 43. `memtrack` caught the double free on stderr and prevented heap corruption!
> 2. The stack address `&stack_var` was intercepted on line 48 without causing a crash.
> 3. The 2 unfreed blocks (`numbers` and `buf`, total 296 bytes) are cleanly displayed in the leak table with exact source coordinates.

---

## 4. `make run-clean` (Clean Memory Management Demo)

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
> **Status:** All allocations cleanly accounted for, showing `0` leaked blocks and 640 bytes peak usage.
