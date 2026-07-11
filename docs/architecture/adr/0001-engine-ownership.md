# ADR 0001: Engine Ownership Boundaries

Date: 2026-07-10
Status: Accepted

## Context

GEM will coordinate ARM64EC/AArch64 correctness engines, optional optimized engines, and Blink for x64 execution. These engines expose different private register banks, code caches, and fault-reporting mechanisms. The roadmap requires canonical Windows state to remain independent of Darwin x18, host signal frames, and engine internals.

## Decision

GEM owns canonical CPU state in `gem_thread_context`, including guest x18/TEB, current ISA, and deterministic stop reasons. Execution engines own only transient synchronized views while running a bounded slice of guest code.

Engines must:

- import state from `gem_thread_context` before execution and export state back at every GEM boundary;
- report explicit stop reasons rather than relying on signal-handler side effects;
- treat Darwin x18, `ucontext_t`, Blink internals, and native engine registers as non-authoritative;
- preserve `x[18] == teb` at guest boundaries;
- use ARM64X/CHPE metadata for ISA/range classification, not instruction-byte guessing;
- avoid Rosetta dependencies; and
- provide a correctness fallback for every optimized path.

## Consequences

Fast engines and code caches are optional optimizations. They may be disabled or bypassed without changing architectural results. No engine may retain uncommitted canonical state across a transition, fault, host return, or budget stop. Correctness-engine selection remains a future conformance decision.

## Rejected alternatives

- Making host x18 or Darwin `ucontext_t` the canonical TEB source.
- Letting Blink or an AArch64 engine own canonical state after faults or transitions.
- Selecting an optimized engine path without a deterministic fallback.
- Inferring mixed-ISA targets from instruction bytes.

## References

- `docs/architecture/deterministic-vcpu-plan.md`
- `docs/architecture/gem-abi.md`
- `ROADMAP.md` Milestones 0, 3, and 5
- Microsoft ARM64EC ABI and thunk documentation
- Apple JIT guidance for Apple silicon
