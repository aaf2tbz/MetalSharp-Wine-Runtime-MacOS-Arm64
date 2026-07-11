// SPDX-License-Identifier: Apache-2.0
#include "fixture_api.h"

#include <stdarg.h>

int32_t fixture_x64_target(int32_t value) {
    return value * 3 + 7;
}

double fixture_x64_floating_target(double value) {
    return value * 2.0 - 0.5;
}

arm64x_fixture_pair fixture_x64_aggregate_target(arm64x_fixture_pair value) {
    arm64x_fixture_pair result = {value.second - 3, value.first + 5};
    return result;
}

int64_t fixture_x64_variadic_target(uint32_t count, ...) {
    int64_t total = 0;
    uint32_t index;
    va_list values;
    va_start(values, count);
    for (index = 0; index < count; ++index)
        total += va_arg(values, int32_t);
    va_end(values);
    return total;
}
