# ADR 0011: Rosetta differential development oracle

Date: 2026-07-15
Status: Accepted for development and conformance work

## Context

The native ARM64 GEM/Blink runtime needs broad, reproducible x86 semantic
coverage before the i386 Notepad and Steam Setup milestones can be treated as
hardened. Rosetta 2 is available on the development host and provides a mature
x86_64-to-ARM64 implementation. It does not directly execute i386 Mach-O code,
so it cannot alone characterize the legacy-only IA-32 environment required by
Windows WoW64.

ProjectChampollion documents useful observable Rosetta internals: a direct
x86_64-to-ARM64 register mapping, JIT translation of dynamically generated
code reached by indirect branches, legacy-prefix decoding, optional translation
IR/segment diagnostics, and an ARM64 `execve` runner that keeps LLDB attached
through the x86_64-to-Rosetta transition. Its analysis targets an earlier
Rosetta build and is supporting research, not a normative compatibility table.

## Decision

Conformance uses two separate Rosetta-backed lanes:

1. A direct x86_64 Mach-O corpus executes generated code under Rosetta. This is
   the primary oracle for shared x86 semantics and Rosetta's supported x86_64
   envelope. Every run proves translation with `sysctl.proc_translated == 1`.
2. A freestanding PE32 console corpus runs through a pinned, x86_64 MetalSharp
   Wine WoW64 carrier under Rosetta. This covers IA-32 register width,
   addressing, x87, segmentation, PE32 transitions, and other legacy-only
   behavior. Carrier behavior is identified separately from Rosetta behavior.

Both lanes emit normalized, versioned architectural records containing only
defined outputs: registers, defined flag masks, memory, SIMD/x87 state, fault
class and address, and instruction bytes. Inputs, executables, carrier archive,
host OS/Rosetta identity, and results are hash-bound. Native ARM64 GEM/Blink
replays the exact instruction bytes and compares records field by field.

The initial PE32 runner is console-only, uses a fresh isolated prefix, disables
installer/UI helpers, imposes a process-group watchdog, and always kills its
scoped wineserver. It refuses quarantined, unsigned, or improperly entitled
Wine hosts. Runtime preparation ad-hoc signs every extracted Mach-O and grants
the Wine host executables only the JIT/executable-memory allowances needed for
this local oracle.

ProjectChampollion's tracing techniques are used only after a mismatch is
minimized. Architectural state is the acceptance oracle; Rosetta IR, AOT code,
and ARM64 register mapping are diagnostic evidence that helps locate a bad
decode, lowering, flag, memory, or transition decision.

## Coverage order

Coverage expands by encoding family rather than attempting an unbounded cross
product: one-byte, `0F`, `0F 38`, and `0F 3A` maps; legacy/REX/VEX prefixes;
ModR/M, SIB, displacement and immediate edges; arithmetic flags; branches and
stack; faults and page boundaries; string operations; atomics and ordering;
x87, MMX, SSE through AVX2; self-modifying code; and i386 segmentation/WoW64
transitions. Randomized differential cases supplement, but never replace,
deterministic boundary cases.

## Current verified corpus

The first broad PE32 matrix contains 514 fully defined cases and completed in a
single 4.596-second carrier batch on macOS 27.0 (26A5378j). It covers 8-, 16-,
and 32-bit arithmetic and flags; shifts, rotates and double shifts at masked
count boundaries; bit scans and bit manipulation; aligned and unaligned locked
memory operations; scalar moves; packed integer SSE2, SSSE3, and SSE4.1; and
POPCNT/LZCNT/TZCNT. Rosetta produced a record for every case.

A freshly extracted Blink tree with patches 0001 through 0006 replayed every
record in both the ARM64 interpreter and ARM64 JIT modes: 1,028 comparisons,
1,028 matches, zero unsupported instructions, zero crashes, and zero semantic
mismatches. The expanded run found and froze fixes for INC/NEG auxiliary carry,
CMPXCHG subtraction direction, LZCNT, Blink embedding bus initialization for
unaligned atomics, and the SSE4.1 packed integer min/max family.

This result does not close the remaining coverage plan. Fault delivery and page
crossings, x87/MMX state, string/REP instructions, segmentation, self-modifying
code, and wider seeded randomized input remain separate follow-on corpora
because they require additional normalized state or exception capture.

## Boundaries

- Rosetta and the MetalSharp carrier are development oracles only. Neither is
  linked, packaged, invoked, or silently selected by the shipped no-Rosetta
  runtime.
- A Rosetta match is evidence of compatibility, not permission to diverge from
  Intel architectural requirements.
- Rosetta's x86_64 coverage does not prove i386 segmentation or Windows WoW64;
  those claims require the separately identified PE32 carrier lane and native
  Windows evidence where available.
- GUI workloads are recorded separately from console semantic collection. They
  do not replace field-by-field architectural conformance.

## Relationship to ADR 0010

ADR 0010's rejection of Rosetta as an implementation path remains unchanged.
This ADR permits Rosetta only as an offline differential development oracle.
