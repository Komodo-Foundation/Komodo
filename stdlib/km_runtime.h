/*
 * km_runtime.h — the ABI shared between the Komodo compiler's generated
 * assembly and the C standard-library implementations.
 */
#ifndef KM_RUNTIME_H
#define KM_RUNTIME_H

#include <stdint.h>
#include <inttypes.h>

/*
 * Word size.  A Komodo value is always a 64-bit word (an int as itself, a
 * float as its IEEE-754 bits, a string / array / object as a pointer), and
 * every layout below is spelled with int64_t so it holds on targets where
 * C `long` is not 64 bits (WebAssembly's wasm32).  A pointer stored in a
 * word is zero-extended (KM_WORD) and recovered by truncation (KM_PTR);
 * on x86-64 both are the identity.
 */
#define KM_WORD(p)    ((int64_t)(uintptr_t)(p))
#define KM_PTR(T, w)  ((T)(uintptr_t)(w))
#define KM_STR(w)     KM_PTR(const char *, w)
#define KM_ARR(w)     KM_PTR(KmArray *, w)

/*
 * Every Komodo array value is a pointer to this header followed by
 * `cap` contiguous 64-bit slots.  `len` slots are live.
 */
typedef struct KmArray {
    int64_t len;
    int64_t cap;
    int64_t data[];
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
    int64_t tag;
    int64_t value;
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
static inline double km_as_float(int64_t bits)
{
    double d;
    __builtin_memcpy(&d, &bits, sizeof d);
    return d;
}
static inline int64_t km_from_float(double d)
{
    int64_t bits;
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
void km_index_error(int64_t index, KmArray *a);

/* ── compiler-internal runtime (operator lowerings, not Komodo calls) ── */
KmArray *km_array_new(int64_t len);
KmArray *km_array_append(KmArray *dst, KmArray *src);
KmArray *km_array_grow(KmArray *dst, KmArray *src);
KmArray *km_array_push(KmArray *dst, int64_t v);
KmArray *km_array_repeat(KmArray *a, int64_t n);
const char *km_str_join(KmArray *parts, const char *sep);
KmArray *km_array_slice(KmArray *a, int64_t start, int64_t end);
const char *km_array_to_string(KmArray *a, int64_t leaf, int64_t depth);

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
const char *km_str_repeat(const char *s, int64_t n);
int64_t        km_str_len(const char *s);
int64_t        km_str_eq(const char *a, const char *b);
const char *km_str_slice(const char *s, int64_t start, int64_t end);
const char *km_str_char(const char *s, int64_t i);
int64_t        km_str_cmp(const char *a, const char *b);
int64_t        km_str_hash(const char *s);
int64_t        km_str_find(const char *s, const char *sub);
const char *km_str_sub(const char *s, int64_t n, int64_t start, int64_t end);
/* `x in coll` membership — 1 / 0 */
int64_t        km_in_str(const char *haystack, const char *needle);
int64_t        km_in_array(KmArray *a, int64_t v, int64_t leaf);
const char *km_to_string(int64_t tag, int64_t value);
/* "%g" for a double, but integral values keep a ".0" (Komodo float text) */
void        km_fmt_double(char *buf, unsigned long cap, double d);

/* ── module: console ── */
void console_print(int64_t argc, KmArg *items);
void console_write(int64_t argc, KmArg *items);
void console_error(int64_t argc, KmArg *items);

/* ── module: algorithm (C primitives; public overloads live in the .kmm) ── */
void   km_algo_sort(int64_t argc, KmArg *items);
void   km_algo_reverse(int64_t argc, KmArg *items);
int64_t   km_algo_sum(int64_t argc, KmArg *items);
int64_t   km_algo_max(int64_t argc, KmArg *items);
int64_t   km_algo_min(int64_t argc, KmArg *items);
int64_t   km_algo_contains(int64_t argc, KmArg *items);
int64_t   km_algo_index_of(int64_t argc, KmArg *items);
void   km_algo_fill(int64_t argc, KmArg *items);
void   km_algo_sortf(int64_t argc, KmArg *items);
double km_algo_sumf(int64_t argc, KmArg *items);
double km_algo_maxf(int64_t argc, KmArg *items);
double km_algo_minf(int64_t argc, KmArg *items);
void   km_algo_reversef(int64_t argc, KmArg *items);
void   km_algo_fillf(int64_t argc, KmArg *items);
int64_t   km_algo_index_off(int64_t argc, KmArg *items);
int64_t   km_algo_containsf(int64_t argc, KmArg *items);

/* ── module: math ── */
int64_t   math_abs(int64_t argc, KmArg *items);
int64_t   math_min(int64_t argc, KmArg *items);
int64_t   math_max(int64_t argc, KmArg *items);
int64_t   math_pow(int64_t argc, KmArg *items);
int64_t   math_gcd(int64_t argc, KmArg *items);
int64_t   math_clamp(int64_t argc, KmArg *items);
int64_t   math_sign(int64_t argc, KmArg *items);
double math_sqrt(int64_t argc, KmArg *items);
double math_floor(int64_t argc, KmArg *items);
double math_ceil(int64_t argc, KmArg *items);
double math_round(int64_t argc, KmArg *items);
double math_trunc(int64_t argc, KmArg *items);
double math_fabs(int64_t argc, KmArg *items);
double math_sin(int64_t argc, KmArg *items);
double math_cos(int64_t argc, KmArg *items);
double math_tan(int64_t argc, KmArg *items);
double math_atan(int64_t argc, KmArg *items);
double math_atan2(int64_t argc, KmArg *items);
double math_exp(int64_t argc, KmArg *items);
double math_log(int64_t argc, KmArg *items);
double math_log2(int64_t argc, KmArg *items);
double math_log10(int64_t argc, KmArg *items);
double math_powf(int64_t argc, KmArg *items);
double math_hypot(int64_t argc, KmArg *items);
double math_pi(int64_t argc, KmArg *items);
double math_e(int64_t argc, KmArg *items);

/* ── module: file ── */
int64_t      exists(int64_t argc, KmArg *items);
int64_t      is_dir(int64_t argc, KmArg *items);
const char  *read_all(int64_t argc, KmArg *items);
void         write_all(int64_t argc, KmArg *items);
void      append_all(int64_t argc, KmArg *items);
KmArray  *read_lines(int64_t argc, KmArg *items);
KmArray  *list_dir(int64_t argc, KmArg *items);
const char  *find(int64_t argc, KmArg *items);
void         km_fail(const char *msg);

/* bare print(...) — alias of console.print */
void print(int64_t argc, KmArg *items);

/* ── module: string ── */
int64_t        string_length(int64_t argc, KmArg *items);
KmArray    *string_split(int64_t argc, KmArg *items);
const char *string_join(int64_t argc, KmArg *items);
const char *string_trim(int64_t argc, KmArg *items);
const char *string_trim_start(int64_t argc, KmArg *items);
const char *string_trim_end(int64_t argc, KmArg *items);
const char *string_replace(int64_t argc, KmArg *items);
int64_t        string_index_of(int64_t argc, KmArg *items);
int64_t        string_contains(int64_t argc, KmArg *items);
int64_t        string_starts_with(int64_t argc, KmArg *items);
int64_t        string_ends_with(int64_t argc, KmArg *items);
const char *string_to_upper(int64_t argc, KmArg *items);
const char *string_to_lower(int64_t argc, KmArg *items);
const char *string_repeat(int64_t argc, KmArg *items);
const char *string_reverse(int64_t argc, KmArg *items);
const char *string_substring(int64_t argc, KmArg *items);
const char *string_pad_start(int64_t argc, KmArg *items);
const char *string_pad_end(int64_t argc, KmArg *items);

/* ── module: os ── */
extern int km_main_argc;
extern char **km_main_argv;
KmArray    *os_args(int64_t argc, KmArg *items);
const char *os_getenv(int64_t argc, KmArg *items);
void        os_setenv(int64_t argc, KmArg *items);
void        os_exit(int64_t argc, KmArg *items);
const char *os_cwd(int64_t argc, KmArg *items);
int64_t     os_pid(int64_t argc, KmArg *items);
const char *os_platform(int64_t argc, KmArg *items);

/* ── module: time ── */
int64_t     time_now(int64_t argc, KmArg *items);
int64_t     time_millis(int64_t argc, KmArg *items);
void        time_sleep_ms(int64_t argc, KmArg *items);
const char *time_format(int64_t argc, KmArg *items);

/* ── module: random ── */
void        random_seed(int64_t argc, KmArg *items);
int64_t     random_int(int64_t argc, KmArg *items);
double      random_float(int64_t argc, KmArg *items);
void        random_shuffle(int64_t argc, KmArg *items);

/* ── module: path ── */
const char *path_join(int64_t argc, KmArg *items);
const char *path_dirname(int64_t argc, KmArg *items);
const char *path_basename(int64_t argc, KmArg *items);
const char *path_ext(int64_t argc, KmArg *items);
const char *path_abs(int64_t argc, KmArg *items);
int64_t     path_exists(int64_t argc, KmArg *items);

#endif /* KM_RUNTIME_H */
