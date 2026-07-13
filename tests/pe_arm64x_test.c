// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/pe_arm64x.h"
#include "pe_arm64x_fixture_builder.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void check_mapped(const struct pe_arm64x_fixture *fixture, uint32_t version) {
    const uint64_t loaded_base = UINT64_C(0x0000000200000000);
    struct gem_pe_arm64x_image *image = NULL;
    struct gem_pe_arm64x_summary summary;
    struct gem_pe_arm64x_rva_info info;
    uint8_t *mapped = calloc(1U, PE_ARM64X_FIXTURE_SIZE_OF_IMAGE);
    assert(mapped != NULL);
    memcpy(mapped, fixture->bytes, 0x400U);
    memcpy(mapped + PE_ARM64X_FIXTURE_TEXT_RVA, fixture->bytes + 0x400U,
           PE_ARM64X_FIXTURE_TEXT_SIZE);
    memcpy(mapped + PE_ARM64X_FIXTURE_RDATA_RVA, fixture->bytes + 0x2400U,
           PE_ARM64X_FIXTURE_RDATA_SIZE);
    pe_arm64x_fixture_put_u64(mapped, PE_ARM64X_FIXTURE_LOAD_CONFIG_RVA + 0xc8U,
                              loaded_base + PE_ARM64X_FIXTURE_CHPE_RVA);
    /* Wine selects the ARM64EC constituent when it maps an ARM64X image. */
    pe_arm64x_fixture_put_u16(mapped, 0x84U, 0xa641U);

    assert(gem_pe_arm64x_parse_mapped(mapped, PE_ARM64X_FIXTURE_SIZE_OF_IMAGE, loaded_base, NULL,
                                      &image) == GEM_PE_OK);
    assert(gem_pe_arm64x_get_summary(image, &summary) == GEM_PE_OK && summary.machine == 0xa641U &&
           summary.chpe_metadata_version == version);
    assert(gem_pe_arm64x_classify_rva(image, 0x1900U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_X64);
    gem_pe_arm64x_image_destroy(image);
    image = NULL;

    /* Wine can also publish the mapped constituent as AMD64 while preserving
     * the same relocated CHPE metadata. */
    pe_arm64x_fixture_put_u16(mapped, 0x84U, 0x8664U);
    assert(gem_pe_arm64x_parse_mapped(mapped, PE_ARM64X_FIXTURE_SIZE_OF_IMAGE, loaded_base, NULL,
                                      &image) == GEM_PE_OK);
    assert(gem_pe_arm64x_get_summary(image, &summary) == GEM_PE_OK && summary.machine == 0x8664U &&
           summary.chpe_metadata_version == version);
    gem_pe_arm64x_image_destroy(image);
    image = NULL;

    pe_arm64x_fixture_put_u64(mapped, PE_ARM64X_FIXTURE_LOAD_CONFIG_RVA + 0xc8U, loaded_base - 1U);
    assert(gem_pe_arm64x_parse_mapped(mapped, PE_ARM64X_FIXTURE_SIZE_OF_IMAGE, loaded_base, NULL,
                                      &image) == GEM_PE_ERROR_BAD_LOAD_CONFIG);
    assert(image == NULL);
    free(mapped);
}

static void check(uint32_t version) {
    struct pe_arm64x_fixture f;
    struct gem_pe_arm64x_image *i = NULL;
    struct gem_pe_arm64x_summary s;
    struct gem_pe_arm64x_rva_info info;
    assert(pe_arm64x_fixture_build(version, &f));
    assert(gem_pe_arm64x_parse(f.bytes, f.size, NULL, &i) == GEM_PE_OK);
    check_mapped(&f, version);
    assert(gem_pe_arm64x_get_summary(i, &s) == GEM_PE_OK && s.chpe_metadata_version == version &&
           s.code_range_count == 3U && s.entry_range_count == 1U && s.redirection_count == 1U);
    assert(gem_pe_arm64x_classify_rva(i, 0x1000U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_ARM64);
    assert(gem_pe_arm64x_classify_rva(i, 0x1400U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_ARM64EC);
    assert(gem_pe_arm64x_classify_rva(i, 0x1800U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_FAST_FORWARD);
    assert(gem_pe_arm64x_classify_rva(i, 0x1801U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_THUNK);
    assert(gem_pe_arm64x_classify_rva(i, 0x1900U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_X64);
    assert(gem_pe_arm64x_classify_rva(i, 0x3000U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_DATA);
    assert(gem_pe_arm64x_classify_rva(i, 0x4000U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_INVALID);
    memset(f.bytes, 0xa5, f.size);
    pe_arm64x_fixture_destroy(&f);
    assert(gem_pe_arm64x_classify_rva(i, 0x1400U, &info) == GEM_PE_OK &&
           info.classification == GEM_PE_RVA_ARM64EC);
    gem_pe_arm64x_image_destroy(i);
}
int main(void) {
    check(1U);
    check(2U);
    return 0;
}
