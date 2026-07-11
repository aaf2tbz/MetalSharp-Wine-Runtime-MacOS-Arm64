// SPDX-License-Identifier: Apache-2.0
#include "fixture_api.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static int fail(const char *message) {
    fprintf(stderr, "ARM64X fixture failure: %s\n", message);
    return 1;
}

int main(void) {
    USHORT process_machine = 0;
    USHORT native_machine = 0;
    arm64x_fixture_pair input = {31, 47};
    arm64x_fixture_pair output;

    if (!IsWow64Process2(GetCurrentProcess(), &process_machine, &native_machine))
        return fail("IsWow64Process2 failed");
    if (native_machine != IMAGE_FILE_MACHINE_ARM64)
        return fail("host OS is not native ARM64");
    if (fixture_integer(0x100) != 0x1334)
        return fail("integer result");
    if (fabs(fixture_floating(2.5) - 4.0) > 0.000001)
        return fail("floating result");
    output = fixture_aggregate(input);
    if (output.first != 58 || output.second != 22)
        return fail("aggregate result");
    if (fixture_variadic(4, 3, -5, 17, 9) != 24)
        return fail("variadic result");
    if (fixture_indirect_x64(12) != 43)
        return fail("indirect x64 integer result");
    if (fabs(fixture_indirect_x64_floating(2.5) - 4.5) > 0.000001)
        return fail("indirect x64 floating result");
    output = fixture_indirect_x64_aggregate(input);
    if (output.first != 44 || output.second != 36)
        return fail("indirect x64 aggregate result");
    if (fixture_indirect_x64_variadic(4, 3, -5, 17, 9) != 24)
        return fail("indirect x64 variadic result");
    if (fixture_import_probe() != 1u)
        return fail("import probe");
    printf("ARM64X linked fixture native execution passed\n");
    return 0;
}
