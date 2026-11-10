/*
 * path — Komodo standard path manipulation, reached through the uniform
 * call ABI (rdi = argc, rsi = KmArg records).  The string functions are
 * pure lexical work (no filesystem access) except path_abs, which reads
 * the working directory, and path_exists, which stats the path.
 */
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include "km_runtime.h"

#define ARG(i) (KM_STR(items[i].value) ? KM_STR(items[i].value) : "")

static const char *out_copy(const char *s, size_t n)
{
    return km_strndup(s, n);
}

/* path.join(a, b) : string — `b` replaces the whole path when absolute. */
const char *path_join(int64_t argc, KmArg *items)
{
    if (argc < 2) return out_copy("", 0);
    const char *a = ARG(0), *b = ARG(1);
    size_t al = strlen(a), bl = strlen(b);
    if (al == 0) return out_copy(b, bl);
    if (bl == 0) return out_copy(a, al);
    if (b[0] == '/') return out_copy(b, bl);
    int sep = a[al - 1] != '/';
    char *buf = km_alloc_atomic(al + (size_t)sep + bl + 1);
    memcpy(buf, a, al);
    if (sep) buf[al++] = '/';
    memcpy(buf + al, b, bl);
    buf[al + bl] = '\0';
    return buf;
}

/* path.dirname(p) : string — everything before the last component, with
 * trailing slashes removed ("." when there is none, "/" for the root). */
const char *path_dirname(int64_t argc, KmArg *items)
{
    if (argc < 1) return out_copy(".", 1);
    const char *p = ARG(0);
    size_t len = strlen(p);
    if (len == 0) return out_copy(".", 1);
    while (len > 0 && p[len - 1] == '/') len--;
    if (len == 0) return out_copy("/", 1);          /* p was all slashes */
    size_t i = len;
    while (i > 0 && p[i - 1] != '/') i--;
    if (i == 0) return out_copy(".", 1);          /* bare name */
    while (i > 0 && p[i - 1] == '/') i--;
    if (i == 0) return out_copy("/", 1);           /* child of root */
    return out_copy(p, i);
}

/* path.basename(p) : string — the last component; trailing slashes ignored. */
const char *path_basename(int64_t argc, KmArg *items)
{
    if (argc < 1) return out_copy("", 0);
    const char *p = ARG(0);
    size_t end = strlen(p);
    while (end > 0 && p[end - 1] == '/') end--;
    size_t start = end;
    while (start > 0 && p[start - 1] != '/') start--;
    return out_copy(p + start, end - start);
}

/* path.ext(p) : string — extension of the basename, including the dot
 * (".gz" for "a.tar.gz").  Leading dots do not start an extension, so
 * ".bashrc" and "README" have none: "". */
const char *path_ext(int64_t argc, KmArg *items)
{
    const char *base = path_basename(argc, items);
    size_t bl = strlen(base);
    const char *dot = NULL;
    for (size_t i = 0; i < bl; i++)
        if (base[i] == '.') dot = base + i;
    if (!dot || dot == base) return out_copy("", 0);
    return out_copy(dot, (size_t)(base + bl - dot));
}

/* path.abs(p) : string — absolute form: relative paths are taken against
 * the working directory, then "." and ".." components are resolved
 * lexically.  Nothing has to exist. */
const char *path_abs(int64_t argc, KmArg *items)
{
    if (argc < 1) return out_copy("", 0);
    const char *p = ARG(0);
    char cwd[PATH_MAX];
    const char *full;
    if (p[0] == '/') {
        full = p;
    } else {
        if (!getcwd(cwd, sizeof cwd)) return out_copy("", 0);
        const char *joined = path_join(2, (KmArg[]){ {KM_TAG_STR, KM_WORD(cwd)},
                                                    {KM_TAG_STR, KM_WORD(p)} });
        full = joined;
    }
    size_t n = strlen(full);
    char *out = km_alloc_atomic(n + 2);
    size_t len = 0;                     /* out holds "/a/b" style text */
    size_t i = 0;
    while (i <= n) {
        size_t s = i;
        while (i < n && full[i] != '/') i++;
        size_t cl = i - s;
        if (cl == 0 || (cl == 1 && full[s] == '.')) {
            /* skip empty and "." components */
        } else if (cl == 2 && full[s] == '.' && full[s + 1] == '.') {
            while (len > 0 && out[len - 1] != '/') len--;
            if (len > 0) len--;
        } else {
            out[len++] = '/';
            memcpy(out + len, full + s, cl);
            len += cl;
        }
        i++;
    }
    if (len == 0) out[len++] = '/';
    out[len] = '\0';
    return out;
}

/* path.exists(p) : int — 1 when anything (file, directory, ...) is at `p`. */
int64_t path_exists(int64_t argc, KmArg *items)
{
    if (argc < 1) return 0;
    struct stat st;
    return stat(ARG(0), &st) == 0;
}
