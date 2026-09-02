/*
 * math — Komodo standard numeric helpers.
 *
 * Reached through the uniform Komodo call ABI (see km_runtime.h).
 * Integer-returning routines hand the value back in rax; math_sqrt
 * returns a double in xmm0 (the compiler knows its return type).
 */
#include <math.h>
#include "km_runtime.h"

long math_abs(long argc, KmArg *items)
{
    (void)argc;
    long v = items[0].value;
    return v < 0 ? -v : v;
}

long math_min(long argc, KmArg *items)
{
    (void)argc;
    long a = items[0].value, b = items[1].value;
    return a < b ? a : b;
}

long math_max(long argc, KmArg *items)
{
    (void)argc;
    long a = items[0].value, b = items[1].value;
    return a > b ? a : b;
}

long math_pow(long argc, KmArg *items)
{
    (void)argc;
    long base = items[0].value, exp = items[1].value, result = 1;
    while (exp > 0) {
        if (exp & 1)
            result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

long math_gcd(long argc, KmArg *items)
{
    (void)argc;
    long a = items[0].value, b = items[1].value;
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) {
        long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

double math_sqrt(long argc, KmArg *items)
{
    (void)argc;
    KmArg a = items[0];
    double x = (a.tag == KM_TAG_FLOAT) ? km_as_float(a.value) : (double)a.value;
    return sqrt(x);
}
