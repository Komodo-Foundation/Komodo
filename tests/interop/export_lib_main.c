#include <stdio.h>
#include "export_lib.h"

int main(void) {
    printf("fib(20) = %ld\n", (long)fib(20));
    printf("clamp_f = %g %g %g\n", clamp_f(-1.0, 0.0, 1.0), clamp_f(0.5, 0.0, 1.0), clamp_f(7.0, 0.0, 1.0));
    printf("label = %s %s\n", label(0), label(9));
    printf("many = %ld\n", (long)many(1, 2, 3, 4, 5, 6, 7, 8));
    printf("mixed = %g\n", mixed(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11));
    printf("narrow = %g\n", narrow(-3, 1000, 1.5f));
    return 0;
}
