# i386 Phase 6 performance report

Status: W9 engine measurements complete; final interactive Notepad distribution pending the
remaining Wine callback/exception fix.

## Acceptance policy

SharpWine's native macOS ARM64 execution evidence decides whether an i386 capability is retained.
Interpreter/JIT equality, checked state and fault behavior, deterministic corpus coverage, and real
guest program loading are positive capability evidence. External implementations and native-Windows
results are comparison oracles, not ceilings. A CPUID family is masked only while its native GEM/Blink
implementation is genuinely incomplete.

## Method

Five independent Release runs used the Phase 5.5 performance fixture on native Apple ARM64. Each run
compares one-instruction stepping with the checked multi-instruction path and verifies one-step,
bounded-interpreter, and JIT equality at every tested budget boundary. The optimized path retains
transaction validation, precise stops and faults, async stops, and full state export at observable
boundaries.

Host: macOS 27.0 (26A5378n), arm64, Apple clang 21.0.0. Build:
`sharpwine-phase-6-w9-refresh-final2`. Machine-readable samples and counters are in
`docs/architecture/adr/i386-phase6-performance-evidence.json`.

## Results

| Sample | Step median (ns) | Run median (ns) | Speedup | Throughput (instructions/s) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 40,181,000 | 4,004,000 | 10.035× | 16,367,632 |
| 2 | 37,388,000 | 3,871,000 | 9.658× | 16,929,992 |
| 3 | 36,850,000 | 3,829,000 | 9.624× | 17,115,696 |
| 4 | 37,148,000 | 3,751,000 | 9.903× | 17,471,608 |
| 5 | 37,413,000 | 3,880,000 | 9.643× | 16,890,722 |
| **Median** | **37,388,000** | **3,871,000** | **9.658×** | **16,929,992** |

The required floor is 3.000×. The adaptive checked quantum reaches 4,096 instructions and reduces
the fixture from 1,536 quanta at the former 256-instruction ceiling to 336. A representative run
retired 360,576 instructions with 261 page snapshots, 336 state imports/exports, zero retries,
1,069,056 bytes copied, zero committed write bytes, and 41 µs lock wait.

The JIT/block equality lane retired 32,896 instructions with 8 compilations, 32,896 executions,
32,888 JIT cache hits, zero failures, 8 blocks created, 32,633 block-cache/direct-link hits, and zero
code invalidations. The CALL/RET probe recorded 64 calls, 64 returns, 64 correct predictions, zero
misses, and four precise block invalidations.

## Loader relevance

Writable, non-executable host-backed pages are lazily refreshed at Wine boundaries, read-only
executable pages retain reviewed decode/JIT state, and writable executable pages are reconciled once
per public run. Dedicated production-JIT and interpreter gates prove guest writes reach an external
RWX page and a later host mutation is observed on the next run. A transaction conflict forces a
resynchronization before retry.

With this synchronization policy, the real 32-bit Notepad path no longer consumes a stale
`RtlGetLocaleFileMappingAddress` output pointer or takes the compiler's deliberate null trap. It
progresses through DLL/NLS initialization into the window-class registration path at normal speed.
That is substantive native program-loading evidence. The next reproducible ARM64EC/WoW64 callback
and class-registration fault remains open, so the Phase 6 exit checkbox is not complete until that
native state path is fixed and visible launch distributions are captured.

## Verification

- Fresh locked Blink patch application produced `blink/gem_embed.c` SHA-256
  `5880d08128b08aac307ac03678030d51a697f08be8f274f7b4d8d1a759c132ca`.
- All 37 tests enabled in the fresh W9 refresh build passed, including the production-JIT and
  interpreter external executable-page refresh gates plus all enabled i386 conformance, golden,
  randomized, performance, page, and concurrency gates.
- The broader build passed all 42 configured tests, including both authentic ARM64X lanes. The
  roundtrip CI regression was fixed by removing a stale expectation that Blink's private lazy-parity
  byte appear in architectural RFLAGS.
