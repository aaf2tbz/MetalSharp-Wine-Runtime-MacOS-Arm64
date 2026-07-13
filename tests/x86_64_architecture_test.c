// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/x86_64.h"

#include <assert.h>
#include <string.h>

static void put_u16(uint8_t *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1U] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    put_u16(bytes, offset, (uint16_t)value);
    put_u16(bytes, offset + 2U, (uint16_t)(value >> 16U));
}

static void put_u64(uint8_t *bytes, size_t offset, uint64_t value) {
    put_u32(bytes, offset, (uint32_t)value);
    put_u32(bytes, offset + 4U, (uint32_t)(value >> 32U));
}

static void build_pe32_plus(uint8_t bytes[512]) {
    const size_t pe = 0x80U;
    const size_t coff = pe + 4U;
    const size_t optional = coff + 20U;
    memset(bytes, 0, 512U);
    put_u16(bytes, 0U, 0x5a4dU);
    put_u32(bytes, 0x3cU, (uint32_t)pe);
    put_u32(bytes, pe, 0x00004550U);
    put_u16(bytes, coff, GEM_X86_64_PE_MACHINE);
    put_u16(bytes, coff + 2U, 1U);
    put_u16(bytes, coff + 16U, 0x00f0U);
    put_u16(bytes, optional, GEM_X86_64_PE32_PLUS_MAGIC);
    put_u32(bytes, optional + 16U, 0x1000U);
    put_u64(bytes, optional + 24U, UINT64_C(0x0000000140000000));
    put_u32(bytes, optional + 56U, 0x2000U);
    put_u32(bytes, optional + 60U, 0x200U);
}

static struct gem_x86_64_pe_identity identity(void) {
    struct gem_x86_64_pe_identity result;
    memset(&result, 0xa5, sizeof(result));
    result.abi_version = GEM_X86_64_ABI_VERSION;
    result.struct_size = sizeof(result);
    return result;
}

int main(void) {
    uint8_t bytes[512];
    struct gem_x86_64_pe_identity result;

    assert(gem_x86_64_abi_version() == GEM_X86_64_ABI_VERSION);
    assert(strcmp(gem_x86_64_pe_status_name(GEM_X86_64_PE_WRONG_MACHINE), "wrong-machine") == 0);
    build_pe32_plus(bytes);
    result = identity();
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), &result) == GEM_X86_64_PE_OK);
    assert(result.machine == GEM_X86_64_PE_MACHINE &&
           result.optional_magic == GEM_X86_64_PE32_PLUS_MAGIC && result.section_count == 1U &&
           result.entry_point_rva == 0x1000U && result.image_base == UINT64_C(0x140000000) &&
           result.size_of_image == 0x2000U && result.size_of_headers == 0x200U);

    result = identity();
    assert(gem_x86_64_classify_pe(bytes, 63U, &result) == GEM_X86_64_PE_TRUNCATED);
    bytes[0] = 0U;
    result = identity();
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), &result) == GEM_X86_64_PE_BAD_DOS_HEADER);

    build_pe32_plus(bytes);
    put_u16(bytes, 0x84U, 0xaa64U);
    result = identity();
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), &result) == GEM_X86_64_PE_WRONG_MACHINE);
    assert(result.machine == 0xaa64U);

    build_pe32_plus(bytes);
    put_u16(bytes, 0x98U, 0x010bU);
    result = identity();
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), &result) ==
           GEM_X86_64_PE_WRONG_OPTIONAL_MAGIC);

    build_pe32_plus(bytes);
    put_u16(bytes, 0x86U, GEM_X86_64_MAX_PE_SECTIONS + 1U);
    result = identity();
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), &result) == GEM_X86_64_PE_LIMIT_EXCEEDED);

    build_pe32_plus(bytes);
    put_u32(bytes, 0x98U + 56U, 0x800U);
    result = identity();
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), &result) == GEM_X86_64_PE_BAD_HEADER);

    result = identity();
    result.abi_version = 0U;
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), &result) == GEM_X86_64_PE_ABI_MISMATCH);
    assert(result.abi_version == 0U && result.struct_size == 0U);
    assert(gem_x86_64_classify_pe(bytes, sizeof(bytes), NULL) == GEM_X86_64_PE_INVALID_ARGUMENT);
    return 0;
}
