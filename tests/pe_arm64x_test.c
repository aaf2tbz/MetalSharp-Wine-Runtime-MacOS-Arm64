// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/pe_arm64x.h"
#include "pe_arm64x_fixture_builder.h"
#include <assert.h>
#include <string.h>

static void check(uint32_t version) {
    struct pe_arm64x_fixture f;
    struct gem_pe_arm64x_image *i = NULL;
    struct gem_pe_arm64x_summary s;
    struct gem_pe_arm64x_rva_info info;
    assert(pe_arm64x_fixture_build(version, &f));
    assert(gem_pe_arm64x_parse(f.bytes, f.size, NULL, &i) == GEM_PE_OK);
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
