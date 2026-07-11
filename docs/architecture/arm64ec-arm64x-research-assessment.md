# ARM64EC/ARM64X research assessment

Status: evidence-corrected planning input, not an implementation specification.

This document evaluates the local research note `Architecture Research for arm64ec - arm64x.md` against repository invariants and experiments completed through 2026-07-11. The note identifies useful problem areas, but its sample code and several factual claims are not safe to implement without additional evidence.

## Directions retained

- Parse PE32+ load configuration and CHPE records with checked file-offset/RVA/VA conversion. Mach-O `dyld` is not a PE loader and does not provide this information.
- Keep ISA classification metadata-only. Neither thunk-looking bytes nor a raw `.hybmp$x` COFF section prove a linked target's ISA.
- Keep 4 KiB Windows guest-page semantics independent of the host's physical page size.
- Treat x86 memory-order correctness as an explicit x64-engine contract with litmus tests, not as an assumed property of generated AArch64.
- Prefer architectural AArch64 atomics where their semantics have been proven equivalent, with a deterministic correctness fallback.

These directions fit GEM ownership: GEM owns canonical state, checked guest memory, faults, and transitions. AArch64 and x64 engines consume those interfaces rather than patching process-global state or owning PE metadata.

## Claims rejected or still unverified

1. **Linked CHPE production.** LLVM/lld 22.1.8 did not produce the required linked ARM64X image. `/MACHINE:ARM64EC`, `/DYNAMICBASE`, and `/GUARD:CF` do not by themselves create valid native and EC load configurations or `__chpe_metadata`. Raw COFF `.hybmp$x`, `.hexpthk`, and `.a64xrm` are not substitutes.
2. **Machine identifiers.** `IMAGE_FILE_MACHINE_ARM64EC` is `0xA641`; code using `0x4441` for ARM64EC is unsuitable for this project.
3. **CHPE layouts.** Handwritten structure layouts, field offsets, and table meanings require a pinned public specification or verified legal fixture. No guessed structure may enter the parser.
4. **TSO API history.** Current native probes found no public/exported `sys_set_tso`, `thread_set_tso`, or `pthread_set_tso_np`, but do not prove that macOS 27 removed previously public APIs. `pthread_from_mach_thread_np()` only converts a Mach thread port to a pthread handle.
5. **Virtualization.framework TSO.** No evidence presently establishes a supported per-vCPU x86-TSO control available to a Linux guest through Virtualization.framework. This is not a planned fallback until documented and measured.
6. **x86 memory ordering.** x86-TSO permits a younger load to become visible before an older store to a different address through the store buffer. A rule that simply inserts a full barrier for each Store→Load pair is neither a validated semantic model nor an acceptable optimization plan. LDAR/STLR substitution alone is also not a proof of x86-TSO equivalence.
7. **Transition barriers.** An `ISB` is not required merely because GEM changes its logical guest ISA. Native instruction-cache maintenance is required only when executable host code is created or modified, following the platform JIT/cache protocol.
8. **Signal-handler fault emulation.** The proposed handlers call operations or inspect state without an established async-signal-safe, race-free contract. Temporarily widening a 16 KiB host page can violate neighboring 4 KiB guest protections. Process-global page and stepping state is incompatible with deterministic multithreaded execution.
9. **Hardware stepping.** Directly mutating guessed `ARM_DEBUG_STATE64`/`MDSCR_EL1` fields from `ucontext_t` is not an accepted macOS user-space single-step API. It must not be used without a documented Mach exception/thread-state contract and a native probe.
10. **Instruction scanning.** Looking for byte `0xF0` and a primary opcode is not an x86 decoder. Prefix ordering, REX/VEX/EVEX, ModRM/SIB, operand widths, implicit atomicity, alignment, and fault behavior require an established decoder or the selected x64 engine's decoder.

## Required evidence tracks

### A. Linked ARM64X fixture and issue #11 execution

The external fixture prerequisite is complete. Issue #10 and CI run `29146642211` used the
reviewed Microsoft producer on native Windows 11 ARM64 to create two clean linked images in
runner-temporary directories, inspect their load configuration and CHPE records, compare bounded
normalized evidence, and execute their ARM64EC hosts. No generated binary was committed or
uploaded.

Issue #11 remains a separate execution gate. Acceptance requires:

- checked inspection of every relevant linked record;
- metadata classification of distinct ARM64EC thunk and real x64 target ranges;
- native-probed relocation/import/alias resolution evidence and forced-nonpreferred-base loading;
- pinned-Dynarmic execution of generated integer, FP, aggregate, and variadic paths;
- native Windows ARM64 validation; and
- an exact stop before any x64 fetch, with no Blink linkage or execution.

### B. x64 memory-model contract

Before Blink JIT is accepted as a correctness path:

- define observable x86-TSO requirements from an authoritative architecture source;
- inventory Blink's interpreter and JIT memory operations, atomics, fences, self-modifying-code protocol, and host compiler assumptions;
- run bounded Store Buffering, Load Buffering, Message Passing, IRIW, locked-operation, and self-modifying-code tests under contention;
- compare interpreter/JIT results and preserve an interpreter or serialized fallback;
- prove fault atomicity for cross-page and misaligned operations; and
- treat any hardware-TSO path as an optional optimization selected only by a supported, queryable API.

### C. 4 KiB guest pages on a 16 KiB host

GEM's checked memory layer remains authoritative. It must never claim independent host protection for four guest subpages sharing one host page unless that isolation is actually enforced. The plan is:

- use checked software translation for canonical guest accesses;
- aggregate host permissions conservatively;
- route protection-sensitive or ambiguous accesses through the correctness path;
- keep guest faults transactional across every 4 KiB boundary;
- avoid signal-handler allocation, locks, logging, broad permission windows, and process-global stepping state; and
- evaluate Mach exceptions only as a separately proven optimization, not as the memory correctness foundation.

## Decision

The research note is valuable as a catalog of questions, not as executable guidance. The roadmap adopts its valid goals through the evidence tracks above while rejecting guessed metadata, unproven TSO controls, ad-hoc decoding, and unsafe signal-based page emulation.
