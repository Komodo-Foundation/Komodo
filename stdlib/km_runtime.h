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
    KM_TAG_BOOL  = 3,
    KM_TAG_ARRAY = 4
};

/*
 * An array-valued argument or return packs its element description into
 * the tag word alongside KM_TAG_ARRAY:
 *
 *     bits  0..7   KM_TAG_ARRAY
 *     bits  8..15  leaf scalar tag (KM_TAG_INT / FLOAT / STR / BOOL)
 *     bits 16..23  nesting depth (1 for [int], 2 for [[int]], …)
 *
 * so  tag = KM_TAG_ARRAY + (leaf << 8) + (depth << 16)  and `value` is
 * the KmArray*.  The compiler builds this in codegen_expr.kmm's
 * arrayTag(); the consumers here dispatch on `tag & 0xFF`.
 */

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

/*
 * Memory (runtime.c).  Every heap value — object, array, string — lives
 * on the collected heap and is reclaimed once nothing reaches it; nothing
 * is freed by hand.  A C stdlib function builds its results with these
 * (never libc malloc, whose blocks the collector cannot see into):
 *
 *   km_alloc        — zeroed memory that may hold pointers (arrays, objects)
 *   km_alloc_atomic — pointer-free bytes (string text); not zeroed
 *   km_strndup      — a collected NUL-terminated copy of n bytes
 *
 * None of them returns NULL: running out of memory (or into the heap
 * ceiling, KOMODO_MAX_HEAP) ends the program with a message, status 3.
 */
#include <stddef.h>
void *km_alloc(size_t n);
void *km_alloc_atomic(size_t n);
char *km_strndup(const char *s, size_t n);
void  km_fmt_bytes(char *buf, size_t cap, size_t n);

/* Kept only for code built by a pre-collector compiler: no-ops. */
void km_retain(void *handle);
void km_release(void *handle);
void km_drop_array(void *handle);

/* Crash safety (km_error.c).  `km_crash_init` installs the handlers that
 * turn SIGSEGV / SIGBUS / SIGFPE / SIGILL / SIGABRT — and a stack
 * overflow — into a readable message plus a Komodo stack trace.
 * `km_komodo_caller` names the innermost Komodo function on the current
 * call stack ("" when there is none).  `km_index_error` is where an
 * out-of-range `arr[i]` lands: it throws a Komodo error. */
void km_crash_init(void);
void km_komodo_caller(char *buf, size_t cap);
void km_index_error(long index, KmArray *a);

/* ── compiler-internal runtime (operator lowerings, not Komodo calls) ── */
KmArray *km_array_new(long len);
KmArray *km_array_append(KmArray *dst, KmArray *src);
KmArray *km_array_grow(KmArray *dst, KmArray *src);
KmArray *km_array_repeat(KmArray *a, long n);
const char *km_str_join(KmArray *parts, const char *sep);
KmArray *km_array_slice(KmArray *a, long start, long end);
const char *km_array_to_string(KmArray *a, long leaf, long depth);

/* error handling — `throw msg` and the `f(args)?` protected-call guard.
 * `km_try_push` registers a jmp_buf reserved on the caller's stack;
 * `km_throw` _longjmps to the innermost registered pad (popping it), or
 * prints "Uncaught error: <msg>" and exit(1)s when there is none.  The
 * generated code calls `_setjmp` itself. */
struct __jmp_buf_tag;
void        km_try_push(struct __jmp_buf_tag *b);
void        km_try_pop(void);
void        km_throw(const char *msg);
const char *km_error_message(void);

/* string operators:  a + b   #s   s * n   a == b */
const char *km_str_concat(const char *a, const char *b);
const char *km_str_repeat(const char *s, long n);
long        km_str_len(const char *s);
long        km_str_eq(const char *a, const char *b);
const char *km_str_slice(const char *s, long start, long end);
const char *km_str_char(const char *s, long i);
long        km_str_cmp(const char *a, const char *b);
long        km_str_hash(const char *s);
long        km_str_find(const char *s, const char *sub);
const char *km_str_sub(const char *s, long n, long start, long end);
/* `x in coll` membership — 1 / 0 */
long        km_in_str(const char *haystack, const char *needle);
long        km_in_array(KmArray *a, long v, long leaf);
const char *km_to_string(long tag, long value);
/* "%g" for a double, but integral values keep a ".0" (Komodo float text) */
void        km_fmt_double(char *buf, unsigned long cap, double d);

/* ── module: console ── */
void console_print(long argc, KmArg *items);
void console_write(long argc, KmArg *items);
void console_error(long argc, KmArg *items);

/* ── module: algorithm (C primitives; public overloads live in the .kmm) ── */
void   km_algo_sort(long argc, KmArg *items);
void   km_algo_reverse(long argc, KmArg *items);
long   km_algo_sum(long argc, KmArg *items);
long   km_algo_max(long argc, KmArg *items);
long   km_algo_min(long argc, KmArg *items);
long   km_algo_contains(long argc, KmArg *items);
long   km_algo_index_of(long argc, KmArg *items);
void   km_algo_fill(long argc, KmArg *items);
void   km_algo_sortf(long argc, KmArg *items);
double km_algo_sumf(long argc, KmArg *items);
double km_algo_maxf(long argc, KmArg *items);
double km_algo_minf(long argc, KmArg *items);
void   km_algo_reversef(long argc, KmArg *items);
void   km_algo_fillf(long argc, KmArg *items);
long   km_algo_index_off(long argc, KmArg *items);
long   km_algo_containsf(long argc, KmArg *items);

/* ── module: math ── */
long   math_abs(long argc, KmArg *items);
long   math_min(long argc, KmArg *items);
long   math_max(long argc, KmArg *items);
long   math_pow(long argc, KmArg *items);
long   math_gcd(long argc, KmArg *items);
long   math_clamp(long argc, KmArg *items);
long   math_sign(long argc, KmArg *items);
double math_sqrt(long argc, KmArg *items);
double math_floor(long argc, KmArg *items);
double math_ceil(long argc, KmArg *items);
double math_round(long argc, KmArg *items);
double math_trunc(long argc, KmArg *items);
double math_fabs(long argc, KmArg *items);
double math_sin(long argc, KmArg *items);
double math_cos(long argc, KmArg *items);
double math_tan(long argc, KmArg *items);
double math_atan(long argc, KmArg *items);
double math_atan2(long argc, KmArg *items);
double math_exp(long argc, KmArg *items);
double math_log(long argc, KmArg *items);
double math_log2(long argc, KmArg *items);
double math_log10(long argc, KmArg *items);
double math_powf(long argc, KmArg *items);
double math_hypot(long argc, KmArg *items);
double math_pi(long argc, KmArg *items);
double math_e(long argc, KmArg *items);

/* ── module: file (extra) ── */
long      is_dir(long argc, KmArg *items);
void      append_all(long argc, KmArg *items);
KmArray  *read_lines(long argc, KmArg *items);

/* ── module: string ── */
long        string_length(long argc, KmArg *items);
KmArray    *string_split(long argc, KmArg *items);
const char *string_join(long argc, KmArg *items);
const char *string_trim(long argc, KmArg *items);
const char *string_trim_start(long argc, KmArg *items);
const char *string_trim_end(long argc, KmArg *items);
const char *string_replace(long argc, KmArg *items);
long        string_index_of(long argc, KmArg *items);
long        string_contains(long argc, KmArg *items);
long        string_starts_with(long argc, KmArg *items);
long        string_ends_with(long argc, KmArg *items);
const char *string_to_upper(long argc, KmArg *items);
const char *string_to_lower(long argc, KmArg *items);
const char *string_repeat(long argc, KmArg *items);
const char *string_reverse(long argc, KmArg *items);
const char *string_substring(long argc, KmArg *items);
const char *string_pad_start(long argc, KmArg *items);
const char *string_pad_end(long argc, KmArg *items);

#endif /* KM_RUNTIME_H */
