# PE32+/ARM64X CHPE parser

`gem_pe_arm64x_parse` accepts PE32+ ARM64X (`0xa64e`) and ARM64 (`0xaa64`)
images only when complete CHPE metadata versions 1 or 2 validates. This is
intentional: Wine 11.12's CHPE-bearing AArch64 `ntdll.dll` has machine
`0xaa64`; an ordinary ARM64 image with no CHPE pointer is rejected with
`no-chpe-metadata`. ARM64EC (`0xa641`) and other machine values are not parser
inputs.

It performs checked little-endian reads, checked arithmetic, bounded
section/table counts, and copies sections and parsed records. The caller retains
the input buffer and may mutate or free it after a successful parse.

Ranges are half-open `[start, end)`. Classification is metadata-only: outside
image, redirection, entry range, CHPE code map, non-executable mapped data, then
invalid uncovered executable RVA. It never reads instruction bytes or infers an
ISA from section flags. This intentionally small parser does not load,
relocate, validate signatures, or execute PE images.

The parser validates the complete minimum metadata layout before reading fields:
80 bytes for v1 and 92 bytes for v2. LLVM defines the 20-dword base layout and
its three v2 fields; Wine's current 29-dword `IMAGE_ARM64EC_METADATA` is a
compatible superset. Tables must be file-backed; code ranges may cross adjacent
executable sections but never a gap or non-executable section.

## Format authorities

- LLVM COFF layout: [`llvm/include/llvm/Object/COFF.h` (`chpe_metadata`,
  `chpe_range_entry`, and table records)](https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/Object/COFF.h).
- Wine 11.12 layout and machine constants: [`include/winnt.h`
  (`IMAGE_ARM64EC_METADATA`, `IMAGE_FILE_MACHINE_ARM64`, and
  `IMAGE_FILE_MACHINE_ARM64X`)](https://gitlab.winehq.org/wine/wine/-/blob/wine-11.12/include/winnt.h).
- Wine's metadata decoder, including ARM64 treatment and range-type meanings:
  [`tools/winedump/pe.c`](https://gitlab.winehq.org/wine/wine/-/blob/wine-11.12/tools/winedump/pe.c).
