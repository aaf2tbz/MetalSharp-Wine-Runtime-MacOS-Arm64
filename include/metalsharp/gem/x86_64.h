// SPDX-License-Identifier: Apache-2.0
#ifndef METALSHARP_GEM_X86_64_H
#define METALSHARP_GEM_X86_64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEM_X86_64_ABI_VERSION UINT32_C(1)
#define GEM_X86_64_PE_MACHINE UINT16_C(0x8664)
#define GEM_X86_64_PE32_PLUS_MAGIC UINT16_C(0x020b)
#define GEM_X86_64_MAX_PE_SECTIONS UINT16_C(96)

enum gem_x86_64_pe_status {
    GEM_X86_64_PE_OK = 0,
    GEM_X86_64_PE_INVALID_ARGUMENT = 1,
    GEM_X86_64_PE_ABI_MISMATCH = 2,
    GEM_X86_64_PE_TRUNCATED = 3,
    GEM_X86_64_PE_BAD_DOS_HEADER = 4,
    GEM_X86_64_PE_BAD_SIGNATURE = 5,
    GEM_X86_64_PE_WRONG_MACHINE = 6,
    GEM_X86_64_PE_WRONG_OPTIONAL_MAGIC = 7,
    GEM_X86_64_PE_BAD_HEADER = 8,
    GEM_X86_64_PE_LIMIT_EXCEEDED = 9
};

struct gem_x86_64_pe_identity {
    uint32_t abi_version;
    uint32_t struct_size;
    uint16_t machine;
    uint16_t optional_magic;
    uint16_t section_count;
    uint16_t reserved;
    uint32_t entry_point_rva;
    uint64_t image_base;
    uint32_t size_of_image;
    uint32_t size_of_headers;
};

#if defined(__cplusplus)
static_assert(sizeof(struct gem_x86_64_pe_identity) == 40U, "gem_x86_64_pe_identity ABI changed");
#else
_Static_assert(sizeof(struct gem_x86_64_pe_identity) == 40U, "gem_x86_64_pe_identity ABI changed");
#endif

uint32_t gem_x86_64_abi_version(void);
const char *gem_x86_64_pe_status_name(enum gem_x86_64_pe_status status);

/* Classifies a complete file-backed PE image.  The caller initializes
 * abi_version and struct_size; all other output fields are cleared on failure.
 * This boundary accepts ordinary AMD64 PE32+ images only.  ARM64X/ARM64EC,
 * i386, PE32, malformed, truncated, and excessive-section inputs fail closed. */
enum gem_x86_64_pe_status gem_x86_64_classify_pe(const uint8_t *bytes, size_t byte_count,
                                                 struct gem_x86_64_pe_identity *identity);

#ifdef __cplusplus
}
#endif

#endif
