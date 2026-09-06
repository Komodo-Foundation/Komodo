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

/* file.exists(path) : bool */
long exists(long argc, KmArg *items)
{
    if (argc < 1) return 0;
    struct stat st;
    const char *path = (const char *)items[0].value;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* file.read_all(path) : string */
const char *read_all(long argc, KmArg *items)
{
    if (argc < 1) return "";
    const char *path = (const char *)items[0].value;
    FILE *f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

/* file.write_all(path, data) */
void write_all(long argc, KmArg *items)
{
    if (argc < 2) return;
    const char *path = (const char *)items[0].value;
    const char *data = (const char *)items[1].value;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fputs(data, f);
    fclose(f);
}

/* file.append_all(path, data)  — create if absent, otherwise append. */
void append_all(long argc, KmArg *items)
{
    if (argc < 2) return;
    const char *path = (const char *)items[0].value;
    const char *data = (const char *)items[1].value;
    FILE *f = fopen(path, "ab");
    if (!f) return;
    fputs(data, f);
    fclose(f);
}

/* file.read_lines(path) : [string]
 * The file split on '\n'; a trailing newline does not yield a final
 * empty element.  '\r' before a '\n' is dropped.  Empty array on error. */
KmArray *read_lines(long argc, KmArg *items)
{
    if (argc < 1) return km_array_new(0);
    const char *path = (const char *)items[0].value;
    FILE *f = fopen(path, "rb");
    if (!f) return km_array_new(0);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);

    long lines = 1;
    for (size_t i = 0; i < got; i++)
        if (buf[i] == '\n') lines++;

    KmArray *out = km_array_new(lines);
    long li = 0;
    size_t start = 0;
    for (size_t i = 0; i <= got; i++) {
        if (i == got || buf[i] == '\n') {
            size_t end = i;
            if (end > start && buf[end - 1] == '\r') end--;
            if (i == got && start == got) break;   /* trailing newline */
            char *line = malloc(end - start + 1);
            memcpy(line, buf + start, end - start);
            line[end - start] = '\0';
            out->data[li++] = (long)line;
            start = i + 1;
        }
    }
    out->len = li;
    free(buf);
    return out;
}

/* file.list_dir(path) : [string]  — entry names in `path` (no "."/"..",
 * unsorted); empty array when `path` is not a directory. */
KmArray *list_dir(long argc, KmArg *items)
{
    const char *path = argc >= 1 ? (const char *)items[0].value : ".";
    if (!path || !*path) path = ".";
    DIR *d = opendir(path);
    if (!d) return km_array_new(0);
    struct dirent *ent;
    long n = 0;
    while ((ent = readdir(d)))
        if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
            n++;
    rewinddir(d);
    KmArray *out = km_array_new(n);
    long i = 0;
    while (i < n && (ent = readdir(d)))
        if (strcmp(ent->d_name, ".") && strcmp(ent->d_name, ".."))
            out->data[i++] = (long)strdup(ent->d_name);
    out->len = i;
    closedir(d);
    return out;
}

/* Recursively search the directory tree containing `from_path` for
 * `{name}.kmm` (then `{name}.km`); return its path, or 0. */
static char *find_rec(const char *dir, const char *name)
{
    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *ent;
    char path[4096];
    char *found = NULL;
    /* first: a direct hit in this directory */
    const char *exts[2] = { ".kmm", ".km" };
    for (int e = 0; e < 2 && !found; e++) {
        snprintf(path, sizeof path, "%s/%s%s", dir, name, exts[e]);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
            found = strdup(path);
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
const char *find(long argc, KmArg *items)
{
    if (argc < 2) return NULL;
    const char *name = (const char *)items[0].value;
    const char *from = (const char *)items[1].value;
    /* directory of `from` */
    char dir[4096];
    strncpy(dir, from, sizeof dir - 1);
    dir[sizeof dir - 1] = '\0';
    char *slash = strrchr(dir, '/');
    if (slash) *slash = '\0';
    else strcpy(dir, ".");
    return find_rec(dir, name);
}
