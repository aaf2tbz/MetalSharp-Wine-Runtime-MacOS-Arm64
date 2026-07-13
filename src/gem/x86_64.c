// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/x86_64.h"

#include <stdbool.h>
#include <string.h>

static bool range_fits(size_t offset, size_t size, size_t total) {
    return offset <= total && size <= total - offset;
}

static uint16_t read_u16(const uint8_t *bytes, size_t offset) {
    return (uint16_t)((uint16_t)bytes[offset] | ((uint16_t)bytes[offset + 1U] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes, size_t offset) {
    return (uint32_t)bytes[offset] | ((uint32_t)bytes[offset + 1U] << 8U) |
           ((uint32_t)bytes[offset + 2U] << 16U) | ((uint32_t)bytes[offset + 3U] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes, size_t offset) {
    return (uint64_t)read_u32(bytes, offset) | ((uint64_t)read_u32(bytes, offset + 4U) << 32U);
}

uint32_t gem_x86_64_abi_version(void) {
    return GEM_X86_64_ABI_VERSION;
}

const char *gem_x86_64_pe_status_name(enum gem_x86_64_pe_status status) {
    static const char *const names[] = {"ok",
                                        "invalid-argument",
                                        "abi-mismatch",
                                        "truncated",
                                        "bad-dos-header",
                                        "bad-signature",
                                        "wrong-machine",
                                        "wrong-optional-magic",
                                        "bad-header",
                                        "limit-exceeded"};
    if ((unsigned int)status >= sizeof(names) / sizeof(names[0]))
        return "unknown";
    return names[status];
}

enum gem_x86_64_pe_status gem_x86_64_classify_pe(const uint8_t *bytes, size_t byte_count,
                                                 struct gem_x86_64_pe_identity *identity) {
    struct gem_x86_64_pe_identity result;
    size_t pe_offset, coff_offset, optional_offset, section_offset, section_bytes;
    uint16_t optional_size;

    if (!identity)
        return GEM_X86_64_PE_INVALID_ARGUMENT;
    if (identity->abi_version != GEM_X86_64_ABI_VERSION ||
        identity->struct_size != sizeof(*identity)) {
        memset(identity, 0, sizeof(*identity));
        return GEM_X86_64_PE_ABI_MISMATCH;
    }
    memset(&result, 0, sizeof(result));
    result.abi_version = GEM_X86_64_ABI_VERSION;
    result.struct_size = sizeof(result);
    if (!bytes) {
        *identity = result;
        return GEM_X86_64_PE_INVALID_ARGUMENT;
    }
    if (!range_fits(0U, 0x40U, byte_count)) {
        *identity = result;
        return GEM_X86_64_PE_TRUNCATED;
    }
    if (read_u16(bytes, 0U) != UINT16_C(0x5a4d)) {
        *identity = result;
        return GEM_X86_64_PE_BAD_DOS_HEADER;
    }
    pe_offset = read_u32(bytes, 0x3cU);
    if (!range_fits(pe_offset, 24U, byte_count)) {
        *identity = result;
        return GEM_X86_64_PE_TRUNCATED;
    }
    if (read_u32(bytes, pe_offset) != UINT32_C(0x00004550)) {
        *identity = result;
        return GEM_X86_64_PE_BAD_SIGNATURE;
    }
    coff_offset = pe_offset + 4U;
    result.machine = read_u16(bytes, coff_offset);
    result.section_count = read_u16(bytes, coff_offset + 2U);
    optional_size = read_u16(bytes, coff_offset + 16U);
    if (result.machine != GEM_X86_64_PE_MACHINE) {
        *identity = result;
        return GEM_X86_64_PE_WRONG_MACHINE;
    }
    if (!result.section_count || result.section_count > GEM_X86_64_MAX_PE_SECTIONS) {
        *identity = result;
        return result.section_count ? GEM_X86_64_PE_LIMIT_EXCEEDED : GEM_X86_64_PE_BAD_HEADER;
    }
    optional_offset = coff_offset + 20U;
    if (optional_size < 112U || !range_fits(optional_offset, optional_size, byte_count)) {
        *identity = result;
        return GEM_X86_64_PE_TRUNCATED;
    }
    result.optional_magic = read_u16(bytes, optional_offset);
    if (result.optional_magic != GEM_X86_64_PE32_PLUS_MAGIC) {
        *identity = result;
        return GEM_X86_64_PE_WRONG_OPTIONAL_MAGIC;
    }
    result.entry_point_rva = read_u32(bytes, optional_offset + 16U);
    result.image_base = read_u64(bytes, optional_offset + 24U);
    result.size_of_image = read_u32(bytes, optional_offset + 56U);
    result.size_of_headers = read_u32(bytes, optional_offset + 60U);
    section_offset = optional_offset + optional_size;
    section_bytes = (size_t)result.section_count * 40U;
    if (!range_fits(section_offset, section_bytes, byte_count)) {
        *identity = result;
        return GEM_X86_64_PE_TRUNCATED;
    }
    if (!result.image_base || !result.size_of_headers ||
        result.size_of_headers > result.size_of_image || result.size_of_headers > byte_count ||
        (result.entry_point_rva && result.entry_point_rva >= result.size_of_image)) {
        *identity = result;
        return GEM_X86_64_PE_BAD_HEADER;
    }
    *identity = result;
    return GEM_X86_64_PE_OK;
}
