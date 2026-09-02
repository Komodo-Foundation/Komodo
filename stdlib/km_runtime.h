/*
 * km_runtime.h — the ABI shared between the Komodo compiler's generated
 * assembly and the C standard-library implementations.
 */
#ifndef KM_RUNTIME_H
#define KM_RUNTIME_H

/*
 * Every Komodo array value is a pointer to this header followed by
 * `cap` contiguous 64-bit slots.  `len` slots are live.
 */
typedef struct KmArray {
    long len;
    long cap;
    long data[];
} KmArray;

/*
 * Uniform Komodo call ABI.
 *
 * Every function written `f(a, b, ...)` in Komodo source — user code and
 * standard library alike — is called the same way:
 *
 *     rdi = argc
 *     rsi = pointer to argc KmArg records
 *
 * `value` holds the raw 64-bit payload: an int as itself, a float as its
 * IEEE-754 bit pattern, a string as a `char *`, a bool as 0 / 1.
 * The single return value comes back in rax (or xmm0 for a float).
 *
 * A standard-library function reads the arguments it expects out of
 * `items` by index; the compiler is responsible for the tags.
 */
typedef struct KmArg {
    long tag;
    long value;
} KmArg;

enum {
    KM_TAG_INT   = 0,
    KM_TAG_FLOAT = 1,
    KM_TAG_STR   = 2,
    KM_TAG_BOOL  = 3
};

/* Reinterpret a raw argument word as a double / from a double. */
static inline double km_as_float(long bits)
{
    double d;
    __builtin_memcpy(&d, &bits, sizeof d);
    return d;
}
static inline long km_from_float(double d)
{
    long bits;
    __builtin_memcpy(&bits, &d, sizeof bits);
    return bits;
}

/* ── compiler-internal runtime (operator lowerings, not Komodo calls) ── */
KmArray *km_array_new(long len);
KmArray *km_array_append(KmArray *dst, KmArray *src);
KmArray *km_array_slice(KmArray *a, long start, long end);

/* string operators:  a + b   #s   s * n   a == b */
const char *km_str_concat(const char *a, const char *b);
const char *km_str_repeat(const char *s, long n);
long        km_str_len(const char *s);
long        km_str_eq(const char *a, const char *b);
const char *km_str_slice(const char *s, long start, long end);

/* ── module: console ── */
void console_print(long argc, KmArg *items);
void console_write(long argc, KmArg *items);
void console_error(long argc, KmArg *items);

/* ── module: algorithm ── */
void algorithm_sort(long argc, KmArg *items);
void algorithm_reverse(long argc, KmArg *items);
long algorithm_sum(long argc, KmArg *items);
long algorithm_max(long argc, KmArg *items);
long algorithm_min(long argc, KmArg *items);
long algorithm_contains(long argc, KmArg *items);
long algorithm_index_of(long argc, KmArg *items);
void algorithm_fill(long argc, KmArg *items);

/* ── module: math ── */
long   math_abs(long argc, KmArg *items);
long   math_min(long argc, KmArg *items);
long   math_max(long argc, KmArg *items);
long   math_pow(long argc, KmArg *items);
long   math_gcd(long argc, KmArg *items);
double math_sqrt(long argc, KmArg *items);

#endif /* KM_RUNTIME_H */
