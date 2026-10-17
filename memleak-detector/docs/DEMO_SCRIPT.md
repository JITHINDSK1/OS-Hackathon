# Live Demo Script – Memory Leak Detection Tool

**Target duration:** 3 minutes  
**Presenter:** [Your Name]  
**Setup:** Terminal open, `cd memleak-detector`, WSL/Linux

---

## Before You Start (Preparation)

```bash
cd memleak-detector
make clean
```
Make sure the terminal is visible to the audience (increase font size to 18pt+).

---

## [0:00 – 0:30] Introduction (30 seconds)

> *"Hi everyone. We built a memory leak detector in C — from scratch, in under 350 lines, with zero external dependencies. Let me show you what it does in real time."*

```bash
# Show the folder structure
ls -R
```

> *"You can see we have the core library in `src/`, two demo programs in `examples/`, and a unit test suite in `tests/`. Everything is built with a single `make` command."*

---

## [0:30 – 1:00] Build (30 seconds)

```bash
make
```

> *"GCC compiles with `-Wall -Wextra` — zero warnings. Let me show you the trick that makes this work."*

```bash
cat src/memtrack.h | head -50
```

> *"These four `#define` lines are the heart of the tool. Every `malloc` call in user code becomes `tracked_malloc`, which records the file name and line number. The user doesn't change a single line of their code — they just add one `#include`."*

---

## [1:00 – 1:45] Demo: Intentional Leaks (45 seconds)

```bash
make run-leak
```

> *"demo_leak intentionally creates three memory leaks, a double-free, and an invalid free. Watch the output..."*

*(Point to the WARNING lines)*

> *"These two warnings — for the double-free and invalid free — are printed immediately, because memtrack catches them at the call site."*

*(Point to the leak table)*

> *"And here at program exit — via `atexit()` — we see the three leaked blocks: their exact addresses, sizes, and the file and line where they were allocated. No Valgrind. No special compiler flags. Just one `#include`."*

---

## [1:45 – 2:15] Demo: Clean Program (30 seconds)

```bash
make run-clean
```

> *"demo_clean allocates memory correctly — malloc, realloc, calloc — and frees everything. The report says: No memory leaks detected. It also shows the peak memory usage was 640 bytes. This is your green light that the code is clean."*

---

## [2:15 – 2:50] Unit Tests (35 seconds)

```bash
make test
```

> *"We have 8 automated unit tests covering: leak detection, calloc zero-initialisation, realloc grow and shrink, double-free interception, invalid-free interception, and peak memory tracking. All 8 pass. `make test` returns exit code 0, so this can be plugged straight into a CI pipeline."*

---

## [2:50 – 3:00] Close (10 seconds)

```bash
cat leak_report.txt
```

> *"The report is also saved to `leak_report.txt` — useful for CI artefacts or sending to a teammate. That's memtrack: lightweight, portable, and powerful enough to catch real bugs. Thank you."*

---

## Backup Q&A Answers

**Q: Why not just use Valgrind?**  
A: Valgrind requires a separate install and slows execution 20–50×. memtrack adds near-zero overhead and works anywhere GCC does.

**Q: What if someone includes the header in memtrack.c itself?**  
A: We `#undef` all four macros at the very top of `memtrack.c` before including any headers. The library always calls real libc.

**Q: Does it work with multi-threaded programs?**  
A: Yes — all list operations are guarded by a `pthread_mutex`. Multiple threads can allocate and free concurrently without data corruption.

**Q: What about stack traces?**  
A: That's our top future-work item. We'd use `backtrace()` from `<execinfo.h>` to capture the full call chain per allocation.
