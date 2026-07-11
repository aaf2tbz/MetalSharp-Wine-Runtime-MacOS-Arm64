# ADR 0002: Memory Ownership Boundaries

Date: 2026-07-10
Status: Accepted

## Context

Windows guest virtual addresses, protection rules, low aliases, and ARM64X code ranges do not match Darwin VM behavior directly. macOS hosts may use 16 KiB pages while Windows code expects 4 KiB guest-page semantics, and low Windows addresses such as `0x7ffe0000` cannot be treated as ordinary native pointers.

## Decision

GEM owns the canonical Windows virtual-address model, guest page protections, low aliases including KUSER shared data at `0x7ffe0000`, and 4 KiB guest-page semantics. Host VM mappings, identity maps, aliases, and direct-memory fast paths are implementation details only after GEM has validated protections, ranges, and aliasing.

Engines must access memory through checked GEM translation APIs unless a documented, current identity-mapping proof exists for the exact access. Executable target classification must come from parsed ARM64X/CHPE metadata, not code bytes or Mach VM permissions.

## Consequences

The correctness path does not require direct native PE execution or native mappings below 4 GiB. GEM can model Windows write-copy, guard, reserve, commit, execute, and sub-host-page protection rules before execution engines observe memory. Fast paths must fall back to checked translation when any proof expires.

## Rejected alternatives

- Dereferencing guest virtual addresses directly as host pointers by default.
- Inferring guest permissions or ISA solely from Mach VM state.
- Requiring native host mappings for low Windows aliases.
- Letting engine callbacks bypass GEM protection and range checks.

## References

- `docs/architecture/deterministic-vcpu-plan.md`
- `docs/architecture/gem-abi.md`
- `ROADMAP.md` Milestones 1 and 2
- Microsoft ARM64X/CHPE metadata documentation sources cited by the architecture plan
