// SPDX-License-Identifier: Apache-2.0
#include "metalsharp/gem/pe_arm64x.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    FILE *fp;
    long n;
    uint8_t *p;
    struct gem_pe_arm64x_image *image = NULL;
    struct gem_pe_arm64x_summary s;
    enum gem_pe_status st;
    if (argc != 2) {
        fprintf(stderr, "usage: %s PE-file\n", argv[0]);
        return 2;
    }
    fp = fopen(argv[1], "rb");
    if (!fp)
        return 2;
    if (fseek(fp, 0, SEEK_END) || ((n = ftell(fp)) < 0) || n > 16777216L) {
        fclose(fp);
        return 2;
    }
    rewind(fp);
    p = malloc((size_t)n);
    if (!p || fread(p, 1U, (size_t)n, fp) != (size_t)n) {
        free(p);
        fclose(fp);
        return 2;
    }
    fclose(fp);
    st = gem_pe_arm64x_parse(p, (size_t)n, NULL, &image);
    free(p);
    if (st != GEM_PE_OK) {
        fprintf(stderr, "parse: %s\n", gem_pe_status_name(st));
        return 1;
    }
    gem_pe_arm64x_get_summary(image, &s);
    printf("machine=0x%04x sections=%u image=0x%x chpe=v%u ranges=%zu entries=%zu redirects=%zu\n",
           s.machine, s.section_count, s.size_of_image, s.chpe_metadata_version, s.code_range_count,
           s.entry_range_count, s.redirection_count);
    gem_pe_arm64x_image_destroy(image);
    return 0;
}
