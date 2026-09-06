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

/* Read argument `i` as a double whether it arrived tagged float or int. */
static double arg_f(KmArg *items, long i)
{
    KmArg a = items[i];
    return (a.tag == KM_TAG_FLOAT) ? km_as_float(a.value) : (double)a.value;
}

double math_sqrt(long argc, KmArg *items)
{
    (void)argc;
    return sqrt(arg_f(items, 0));
}

/* ── floating-point helpers (argument and result are `float`) ────────── */

double math_floor(long argc, KmArg *items) { (void)argc; return floor(arg_f(items, 0)); }
double math_ceil (long argc, KmArg *items) { (void)argc; return ceil (arg_f(items, 0)); }
double math_round(long argc, KmArg *items) { (void)argc; return round(arg_f(items, 0)); }
double math_trunc(long argc, KmArg *items) { (void)argc; return trunc(arg_f(items, 0)); }
double math_fabs (long argc, KmArg *items) { (void)argc; return fabs (arg_f(items, 0)); }

double math_sin(long argc, KmArg *items) { (void)argc; return sin(arg_f(items, 0)); }
double math_cos(long argc, KmArg *items) { (void)argc; return cos(arg_f(items, 0)); }
double math_tan(long argc, KmArg *items) { (void)argc; return tan(arg_f(items, 0)); }
double math_atan(long argc, KmArg *items) { (void)argc; return atan(arg_f(items, 0)); }

double math_exp  (long argc, KmArg *items) { (void)argc; return exp  (arg_f(items, 0)); }
double math_log  (long argc, KmArg *items) { (void)argc; return log  (arg_f(items, 0)); }
double math_log2 (long argc, KmArg *items) { (void)argc; return log2 (arg_f(items, 0)); }
double math_log10(long argc, KmArg *items) { (void)argc; return log10(arg_f(items, 0)); }

/* math.powf(base, exp) : float  — real exponent, unlike integer math_pow. */
double math_powf(long argc, KmArg *items)
{
    (void)argc;
    return pow(arg_f(items, 0), arg_f(items, 1));
}

/* math.hypot(a, b) : float */
double math_hypot(long argc, KmArg *items)
{
    (void)argc;
    return hypot(arg_f(items, 0), arg_f(items, 1));
}

/* math.atan2(y, x) : float */
double math_atan2(long argc, KmArg *items)
{
    (void)argc;
    return atan2(arg_f(items, 0), arg_f(items, 1));
}

/* Constants, as nullary functions. */
double math_pi(long argc, KmArg *items) { (void)argc; (void)items; return 3.14159265358979323846; }
double math_e (long argc, KmArg *items) { (void)argc; (void)items; return 2.71828182845904523536; }

/* math.clamp(x, lo, hi) : int */
long math_clamp(long argc, KmArg *items)
{
    (void)argc;
    long x = items[0].value, lo = items[1].value, hi = items[2].value;
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* math.sign(n) : int  — -1 / 0 / 1 */
long math_sign(long argc, KmArg *items)
{
    (void)argc;
    long n = items[0].value;
    return (n > 0) - (n < 0);
}
