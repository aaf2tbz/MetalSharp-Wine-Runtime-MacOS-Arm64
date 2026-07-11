// SPDX-License-Identifier: Apache-2.0
/* Legal source fixture: LLVM ARM64EC lowering emits the entry/exit/checker thunks. */
#include <stdarg.h>

struct pair {
    long long a, b;
};
extern int ext_integer(int);
extern double ext_float(double);
extern struct pair ext_pair(struct pair);
extern int ext_variadic(const char *, ...);

int fixture_integer(int value) {
    return ext_integer(value);
}
double fixture_float(double value) {
    return ext_float(value);
}
struct pair fixture_pair(struct pair value) {
    return ext_pair(value);
}
int fixture_variadic(int value) {
    return ext_variadic("%d", value);
}
