/*
 * file — Komodo standard file access, reached through the uniform call
 * ABI (rdi = argc, rsi = KmArg records).  Mirrors the interpreter's
 * stdlib/file.py closely enough for the self-hosted compiler to build.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "km_runtime.h"

/* The whole stream, NUL-terminated, on the collected heap; *len =
 * bytes read.  Reads until EOF instead of trusting the size the file
 * reports, so /proc files, pipes and files that grow while being read
 * come back complete. */
static char *slurp(FILE *f, size_t *len)
{
    size_t cap = 4096, n = 0;
    char *buf = km_alloc_atomic(cap);
    for (;;) {
        if (n + 1 >= cap) {
            char *nb = km_alloc_atomic(cap * 2);
            memcpy(nb, buf, n);
            buf = nb;
            cap *= 2;
        }
        size_t got = fread(buf + n, 1, cap - 1 - n, f);
        n += got;
        if (got == 0) break;
    }
    buf[n] = '\0';
    *len = n;
    return buf;
}

/* file.exists(path) : bool  — a regular file exists at `path` */
int64_t exists(int64_t argc, KmArg *items)
{
    if (argc < 1) return 0;
    struct stat st;
    const char *path = KM_STR(items[0].value);
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* file.is_dir(path) : bool */
int64_t is_dir(int64_t argc, KmArg *items)
{
    if (argc < 1) return 0;
    struct stat st;
    const char *path = KM_STR(items[0].value);
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* file.read_all(path) : string */
const char *read_all(int64_t argc, KmArg *items)
{
    if (argc < 1) return "";
    const char *path = KM_STR(items[0].value);
    FILE *f = fopen(path, "rb");
    if (!f) return "";
    size_t got = 0;
    char *buf = slurp(f, &got);
    fclose(f);
    return buf;
}

/* file.write_all(path, data) */
void write_all(int64_t argc, KmArg *items)
{
    if (argc < 2) return;
    const char *path = KM_STR(items[0].value);
    const char *data = KM_STR(items[1].value);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fputs(data, f);
    fclose(f);
}

/* file.append_all(path, data)  — create if absent, otherwise append. */
void append_all(int64_t argc, KmArg *items)
{
    if (argc < 2) return;
    const char *path = KM_STR(items[0].value);
    const char *data = KM_STR(items[1].value);
    FILE *f = fopen(path, "ab");
    if (!f) return;
    fputs(data, f);
    fclose(f);
}

/* file.read_lines(path) : [string]
 * The file split on '\n'; a trailing newline does not yield a final
 * empty element.  '\r' before a '\n' is dropped.  Empty array on error. */
KmArray *read_lines(int64_t argc, KmArg *items)
{
    if (argc < 1) return km_array_new(0);
    const char *path = KM_STR(items[0].value);
    FILE *f = fopen(path, "rb");
    if (!f) return km_array_new(0);
    size_t got = 0;
    char *buf = slurp(f, &got);
    fclose(f);

    int64_t lines = 1;
    for (size_t i = 0; i < got; i++)
        if (buf[i] == '\n') lines++;

    KmArray *out = km_array_new(lines);
    int64_t li = 0;
    size_t start = 0;
    for (size_t i = 0; i <= got; i++) {
        if (i == got || buf[i] == '\n') {
            size_t end = i;
            if (end > start && buf[end - 1] == '\r') end--;
            if (i == got && start == got) break;   /* trailing newline */
            out->data[li++] = KM_WORD(km_strndup(buf + start, end - start));
            start = i + 1;
        }
    }
    out->len = li;
    return out;
}

/* file.list_dir(path) : [string]  — entry names in `path` (no "."/"..",
 * unsorted); empty array when `path` is not a directory. */
KmArray *list_dir(int64_t argc, KmArg *items)
{
    const char *path = argc >= 1 ? KM_STR(items[0].value) : ".";
    if (!path || !*path) path = ".";
    DIR *d = opendir(path);
    if (!d) return km_array_new(0);
    struct dirent *ent;
    int64_t n = 0;
    while ((ent = readdir(d)))
        if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
            n++;
    rewinddir(d);
    KmArray *out = km_array_new(n);
    int64_t i = 0;
    while (i < n && (ent = readdir(d)))
        if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
            out->data[i++] = KM_WORD(km_strndup(ent->d_name, strlen(ent->d_name)));
    out->len = i;
    closedir(d);
    return out;
}

/* Recursively search the directory tree containing `from_path` for
 * `{name}.kmm` (then `{name}.km`); return its path, or 0. */
static const char *find_rec(const char *dir, const char *name)
{
    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *ent;
    char path[4096];
    const char *found = NULL;
    /* first: a direct hit in this directory */
    const char *exts[2] = { ".kmm", ".km" };
    for (int e = 0; e < 2 && !found; e++) {
        snprintf(path, sizeof path, "%s/%s%s", dir, name, exts[e]);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            found = km_strndup(path, strlen(path));
    }
    /* then: recurse into subdirectories */
    rewinddir(d);
    while (!found && (ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            found = find_rec(path, name);
    }
    closedir(d);
    return found;
}

/* file.find(name, from_path) : string?  (0 when nothing matches) */
const char *find(int64_t argc, KmArg *items)
{
    if (argc < 2) return NULL;
    const char *name = KM_STR(items[0].value);
    const char *from = KM_STR(items[1].value);
    /* directory of `from` */
    char dir[4096];
    strncpy(dir, from, sizeof dir - 1);
    dir[sizeof dir - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    else strcpy(dir, ".");
    return find_rec(dir, name);
}
