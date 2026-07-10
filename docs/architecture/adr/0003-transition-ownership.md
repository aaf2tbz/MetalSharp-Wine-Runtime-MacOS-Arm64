# ADR 0003: Transition Ownership Boundaries

Date: 2026-07-10
Status: Accepted

## Context

ARM64EC execution requires mandatory indirect-call checkers, compiler-generated entry/exit thunks, dispatch helpers, return helpers, and state conversion between ARM64EC and x64/Blink. Incorrect guesses at these boundaries can corrupt registers, stacks, target ISA, or the canonical TEB.

## Decision

One GEM transition broker owns ARM64EC/x64 transitions. The broker is responsible for checker behavior, thunk entry/exit execution, dispatch helper stops, return paths, and conversion to and from Blink state.

The broker must:

- drive target classification from ARM64X/CHPE metadata;
- preserve `x[18] == teb` and never rely on host x18 authority;
- use Microsoft ABI documentation, LLVM-generated thunk evidence, Wine-generated artifacts, or legal reproducible fixtures;
- report evidence-backed stop reasons to engines and callers;
- reject guessed dispatcher/helper behavior; and
- keep optimized transition paths behind correctness fallbacks.

## Consequences

Engines may surface a transition stop, but they do not decide transition semantics. Signal handlers may report faults; they are not implicit transition mechanisms. Future ARM64EC checker and thunk implementations must add evidence and tests before behavior becomes accepted.

## Rejected alternatives

- Guessing `__os_arm64x_*` helper register, stack, or return semantics.
- Replacing mandatory checkers with byte-pattern target guessing.
- Static x18-to-x28 substitutions.
- Allowing Blink or an AArch64 engine to perform state conversion outside broker contracts.

## References

- `docs/architecture/deterministic-vcpu-plan.md`
- `docs/architecture/gem-abi.md`
- `ROADMAP.md` Milestones 4 and 5
- Microsoft ARM64EC ABI and assembly/thunk documentation
- LLVM `AArch64Arm64ECCallLowering.cpp`
