/*
 * runtime — primitives the compiler lowers operators to (not Komodo
 * calls).  Array literals are built inline with a raw `malloc`; anything
 * that has to grow, copy or build a value goes through here.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "km_runtime.h"

KmArray *km_array_new(long len)
{
    KmArray *a = malloc(sizeof(KmArray) + (size_t)len * sizeof(long));
    a->len = len;
    a->cap = len;
    return a;
}

/* `a + b` — returns a fresh array holding a's elements then b's; both
 * operands are left untouched (used where the left side may be aliased). */
KmArray *km_array_append(KmArray *dst, KmArray *src)
{
    KmArray *out = km_array_new(dst->len + src->len);
    memcpy(out->data, dst->data, (size_t)dst->len * sizeof(long));
    memcpy(out->data + dst->len, src->data, (size_t)src->len * sizeof(long));
    return out;
}

/* `dst += src` — grow `dst` in place with amortised doubling and return
 * it (the pointer may move via realloc).  The compound-assignment
 * lowerings overwrite the old `dst` slot with the result, so in-place
 * reuse is safe there and turns an O(n^2) build into O(n). */
KmArray *km_array_grow(KmArray *dst, KmArray *src)
{
    long need = dst->len + src->len;
    if (need > dst->cap) {
        long ncap = dst->cap ? dst->cap * 2 : 4;
        if (ncap < need)
            ncap = need;
        dst = realloc(dst, sizeof(KmArray) + (size_t)ncap * sizeof(long));
        dst->cap = ncap;
    }
    memcpy(dst->data + dst->len, src->data, (size_t)src->len * sizeof(long));
    dst->len = need;
    return dst;
}

/* `join(parts, sep)` — concatenate an array of strings with `sep`
 * between them, in one pass / one allocation.  Lets a caller build a
 * large text (e.g. the assembler output) without the O(n^2) churn of
 * repeated `s = s + line`. */
const char *km_str_join(KmArray *parts, const char *sep)
{
    if (!sep)
        sep = "";
    size_t slen = strlen(sep);
    size_t total = 0;
    for (long i = 0; i < parts->len; i++) {
        const char *p = (const char *)parts->data[i];
        total += p ? strlen(p) : 0;
        if (i + 1 < parts->len)
            total += slen;
    }
    char *out = malloc(total + 1);
    char *w = out;
    for (long i = 0; i < parts->len; i++) {
        const char *p = (const char *)parts->data[i];
        if (p) {
            size_t l = strlen(p);
            memcpy(w, p, l);
            w += l;
        }
        if (i + 1 < parts->len) {
            memcpy(w, sep, slen);
            w += slen;
        }
    }
    *w = '\0';
    return out;
}

/* `arr[start to end]` — a fresh array holding the half-open range.
 * `end < 0` (or past the end) means "to the length"; bounds are clamped. */
KmArray *km_array_slice(KmArray *a, long start, long end)
{
    long n = a->len;
    if (end < 0 || end > n)
        end = n;
    if (start < 0)
        start = 0;
    if (start > end)
        start = end;
    long len = end - start;
    KmArray *out = km_array_new(len);
    memcpy(out->data, a->data + start, (size_t)len * sizeof(long));
    return out;
}

/* ── string operators ── */

/* `s[start to end]` — a fresh NUL-terminated substring, same clamping. */
const char *km_str_slice(const char *s, long start, long end)
{
    if (!s) s = "";
    long n = (long)strlen(s);
    if (end < 0 || end > n)
        end = n;
    if (start < 0)
        start = 0;
    if (start > end)
        start = end;
    long len = end - start;
    char *out = malloc((size_t)len + 1);
    memcpy(out, s + start, (size_t)len);
    out[len] = '\0';
    return out;
}

/* `a + b` */
const char *km_str_concat(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *out = malloc(la + lb + 1);
    memcpy(out, a, la);
    memcpy(out + la, b, lb + 1);
    return out;
}

/* `s * n` */
const char *km_str_repeat(const char *s, long n)
{
    if (n < 0)
        n = 0;
    size_t len = strlen(s);
    char *out = malloc(len * (size_t)n + 1);
    char *p = out;
    for (long i = 0; i < n; i++, p += len)
        memcpy(p, s, len);
    *p = '\0';
    return out;
}

/* `#s` */
long km_str_len(const char *s)
{
    return s ? (long)strlen(s) : 0;
}

/* `a == b` */
long km_str_eq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

/* `string(x)` — tag is a compile-time constant (0 int / 1 float-bits /
 * 2 string / 3 bool); value is the raw payload word. */
const char *km_to_string(long tag, long value)
{
    char buf[64];
    switch (tag) {
    case 1: {
        double d;
        __builtin_memcpy(&d, &value, sizeof d);
        snprintf(buf, sizeof buf, "%g", d);
        break;
    }
    case 2:
        return (const char *)value;
    case 3:
        return value ? "true" : "false";
    default:
        snprintf(buf, sizeof buf, "%ld", value);
        break;
    }
    char *out = malloc(strlen(buf) + 1);
    strcpy(out, buf);
    return out;
}

/* ── process entry ──────────────────────────────────────────────────
 * The compiler names a program's entry point `km_program_main` and
 * expects its (optional) `args` parameter as a Komodo string array
 * passed through the uniform call ABI.  This shim marshals the C argv
 * into that array and invokes it. */
long km_program_main(long argc, KmArg *items);

int main(int argc, char **argv)
{
    /* Resolve argv[0] to the executable's real path so a program can
     * locate files that ship next to it (e.g. the compiler's stdlib/)
     * regardless of how it was invoked ("./kmc", bare "kmc" via PATH…). */
    static char exe[4096];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        if (argc > 0)
            argv[0] = exe;
    }

    KmArray *a = km_array_new(argc);
    for (int i = 0; i < argc; i++)
        a->data[i] = (long)argv[i];
    KmArg arg = { KM_TAG_INT, (long)a };
    km_program_main(1, &arg);
    return 0;
}

/* `s[i]` — a fresh one-character string (a Komodo `char`). */
const char *km_str_char(const char *s, long i)
{
    if (!s) return "";
    char *out = malloc(2);
    out[0] = s[i];
    out[1] = '\0';
    return out;
}

/* `s1 <op> s2` ordering — like C strcmp. */
long km_str_cmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b);
}
