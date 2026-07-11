// SPDX-License-Identifier: Apache-2.0
#ifndef TESTS_PE_ARM64X_FIXTURE_BUILDER_H
#define TESTS_PE_ARM64X_FIXTURE_BUILDER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PE_ARM64X_FIXTURE_IMAGE_BASE UINT64_C(0x0000000180000000)
#define PE_ARM64X_FIXTURE_SIZE_OF_IMAGE UINT32_C(0x4000)
#define PE_ARM64X_FIXTURE_TEXT_RVA UINT32_C(0x1000)
#define PE_ARM64X_FIXTURE_TEXT_SIZE UINT32_C(0x2000)
#define PE_ARM64X_FIXTURE_RDATA_RVA UINT32_C(0x3000)
#define PE_ARM64X_FIXTURE_RDATA_SIZE UINT32_C(0x1000)
#define PE_ARM64X_FIXTURE_LOAD_CONFIG_RVA UINT32_C(0x3000)
#define PE_ARM64X_FIXTURE_LOAD_CONFIG_SIZE UINT32_C(0x140)
#define PE_ARM64X_FIXTURE_CHPE_RVA UINT32_C(0x3100)
#define PE_ARM64X_FIXTURE_CODE_MAP_RVA UINT32_C(0x3200)
#define PE_ARM64X_FIXTURE_ENTRY_RANGES_RVA UINT32_C(0x3300)
#define PE_ARM64X_FIXTURE_REDIRECTIONS_RVA UINT32_C(0x3400)

struct pe_arm64x_fixture {
    uint8_t *bytes;
    size_t size;
    uint32_t version;
};

int pe_arm64x_fixture_build(uint32_t chpe_version, struct pe_arm64x_fixture *out_fixture);
int pe_arm64x_fixture_clone(const struct pe_arm64x_fixture *fixture,
                            struct pe_arm64x_fixture *out_fixture);
void pe_arm64x_fixture_destroy(struct pe_arm64x_fixture *fixture);
size_t pe_arm64x_fixture_rva_to_offset(uint32_t rva);
void pe_arm64x_fixture_put_u16(uint8_t *bytes, size_t offset, uint16_t value);
void pe_arm64x_fixture_put_u32(uint8_t *bytes, size_t offset, uint32_t value);
void pe_arm64x_fixture_put_u64(uint8_t *bytes, size_t offset, uint64_t value);

#ifdef __cplusplus
}
#endif

#endif
