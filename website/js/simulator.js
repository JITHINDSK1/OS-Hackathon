/**
 * memtrack — Interactive Memory Leak & Allocator Simulator
 * Mirrors the exact tracking, safety traps, and atexit reporting of memtrack.c
 */

document.addEventListener('DOMContentLoaded', () => {
  // State
  const state = {
    records: [],           // Live tracked allocation nodes
    freedRecords: [],      // History of freed records to simulate double-free
    totalAllocs: 0,
    totalFrees: 0,
    currentBytes: 0,
    peakBytes: 0,
    isTerminated: false,
    baseAddress: 0x55a100416000,
    addressOffset: 0x20
  };

  const sampleFiles = [
    { file: 'examples/demo_leak.c', line: 24 },
    { file: 'examples/demo_leak.c', line: 30 },
    { file: 'examples/demo_leak.c', line: 36 },
    { file: 'src/parser.c', line: 88 },
    { file: 'src/network.c', line: 142 },
    { file: 'src/buffer.c', line: 55 }
  ];

  // DOM Elements
  const sizeInput = document.getElementById('alloc-size-input');
  const presetBtns = document.querySelectorAll('.btn-preset');
  const btnMalloc = document.getElementById('btn-malloc');
  const btnCalloc = document.getElementById('btn-calloc');
  const btnFreeLast = document.getElementById('btn-free-last');
  const btnRealloc = document.getElementById('btn-realloc-grow');
  const btnDoubleFree = document.getElementById('btn-double-free');
  const btnInvalidFree = document.getElementById('btn-invalid-free');
  const btnExitReport = document.getElementById('btn-exit-report');
  const btnResetSim = document.getElementById('btn-reset-sim');
  const btnClearConsole = document.getElementById('btn-clear-console');

  const valTotalAllocs = document.getElementById('val-total-allocs');
  const valTotalFrees = document.getElementById('val-total-frees');
  const valCurrentBytes = document.getElementById('val-current-bytes');
  const valPeakBytes = document.getElementById('val-peak-bytes');
  const activeCountBadge = document.getElementById('active-count-badge');
  const tableBody = document.getElementById('tracked-table-body');
  const consoleOutput = document.getElementById('sim-console-output');
  const reportBox = document.getElementById('sim-report-box');
  const reportText = document.getElementById('terminal-report-text');
  const appStatus = document.getElementById('sim-app-status');

  // Quick Preset Handlers
  presetBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      presetBtns.forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      sizeInput.value = btn.dataset.size;
    });
  });

  // Helper: Next Hex Address
  function getNextAddress() {
    const addr = '0x' + (state.baseAddress + state.addressOffset).toString(16);
    state.addressOffset += 0x60;
    return addr;
  }

  // Helper: Log to Virtual Console
  function logConsole(message, type = 'system-line') {
    const line = document.createElement('div');
    line.className = `console-line ${type}`;
    line.textContent = message;
    consoleOutput.appendChild(line);
    consoleOutput.scrollTop = consoleOutput.scrollHeight;
  }

  // Helper: Update Peak & UI Counters
  function updateMetrics() {
    if (state.currentBytes > state.peakBytes) {
      state.peakBytes = state.currentBytes;
    }

    valTotalAllocs.textContent = state.totalAllocs;
    valTotalFrees.textContent = state.totalFrees;
    valCurrentBytes.innerHTML = `${state.currentBytes} <small>B</small>`;
    valPeakBytes.innerHTML = `${state.peakBytes} <small>B</small>`;
    activeCountBadge.textContent = `${state.records.length} Live Blocks`;

    renderTable();
  }

  // Render Tracked Table
  function renderTable() {
    if (state.records.length === 0) {
      tableBody.innerHTML = `
        <tr class="empty-row">
          <td colspan="5">No active memory allocations. Click <code>malloc()</code> or <code>calloc()</code> above to track memory.</td>
        </tr>`;
      return;
    }

    tableBody.innerHTML = '';
    state.records.forEach((rec, idx) => {
      const row = document.createElement('tr');
      row.innerHTML = `
        <td><code>${rec.ptr}</code></td>
        <td><strong>${rec.size}</strong> B</td>
        <td><code>${rec.file}:${rec.line}</code></td>
        <td><span class="status-active">Active</span></td>
        <td><button class="btn-table-free" data-index="${idx}">free()</button></td>
      `;
      tableBody.appendChild(row);
    });

    // Attach row free listeners
    tableBody.querySelectorAll('.btn-table-free').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const index = parseInt(e.target.dataset.index, 10);
        freeBlockByIndex(index);
      });
    });
  }

  // Action: tracked_malloc
  function handleMalloc(isCalloc = false) {
    if (state.isTerminated) {
      logConsole('[memtrack] Process already terminated. Reset simulator to run again.', 'warn-line');
      return;
    }

    const size = parseInt(sizeInput.value, 10) || 64;
    const sample = sampleFiles[state.totalAllocs % sampleFiles.length];
    const ptr = getNextAddress();

    const record = {
      ptr: ptr,
      size: size,
      file: sample.file,
      line: sample.line,
      type: isCalloc ? 'calloc' : 'malloc'
    };

    // Linked list prepend
    state.records.unshift(record);
    state.totalAllocs++;
    state.currentBytes += size;

    logConsole(`[memtrack] ${isCalloc ? 'calloc' : 'malloc'}(${size}) -> ${ptr} at ${sample.file}:${sample.line}`, 'alloc-line');
    updateMetrics();
  }

  // Action: tracked_free by index
  function freeBlockByIndex(index) {
    if (state.isTerminated) {
      logConsole('[memtrack] Process already terminated.', 'warn-line');
      return;
    }

    if (index < 0 || index >= state.records.length) return;

    const [freed] = state.records.splice(index, 1);
    state.freedRecords.push(freed);
    state.totalFrees++;
    state.currentBytes -= freed.size;

    logConsole(`[memtrack] free(${freed.ptr}) [${freed.size} bytes] cleanly released.`, 'free-line');
    updateMetrics();
  }

  // Action: free last active block
  function handleFreeLast() {
    if (state.records.length === 0) {
      logConsole('[memtrack] No active allocations to free.', 'system-line');
      return;
    }
    freeBlockByIndex(0);
  }

  // Action: realloc grow
  function handleRealloc() {
    if (state.records.length === 0) {
      logConsole('[memtrack] Allocate a block first before calling realloc.', 'system-line');
      return;
    }

    const target = state.records[0];
    const oldSize = target.size;
    const newSize = oldSize * 2;
    const newPtr = getNextAddress();

    // Update in tracking list
    state.currentBytes = (state.currentBytes - oldSize) + newSize;
    target.size = newSize;
    target.ptr = newPtr;

    logConsole(`[memtrack] realloc(${target.ptr}, ${newSize}B) resized from ${oldSize}B`, 'alloc-line');
    updateMetrics();
  }

  // Action: double free
  function handleDoubleFree() {
    if (state.freedRecords.length === 0) {
      logConsole('[memtrack] Allocating and freeing a block first to demonstrate double free...', 'system-line');
      handleMalloc();
      freeBlockByIndex(0);
    }

    const lastFreed = state.freedRecords[state.freedRecords.length - 1];
    logConsole(`[memtrack] WARNING: Invalid/double free of pointer ${lastFreed.ptr} at examples/demo_leak.c:43`, 'warn-line');
  }

  // Action: invalid free (stack variable address)
  function handleInvalidFree() {
    const stackAddr = '0x7ffd5102abac';
    logConsole(`[memtrack] WARNING: Invalid/double free of pointer ${stackAddr} at examples/demo_leak.c:48`, 'warn-line');
  }

  // Action: exit program & generate report (mirrors memtrack_report())
  function handleExitProgram() {
    state.isTerminated = true;
    appStatus.textContent = 'Status: Process Exited';
    appStatus.classList.add('exited');

    const leakedBlocks = state.records.length;
    const leakedBytes = state.currentBytes;

    let report = '\n';
    report += '╔══════════════════════════════════════════════════════════════╗\n';
    report += '║          MEMORY LEAK DETECTION REPORT (memtrack)            ║\n';
    report += '╚══════════════════════════════════════════════════════════════╝\n\n';

    if (leakedBlocks === 0) {
      report += '  ✓  No memory leaks detected. All allocations were freed.\n\n';
    } else {
      report += '  ✗  LEAKED BLOCKS DETECTED:\n\n';
      report += '  Address             Bytes     Allocated at\n';
      report += '  ------------------  --------  ----------------------------\n';

      state.records.forEach(rec => {
        const addrCol = rec.ptr.padEnd(18, ' ');
        const sizeCol = rec.size.toString().padEnd(8, ' ');
        report += `  ${addrCol}  ${sizeCol}  ${rec.file}:${rec.line}\n`;
      });
      report += '\n';
    }

    report += '┌─────────────────────────────────────────────┐\n';
    report += '│                   SUMMARY                   │\n';
    report += '├─────────────────────────────────────────────┤\n';
    report += `│  Total allocations  : ${state.totalAllocs.toString().padEnd(22, ' ')}│\n`;
    report += `│  Total frees        : ${state.totalFrees.toString().padEnd(22, ' ')}│\n`;
    report += `│  Leaked blocks      : ${leakedBlocks.toString().padEnd(22, ' ')}│\n`;
    report += `│  Leaked bytes       : ${leakedBytes.toString().padEnd(22, ' ')}│\n`;
    report += `│  Peak memory usage  : ${(state.peakBytes + ' bytes').padEnd(22, ' ')}│\n`;
    report += '└─────────────────────────────────────────────┘\n\n';
    report += '  [Report also saved to: leak_report.txt]\n';

    reportText.textContent = report;
    reportBox.style.display = 'block';
    reportBox.scrollIntoView({ behavior: 'smooth', block: 'nearest' });

    logConsole(`[memtrack] atexit() triggered. Summary: ${leakedBlocks} leaks (${leakedBytes} bytes).`, 'warn-line');
  }

  // Action: reset simulator
  function handleReset() {
    state.records = [];
    state.freedRecords = [];
    state.totalAllocs = 0;
    state.totalFrees = 0;
    state.currentBytes = 0;
    state.peakBytes = 0;
    state.isTerminated = false;
    state.addressOffset = 0x20;

    appStatus.textContent = 'Status: Process Running';
    appStatus.classList.remove('exited');
    reportBox.style.display = 'none';

    consoleOutput.innerHTML = '<div class="console-line system-line">[memtrack] Simulator reset. Ready for allocator calls.</div>';
    updateMetrics();
  }

  // Event Listeners
  btnMalloc.addEventListener('click', () => handleMalloc(false));
  btnCalloc.addEventListener('click', () => handleMalloc(true));
  btnFreeLast.addEventListener('click', handleFreeLast);
  btnRealloc.addEventListener('click', handleRealloc);
  btnDoubleFree.addEventListener('click', handleDoubleFree);
  btnInvalidFree.addEventListener('click', handleInvalidFree);
  btnExitReport.addEventListener('click', handleExitProgram);
  btnResetSim.addEventListener('click', handleReset);
  btnClearConsole.addEventListener('click', () => {
    consoleOutput.innerHTML = '';
  });

  // Tab Switching Logic (Sample Outputs)
  const tabBtns = document.querySelectorAll('.tab-btn');
  const tabContents = document.querySelectorAll('.tab-content');

  tabBtns.forEach(btn => {
    btn.addEventListener('click', () => {
      tabBtns.forEach(b => b.classList.remove('active'));
      tabContents.forEach(c => c.classList.remove('active'));

      btn.classList.add('active');
      const targetTab = document.getElementById(btn.dataset.tab);
      if (targetTab) {
        targetTab.classList.add('active');
      }
    });
  });

  // Copy Quickstart Code
  const copyBtn = document.getElementById('copy-quickstart');
  if (copyBtn) {
    copyBtn.addEventListener('click', () => {
      const code = `#include <stdio.h>\n#include "memtrack.h"\n\nint main(void) {\n    char *buf = malloc(128);\n    return 0;\n}`;
      navigator.clipboard.writeText(code).then(() => {
        copyBtn.textContent = 'Copied!';
        setTimeout(() => { copyBtn.textContent = 'Copy'; }, 2000);
      });
    });
  }

  // Initialize
  updateMetrics();
});
