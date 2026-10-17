# PPT Outline – Memory Leak Detection and Reporting Tool

*Hackathon Presentation | Suggested: 8–10 slides | Time: 5–7 minutes*

---

## Slide 1 – Title

**Title:** Memory Leak Detection and Reporting Tool  
**Subtitle:** A lightweight C library for catching memory leaks at compile time  
**Team members:** [Name 1], [Name 2], [Name 3], [Name 4], [Name 5]  
**Event / Date:** Hackathon – [Date]

---

## Slide 2 – The Problem: Why Memory Leaks Matter

- Every `malloc` that isn't `free`-d is a **memory leak**
- Leaks cause:
  - Gradual RAM exhaustion → slowdown → crash
  - Especially critical in servers, IoT, embedded systems
- Real-world examples:
  - Firefox: multi-MB leaks in long sessions
  - Linux kernel drivers: silent leaks causing reboots
- **Quote:** *"Memory leaks are like termites — silent, slow, and destructive"*

---

## Slide 3 – Existing Tools and Their Drawbacks

| Tool | Issue |
|------|-------|
| Valgrind | Needs separate install, 20×–50× slowdown |
| AddressSanitizer | Requires special compiler flags, may alter behaviour |
| Smart Pointers (C++) | Language-specific, not available in C |
| Manual review | Error-prone at scale |

**Our solution:** A pure-C library that costs almost nothing to add

---

## Slide 4 – Our Solution: memtrack

- **What it is:** A C header + implementation file (~350 lines)
- **What it does:**
  - Intercepts every `malloc`, `calloc`, `realloc`, `free`
  - Records *where* each allocation happened (file + line)
  - Reports all un-freed blocks at program exit
  - Warns on double-free and invalid-free
- **How to use it:** Just add `#include "memtrack.h"` — nothing else

---

## Slide 5 – System Design

*(Show the ASCII architecture diagram from project_report.md)*

**Three layers:**
1. **Macro layer** (`memtrack.h`) – renames malloc/free → tracked_malloc/tracked_free
2. **Tracking layer** (`memtrack.c`) – linked list + mutex + statistics
3. **Report layer** – walks list at exit, prints table + saves `leak_report.txt`

**Key insight:** `__FILE__` and `__LINE__` macros give us free source-location info

---

## Slide 6 – Implementation Highlights

**The Macro Trick:**
```c
// In memtrack.h
#define malloc(size)  tracked_malloc((size), __FILE__, __LINE__)

// In memtrack.c – disable macros so we can call real libc
#undef malloc
#include <stdlib.h>
```

**Linked List Node:**
```c
typedef struct AllocRecord {
    void *ptr;  size_t size;
    const char *file;  int line;
    struct AllocRecord *next;
} AllocRecord;
```

**Auto-report:**
```c
// On first allocation
atexit(memtrack_report);  // fires automatically at exit
```

---

## Slide 7 – Testing

**Unit Tests (8 tests, all PASS):**

| Test | Covers |
|------|--------|
| malloc then free | Normal allocation cycle |
| malloc without free | 1 leak detected |
| calloc zero-init | Memory is zeroed |
| realloc grow/shrink | Record updated correctly |
| Double free | Intercepted, no crash |
| Invalid free | Intercepted, no crash |
| Peak memory | High-water mark tracked |

`make test` → runs all 8 tests automatically

---

## Slide 8 – Live Demo / Results

*(Run the following commands live or show screenshots)*

```bash
make run-leak   # Shows 3 leaks + 2 warnings
make run-clean  # Shows "No leaks detected"
make test       # 8/8 PASS
```

**Sample output from demo_leak:**
```
  ✗  LEAKED BLOCKS DETECTED:
  0x55f...  64 bytes   demo_leak.c:24
  0x55f...  40 bytes   demo_leak.c:30
  0x55f...  256 bytes  demo_leak.c:36
  Leaked blocks : 3 | Leaked bytes : 360
```

---

## Slide 9 – Impact and Advantages

| Feature | Our Tool | Valgrind |
|---------|----------|----------|
| Installation | None | Required |
| Compilation change | 1 `#include` | None |
| Runtime slowdown | ~0% | 20–50× |
| File/line info | ✓ | ✓ |
| Thread safe | ✓ | ✓ |
| No external deps | ✓ | ✗ |
| Code size | ~350 lines | Millions |

---

## Slide 10 – Future Scope

- 🔍 **Stack traces** – use `backtrace()` to show full call chain per leak
- 🔌 **LD_PRELOAD mode** – intercept allocations in any binary without recompiling
- 📊 **JSON / HTML export** – structured reports for CI/CD integration
- 🖥️ **GUI dashboard** – real-time allocation timeline visualisation
- 🔒 **Per-thread reporting** – colour-coded output per thread ID
- 📦 **CMake support** – for integration into larger projects

---

*"The best code is code that manages its own memory. The second best is code that at least knows when it doesn't."*
