# Contributing

1. Open an issue describing the ABI, runtime, or compatibility problem.
2. Keep changes focused and include deterministic tests.
3. Run `tools/ci/run-all.sh` before pushing.
4. Follow the redistributable fixture policy in [`docs/fixtures.md`](docs/fixtures.md).
5. Do not commit generated binaries, build trees, Wine prefixes, SDK files, or proprietary PE files.
6. Wine-derived patches belong under `third_party/patches/wine` and remain under Wine's LGPL-2.1-or-later terms.
7. Every transition implementation must cite captured evidence or a published ABI contract. Guessed dispatcher behavior is rejected.

Fixture-related pull requests must document provenance, license/SPDX identifier, generation or extraction command, source inputs, toolchain version, expected hash of generated build-directory output when applicable, and why no committed binary is required.

All changes require pull-request review, passing required checks, and resolved review conversations.
