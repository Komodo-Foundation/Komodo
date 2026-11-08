#include <stdint.h>
#include <stdio.h>
#include "export_roundtrip.h"

int64_t c_apply(int64_t n) {
    printf("C says: %s\n", km_describe(n));
    return km_square(n) + 1;
}
