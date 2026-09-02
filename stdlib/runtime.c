/*
 * runtime — primitives the compiler lowers operators to (not Komodo
 * calls).  Array literals are built inline with a raw `malloc`; anything
 * that has to grow, copy or build a value goes through here.
 */
#include <stdlib.h>
#include <string.h>
#include "km_runtime.h"

KmArray *km_array_new(long len)
{
    KmArray *a = malloc(sizeof(KmArray) + (size_t)len * sizeof(long));
    a->len = len;
    a->cap = len;
    return a;
}

/* `dst += src` — returns a fresh array holding dst's elements then src's. */
KmArray *km_array_append(KmArray *dst, KmArray *src)
{
    KmArray *out = km_array_new(dst->len + src->len);
    memcpy(out->data, dst->data, (size_t)dst->len * sizeof(long));
    memcpy(out->data + dst->len, src->data, (size_t)src->len * sizeof(long));
    return out;
}

/* ── string operators ── */

/* `a + b` */
const char *km_str_concat(const char *a, const char *b)
{
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
    return (long)strlen(s);
}

/* `a == b` */
long km_str_eq(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}
