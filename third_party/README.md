# Third-party components

Third-party source is not vendored by default. `components.lock.json` pins reviewed upstream revisions and licenses.
Wine-derived patch files retain Wine's LGPL-2.1-or-later licensing.

Dynarmic is selected as the optional Milestone 3 AArch64 correctness engine and is fetched by CMake at the
pinned revision only when engine support is enabled. Its generated source and build trees must remain under
ignored build directories; do not commit fetched Dynarmic sources, static libraries, or binaries here.

Blink is pinned as the planned x64 engine for the issue #12 memory-order conformance work. Its source and
all generated guest images remain build-tree-only. The unmodified concurrent interpreter and JIT are not
accepted as correctness paths until the contract and tests in ADR 0006 pass on native ARM64 macOS.

Each future component must be recorded in `NOTICE.md` and pass license review before integration.
