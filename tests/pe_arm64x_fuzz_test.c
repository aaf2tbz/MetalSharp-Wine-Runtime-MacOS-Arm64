// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/pe_arm64x.h"
#include "pe_arm64x_fixture_builder.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    struct pe_arm64x_fixture seed;
    struct pe_arm64x_fixture work;
    uint32_t state = 0x12345678U;
    size_t iteration;

    assert(pe_arm64x_fixture_build(2U, &seed));
    assert(pe_arm64x_fixture_clone(&seed, &work));
    for (iteration = 0U; iteration < 4000U; ++iteration) {
        struct gem_pe_arm64x_image *image = NULL;
        size_t mutation;
        size_t length;
        size_t deep;
        enum gem_pe_status status;

        memcpy(work.bytes, seed.bytes, seed.size);
        state = state * 1664525U + 1013904223U;
        mutation = (size_t)state % work.size;
        work.bytes[mutation] ^= (uint8_t)(state >> 24U);
        state = state * 1664525U + 1013904223U;
        deep = pe_arm64x_fixture_rva_to_offset(PE_ARM64X_FIXTURE_CHPE_RVA) +
               ((size_t)(state % 23U) * 4U);
        work.bytes[deep] ^= (uint8_t)(state >> 16U);
        state = state * 1664525U + 1013904223U;
        length = (size_t)state % (seed.size + 1U);
        status = gem_pe_arm64x_parse(work.bytes, length, NULL, &image);
        if (status == GEM_PE_OK)
            gem_pe_arm64x_image_destroy(image);
    }
    pe_arm64x_fixture_destroy(&work);
    pe_arm64x_fixture_destroy(&seed);
    return 0;
}
