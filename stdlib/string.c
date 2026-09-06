/*
 * string — Komodo standard string utilities.
 *
 * Reached through the uniform Komodo call ABI (see km_runtime.h):
 * rdi = argc, rsi = KmArg records.  Every argument here is a `char *`
 * (KM_TAG_STR) or a small int; results are a fresh `char *`, a `long`
 * (0/1 for predicates, or an index), or a `KmArray *` of `char *`.
 *
 * Allocation policy matches the rest of the C stdlib (file.c's read_all,
 * list_dir): results are malloc'd and never freed — a program-lifetime
 * leak the conservative escape analysis is not asked to reason about.
 *
 * The bare `string(x)` value conversion is a separate compiler built-in
 * and has nothing to do with this module.
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "km_runtime.h"

#define STR(items, i)  ((const char *)(items)[i].value)

static char *dup_n(const char *s, size_t n)
{
    char *out = malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* string.length(s) : int  — byte length (mirrors #s). */
long string_length(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0);
    return s ? (long)strlen(s) : 0;
}

/* string.split(s, sep) : [string]
 * Split on every non-overlapping occurrence of `sep`.  An empty `sep`
 * yields the whole string as a single element. */
KmArray *string_split(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0);
    const char *sep = STR(items, 1);
    if (!s) s = "";
    if (!sep || !*sep) {
        KmArray *one = km_array_new(1);
        one->data[0] = (long)dup_n(s, strlen(s));
        one->len = 1;
        return one;
    }
    size_t seplen = strlen(sep);

    /* count pieces */
    long pieces = 1;
    for (const char *p = s; (p = strstr(p, sep)); p += seplen)
        pieces++;

    KmArray *out = km_array_new(pieces);
    long i = 0;
    const char *start = s;
    const char *hit;
    while ((hit = strstr(start, sep))) {
        out->data[i++] = (long)dup_n(start, (size_t)(hit - start));
        start = hit + seplen;
    }
    out->data[i++] = (long)dup_n(start, strlen(start));
    out->len = i;
    return out;
}

/* string.join(parts, sep) : string */
const char *string_join(long argc, KmArg *items)
{
    (void)argc;
    KmArray *parts = (KmArray *)items[0].value;
    const char *sep = STR(items, 1);
    if (!sep) sep = "";
    if (!parts || parts->len == 0) return "";
    size_t seplen = strlen(sep);
    size_t total = 0;
    for (long i = 0; i < parts->len; i++) {
        const char *p = (const char *)parts->data[i];
        total += p ? strlen(p) : 0;
        if (i + 1 < parts->len) total += seplen;
    }
    char *out = malloc(total + 1);
    char *w = out;
    for (long i = 0; i < parts->len; i++) {
        const char *p = (const char *)parts->data[i];
        size_t n = p ? strlen(p) : 0;
        memcpy(w, p, n);
        w += n;
        if (i + 1 < parts->len) { memcpy(w, sep, seplen); w += seplen; }
    }
    *w = '\0';
    return out;
}

static const char *trim_impl(const char *s, int left, int right)
{
    if (!s) return "";
    const char *b = s;
    const char *e = s + strlen(s);
    if (left)  while (b < e && isspace((unsigned char)*b)) b++;
    if (right) while (e > b && isspace((unsigned char)e[-1])) e--;
    return dup_n(b, (size_t)(e - b));
}

/* string.trim(s) / trim_start(s) / trim_end(s) : string */
const char *string_trim(long argc, KmArg *items)       { (void)argc; return trim_impl(STR(items, 0), 1, 1); }
const char *string_trim_start(long argc, KmArg *items) { (void)argc; return trim_impl(STR(items, 0), 1, 0); }
const char *string_trim_end(long argc, KmArg *items)   { (void)argc; return trim_impl(STR(items, 0), 0, 1); }

/* string.replace(s, from, to) : string  — every non-overlapping match.
 * An empty `from` returns `s` unchanged. */
const char *string_replace(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0);
    const char *from = STR(items, 1);
    const char *to = STR(items, 2);
    if (!s) return "";
    if (!from || !*from) return dup_n(s, strlen(s));
    if (!to) to = "";
    size_t flen = strlen(from), tlen = strlen(to);

    long hits = 0;
    for (const char *p = s; (p = strstr(p, from)); p += flen)
        hits++;
    if (hits == 0) return dup_n(s, strlen(s));

    size_t outlen = strlen(s) + (size_t)hits * (tlen - flen);
    char *out = malloc(outlen + 1);
    char *w = out;
    const char *start = s, *hit;
    while ((hit = strstr(start, from))) {
        size_t chunk = (size_t)(hit - start);
        memcpy(w, start, chunk); w += chunk;
        memcpy(w, to, tlen);     w += tlen;
        start = hit + flen;
    }
    strcpy(w, start);
    return out;
}

/* string.index_of(s, sub) : int  — byte offset, or -1. */
long string_index_of(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0);
    const char *sub = STR(items, 1);
    if (!s || !sub) return -1;
    const char *hit = strstr(s, sub);
    return hit ? (long)(hit - s) : -1;
}

long string_contains(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0), *sub = STR(items, 1);
    return (s && sub && strstr(s, sub)) ? 1 : 0;
}

long string_starts_with(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0), *pre = STR(items, 1);
    if (!s || !pre) return 0;
    size_t n = strlen(pre);
    return strncmp(s, pre, n) == 0 ? 1 : 0;
}

long string_ends_with(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0), *suf = STR(items, 1);
    if (!s || !suf) return 0;
    size_t sn = strlen(s), fn = strlen(suf);
    return fn <= sn && memcmp(s + sn - fn, suf, fn) == 0 ? 1 : 0;
}

static const char *map_case(const char *s, int up)
{
    if (!s) return "";
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        out[i] = up ? (char)toupper((unsigned char)s[i])
                    : (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

const char *string_to_upper(long argc, KmArg *items) { (void)argc; return map_case(STR(items, 0), 1); }
const char *string_to_lower(long argc, KmArg *items) { (void)argc; return map_case(STR(items, 0), 0); }

/* string.repeat(s, n) : string */
const char *string_repeat(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0);
    long n = items[1].value;
    if (!s || n <= 0) return "";
    size_t sn = strlen(s);
    char *out = malloc(sn * (size_t)n + 1);
    char *w = out;
    for (long i = 0; i < n; i++) { memcpy(w, s, sn); w += sn; }
    *w = '\0';
    return out;
}

/* string.reverse(s) : string  — byte-wise. */
const char *string_reverse(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0);
    if (!s) return "";
    size_t n = strlen(s);
    char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++)
        out[i] = s[n - 1 - i];
    out[n] = '\0';
    return out;
}

/* string.substring(s, start, end) : string  — [start, end) byte range,
 * clamped to bounds; negative indices count from the end. */
const char *string_substring(long argc, KmArg *items)
{
    (void)argc;
    const char *s = STR(items, 0);
    if (!s) return "";
    long n = (long)strlen(s);
    long a = items[1].value, b = items[2].value;
    if (a < 0) a += n;
    if (b < 0) b += n;
    if (a < 0) a = 0;
    if (b > n) b = n;
    if (a >= b) return "";
    return dup_n(s + a, (size_t)(b - a));
}

static const char *pad_impl(const char *s, long width, const char *pad, int start)
{
    if (!s) s = "";
    if (!pad || !*pad) pad = " ";
    long sn = (long)strlen(s);
    if (sn >= width) return dup_n(s, (size_t)sn);
    long need = width - sn;
    long pn = (long)strlen(pad);
    char *fill = malloc((size_t)need + 1);
    for (long i = 0; i < need; i++)
        fill[i] = pad[i % pn];
    fill[need] = '\0';
    char *out = malloc((size_t)width + 1);
    if (start) { strcpy(out, fill); strcat(out, s); }
    else       { strcpy(out, s); strcat(out, fill); }
    free(fill);
    return out;
}

const char *string_pad_start(long argc, KmArg *items)
{
    (void)argc;
    return pad_impl(STR(items, 0), items[1].value, STR(items, 2), 1);
}
const char *string_pad_end(long argc, KmArg *items)
{
    (void)argc;
    return pad_impl(STR(items, 0), items[1].value, STR(items, 2), 0);
}
