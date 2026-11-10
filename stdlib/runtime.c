/*
 * runtime — primitives the compiler lowers operators to (not Komodo
 * calls), the collected heap every Komodo value lives on, and the
 * process entry.  Array literals and `new` call km_alloc directly;
 * anything that has to grow, copy or build a value goes through here.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "km_runtime.h"

/* ── the collected heap ─────────────────────────────────────────────
 * Every heap value a Komodo program makes — objects, arrays, strings —
 * comes from the Boehm-Demers-Weiser conservative collector (libgc,
 * linked as libgc.so.1).  Its roots are the stack, the registers and
 * the program's data / bss (where top-level `let`s live), so whatever is
 * no longer reachable from them is reclaimed automatically; nothing in
 * Komodo ever frees by hand.  Pointer-free data (string bytes) is
 * allocated "atomic": never scanned, never zeroed.
 *
 * Interior pointers are recognised (libgc's default), so `data(arr)`
 * handed to C keeps its array alive.  Anything C code keeps must stay
 * reachable from Komodo too: the collector does not look inside memory
 * that libc malloc or a C library owns.
 *
 * Prototypes are declared here, so only the shared library is needed,
 * not its development headers. */
#ifndef __EMSCRIPTEN__
extern void   GC_init(void);
extern void  *GC_malloc(size_t);
extern void  *GC_malloc_atomic(size_t);
extern void  *GC_malloc_ignore_off_page(size_t);
extern void  *GC_malloc_atomic_ignore_off_page(size_t);
extern void  *GC_base(void *);
extern void   GC_disable(void);
extern void   GC_set_max_heap_size(unsigned long);
extern void   GC_set_oom_fn(void *(*)(size_t));
extern void   GC_set_warn_proc(void (*)(char *, unsigned long));
extern void   GC_ignore_warn_proc(char *, unsigned long);
extern size_t GC_get_heap_size(void);
extern void   GC_set_min_bytes_allocd(size_t);
#endif

/* The least a program allocates between two collections.  libgc
 * otherwise collects after ~2/3 of the live data's size: a program that
 * churns through short-lived values around a small live set (Meld's
 * self-test allocates 17 GB over ~15 MB live) then collects every few MB
 * and spends half its time marking. */
#define KM_MIN_ALLOC_BETWEEN_GCS ((size_t)64 << 20)

/* Blocks this large are only ever referenced from their first page
 * (a Komodo handle points at the header), which lets the collector skip
 * the false-reference checks that make huge blocks slow to place. */
#define KM_BIG_BLOCK (64 * 1024)

#ifdef __EMSCRIPTEN__
static void km_out_of_memory(size_t want)
{
    (void)want;
    fflush(NULL);
    fputs("Komodo: out of memory\n", stderr);
    _exit(3);
}
#else
/* 0 = no ceiling (KOMODO_MAX_HEAP=0) */
static size_t km_heap_limit;

/* Out of memory: the heap ceiling was reached, or the system refused
 * more.  Never returns — generated code is never handed NULL. */
static void km_out_of_memory(size_t want)
{
    char where[160] = "";
    km_komodo_caller(where, sizeof where);
    size_t heap = GC_get_heap_size();
    char lim[32];
    fflush(NULL);
    if (km_heap_limit && heap + want + (1 << 20) >= km_heap_limit / 100 * 95) {
        km_fmt_bytes(lim, sizeof lim, km_heap_limit);
        fprintf(stderr, "Komodo: out of memory - heap limit %s reached%s%s"
                " (raise it with KOMODO_MAX_HEAP)\n",
                lim, *where ? " in " : "", where);
    } else {
        km_fmt_bytes(lim, sizeof lim, heap);
        fprintf(stderr, "Komodo: out of memory - the system has no memory left"
                " (heap %s)%s%s\n", lim, *where ? " in " : "", where);
    }
    _exit(3);
}

static void *km_oom_fn(size_t want)
{
    km_out_of_memory(want);
    return NULL;
}
#endif

#ifdef __EMSCRIPTEN__
/* WebAssembly has no collector: allocation is plain (dlmalloc) memory that
 * is never freed.  calloc zeroes it, as km_alloc promises. */
void *km_alloc(size_t n)
{
    void *p = calloc(n ? n : 1, 1);
    if (!p)
        km_out_of_memory(n);
    return p;
}

void *km_alloc_atomic(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p)
        km_out_of_memory(n);
    return p;
}
#else
/* Pointer-carrying memory (objects, arrays): zeroed and scanned. */
void *km_alloc(size_t n)
{
    void *p = n >= KM_BIG_BLOCK ? GC_malloc_ignore_off_page(n) : GC_malloc(n ? n : 1);
    if (!p)
        km_out_of_memory(n);
    return p;
}

/* Pointer-free bytes (string text): neither zeroed nor scanned. */
void *km_alloc_atomic(size_t n)
{
    void *p = n >= KM_BIG_BLOCK ? GC_malloc_atomic_ignore_off_page(n) : GC_malloc_atomic(n ? n : 1);
    if (!p)
        km_out_of_memory(n);
    return p;
}
#endif

/* A fresh NUL-terminated copy of the first `n` bytes of `s`. */
char *km_strndup(const char *s, size_t n)
{
    char *out = km_alloc_atomic(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* "10.4 GB" / "512 MB" (powers of 1024, as `free -h` shows them) */
void km_fmt_bytes(char *buf, size_t cap, size_t n)
{
    double gb = (double)n / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0)
        snprintf(buf, cap, "%.1f GB", gb);
    else
        snprintf(buf, cap, "%.0f MB", (double)n / (1024.0 * 1024.0));
}

#ifndef __EMSCRIPTEN__
/* Read a small text file into buf; 0 on failure. */
static int km_read_text(const char *path, char *buf, size_t cap)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    ssize_t n = read(fd, buf, cap - 1);
    close(fd);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    return 1;
}

/* The tightest cgroup (v2) memory.max on the way from this process's
 * cgroup up to the root — a container or `systemd-run -p MemoryMax=`
 * limit — or 0 when there is none. */
static size_t km_cgroup_limit(void)
{
    char cg[1024], path[1200], val[64];
    if (!km_read_text("/proc/self/cgroup", cg, sizeof cg))
        return 0;
    char *rel = strstr(cg, "0::");
    if (!rel)
        return 0;
    rel += 3;
    char *nl = strchr(rel, '\n');
    if (nl)
        *nl = '\0';
    size_t best = 0;
    for (;;) {
        snprintf(path, sizeof path, "/sys/fs/cgroup%s/memory.max", rel);
        if (km_read_text(path, val, sizeof val) && val[0] >= '0' && val[0] <= '9') {
            size_t v = strtoull(val, NULL, 10);
            if (v && (!best || v < best))
                best = v;
        }
        char *slash = strrchr(rel, '/');
        if (!slash || slash == rel)
            break;
        *slash = '\0';
    }
    return best;
}

/* KOMODO_MAX_HEAP: bytes, or a number with a K / M / G / T suffix
 * (powers of 1024; "1.5G" works).  0 = no ceiling.  -1 if malformed. */
static long double km_parse_size(const char *s)
{
    char *end;
    long double v = strtold(s, &end);
    if (end == s || v < 0)
        return -1;
    switch (*end) {
    case 'k': case 'K': v *= 1024.0L; end++; break;
    case 'm': case 'M': v *= 1024.0L * 1024; end++; break;
    case 'g': case 'G': v *= 1024.0L * 1024 * 1024; end++; break;
    case 't': case 'T': v *= 1024.0L * 1024 * 1024 * 1024; end++; break;
    }
    if (*end == 'i' || *end == 'I')
        end++;
    if (*end == 'b' || *end == 'B')
        end++;
    return *end ? -1 : v;
}

/* The heap ceiling: KOMODO_MAX_HEAP when set, else 70 % of the memory
 * this process may use — physical RAM, or a lower cgroup limit.  Running
 * into it ends the program with a clear message instead of letting it
 * swallow the machine. */
static size_t km_pick_heap_limit(void)
{
    const char *env = getenv("KOMODO_MAX_HEAP");
    if (env && *env) {
        long double v = km_parse_size(env);
        if (v >= 0)
            return (size_t)v;
        fprintf(stderr, "Komodo: ignoring KOMODO_MAX_HEAP=%s (want bytes or e.g. 2G)\n", env);
    }
    int64_t pages = sysconf(_SC_PHYS_PAGES), psize = sysconf(_SC_PAGE_SIZE);
    size_t avail = pages > 0 && psize > 0 ? (size_t)pages * (size_t)psize : 0;
    size_t cg = km_cgroup_limit();
    if (cg && (!avail || cg < avail))
        avail = cg;
    return avail / 10 * 7;
}

/* Programs compiled by a pre-collector kmc allocate with raw malloc,
 * where the collector cannot see their pointers.  Such code lacks this
 * marker (the backend emits it) and runs with collection switched off —
 * the old never-free behaviour — which keeps bootstrapping from an
 * older compiler working. */
extern const int64_t km_abi_gc __attribute__((weak));

static void km_heap_init(void)
{
    GC_init();
    GC_set_warn_proc(GC_ignore_warn_proc);
    GC_set_oom_fn(km_oom_fn);
    km_heap_limit = km_pick_heap_limit();
    size_t gap = KM_MIN_ALLOC_BETWEEN_GCS;
    if (km_heap_limit) {
        GC_set_max_heap_size(km_heap_limit);
        if (gap > km_heap_limit / 8)
            gap = km_heap_limit / 8;
    }
    if (gap)
        GC_set_min_bytes_allocd(gap);
    if (&km_abi_gc == NULL)
        GC_disable();
}
#else
static void km_heap_init(void) {}
#endif

/* Render a double the Komodo way: like printf's "%g", but an integral
 * value keeps a ".0" so it still reads as a float ("4.0", not "4").
 * Non-finite ("inf"/"nan") and exponent forms are left as %g produced. */
void km_fmt_double(char *buf, unsigned long cap, double d)
{
    snprintf(buf, cap, "%g", d);
    for (const char *p = buf; *p; p++)
        if (*p == '.' || *p == 'e' || *p == 'E' || *p == 'n' || *p == 'i')
            return;
    unsigned long len = strlen(buf);
    if (len + 3 <= cap) {
        buf[len] = '.';
        buf[len + 1] = '0';
        buf[len + 2] = '\0';
    }
}

/* `needle in haystack` for strings — a substring test. */
int64_t km_in_str(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return 0;
    return strstr(haystack, needle) != NULL;
}

/* `v in arr` — linear membership.  `leaf` is the element's scalar tag:
 * 2 (string) compares with strcmp, anything else a raw 64-bit `==`
 * (ints, bools, chars, and float bit patterns for exact matches). */
int64_t km_in_array(KmArray *a, int64_t v, int64_t leaf)
{
    if (!a)
        return 0;
    if (leaf == 2) {
        const char *needle = KM_STR(v);
        for (int64_t i = 0; i < a->len; i++) {
            const char *e = KM_STR(a->data[i]);
            if (e == needle || (e && needle && strcmp(e, needle) == 0))
                return 1;
        }
        return 0;
    }
    for (int64_t i = 0; i < a->len; i++)
        if (a->data[i] == v)
            return 1;
    return 0;
}

KmArray *km_array_new(int64_t len)
{
    KmArray *a = km_alloc(sizeof(KmArray) + (size_t)len * sizeof(int64_t));
    a->len = len;
    a->cap = len;
    return a;
}

/* Explicit frees from code built by a pre-collector kmc (its escape
 * analysis released some locals by hand).  The collector owns every
 * lifetime now, so these do nothing; current code never calls them. */
void km_retain(void *handle)     { (void)handle; }
void km_release(void *handle)    { (void)handle; }
void km_drop_array(void *handle) { (void)handle; }

/* `a + b` — returns a fresh array holding a's elements then b's; both
 * operands are left untouched (used where the left side may be aliased). */
KmArray *km_array_append(KmArray *dst, KmArray *src)
{
    KmArray *out = km_array_new(dst->len + src->len);
    memcpy(out->data, dst->data, (size_t)dst->len * sizeof(int64_t));
    memcpy(out->data + dst->len, src->data, (size_t)src->len * sizeof(int64_t));
    return out;
}

/* `dst += src` — grow `dst` in place with amortised doubling and return
 * it (the pointer moves when the capacity runs out).  The compound-
 * assignment lowerings overwrite the old `dst` slot with the result, so
 * in-place reuse is safe there and turns an O(n^2) build into O(n).  A
 * moved array's old block is left to the collector, never freed here,
 * so another alias of it stays valid (it just stops seeing appends). */
KmArray *km_array_grow(KmArray *dst, KmArray *src)
{
    int64_t need = dst->len + src->len;
    if (need > dst->cap) {
        int64_t ncap = dst->cap ? dst->cap * 2 : 4;
        if (ncap < need)
            ncap = need;
        KmArray *grown = km_alloc(sizeof(KmArray) + (size_t)ncap * sizeof(int64_t));
        memcpy(grown, dst, sizeof(KmArray) + (size_t)dst->len * sizeof(int64_t));
        dst = grown;
        dst->cap = ncap;
    }
    memcpy(dst->data + dst->len, src->data, (size_t)src->len * sizeof(int64_t));
    dst->len = need;
    return dst;
}

/* `dst += [v]` — append one element in place, growing like km_array_grow
 * but with no one-element temporary array.  Returns the (maybe moved)
 * handle, which the caller stores back. */
KmArray *km_array_push(KmArray *dst, int64_t v)
{
    if (dst->len >= dst->cap) {
        int64_t ncap = dst->cap ? dst->cap * 2 : 4;
        KmArray *grown = km_alloc(sizeof(KmArray) + (size_t)ncap * sizeof(int64_t));
        memcpy(grown, dst, sizeof(KmArray) + (size_t)dst->len * sizeof(int64_t));
        dst = grown;
        dst->cap = ncap;
    }
    dst->data[dst->len] = v;
    dst->len += 1;
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
    for (int64_t i = 0; i < parts->len; i++) {
        const char *p = KM_STR(parts->data[i]);
        total += p ? strlen(p) : 0;
        if (i + 1 < parts->len)
            total += slen;
    }
    char *out = km_alloc_atomic(total + 1);
    char *w = out;
    for (int64_t i = 0; i < parts->len; i++) {
        const char *p = KM_STR(parts->data[i]);
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
KmArray *km_array_slice(KmArray *a, int64_t start, int64_t end)
{
    int64_t n = a->len;
    if (end < 0 || end > n)
        end = n;
    if (start < 0)
        start = 0;
    if (start > end)
        start = end;
    int64_t len = end - start;
    KmArray *out = km_array_new(len);
    memcpy(out->data, a->data + start, (size_t)len * sizeof(int64_t));
    return out;
}

/* `a * n` — a fresh array holding `a`'s elements repeated `n` times
 * (n <= 0 yields an empty array). */
KmArray *km_array_repeat(KmArray *a, int64_t n)
{
    if (n < 0)
        n = 0;
    KmArray *out = km_array_new(a->len * n);
    for (int64_t i = 0; i < n; i++)
        memcpy(out->data + i * a->len, a->data, (size_t)a->len * sizeof(int64_t));
    return out;
}

/* ── string operators ── */

/* `s[start to end]` — a fresh NUL-terminated substring, same clamping. */
const char *km_str_slice(const char *s, int64_t start, int64_t end)
{
    if (!s) s = "";
    /* Only measure as far as `end`: a short slice of a int64_t string (a
     * lexer cutting tokens out of its source) must not cost a full
     * strlen each time. */
    int64_t n = end < 0 ? (int64_t)strlen(s) : (int64_t)strnlen(s, (size_t)end);
    if (end < 0 || end > n)
        end = n;
    if (start < 0)
        start = 0;
    if (start > end)
        start = end;
    return km_strndup(s + start, (size_t)(end - start));
}

/* `a + b` */
const char *km_str_concat(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *out = km_alloc_atomic(la + lb + 1);
    memcpy(out, a, la);
    memcpy(out + la, b, lb + 1);
    return out;
}

/* `s * n` */
const char *km_str_repeat(const char *s, int64_t n)
{
    if (n < 0)
        n = 0;
    size_t len = strlen(s);
    char *out = km_alloc_atomic(len * (size_t)n + 1);
    char *p = out;
    for (int64_t i = 0; i < n; i++, p += len)
        memcpy(p, s, len);
    *p = '\0';
    return out;
}

/* `#s` */
int64_t km_str_len(const char *s)
{
    return s ? (int64_t)strlen(s) : 0;
}

/* `a == b` */
int64_t km_str_eq(const char *a, const char *b)
{
    if (a == b) return 1;           /* same pointer (interned literal / char) */
    if (!a || !b) return 0;
    if (*a != *b) return 0;         /* most mismatches differ at byte 0 */
    return strcmp(a, b) == 0;
}

/* Pretty-print an array for `print`/`string(x)` — see km_runtime.h for
 * how `leaf`/`depth` were packed into the caller's tag.  `depth > 1`
 * means each element is itself a KmArray*, printed the same way one
 * level shallower; at `depth == 1` elements are read as `leaf`. */
const char *km_array_to_string(KmArray *a, int64_t leaf, int64_t depth)
{
    if (!a)
        return "[]";
    size_t size = 0;
    char *buf = NULL;
    FILE *m = open_memstream(&buf, &size);
    fputc('[', m);
    for (int64_t i = 0; i < a->len; i++) {
        if (i)
            fputs(", ", m);
        int64_t v = a->data[i];
        if (depth > 1) {
            fputs(km_array_to_string(KM_ARR(v), leaf, depth - 1), m);
        } else if (leaf == KM_TAG_FLOAT) {
            char fb[64];
            km_fmt_double(fb, sizeof fb, km_as_float(v));
            fputs(fb, m);
        } else if (leaf == KM_TAG_STR) {
            fputc('"', m);
            const char *s = v ? KM_STR(v) : "";
            for (const char *p = s; *p; p++) {
                if (*p == '"' || *p == '\\')
                    fputc('\\', m);
                fputc(*p, m);
            }
            fputc('"', m);
        } else if (leaf == KM_TAG_BOOL) {
            fputs(v ? "true" : "false", m);
        } else if (leaf == KM_TAG_INT) {
            fprintf(m, "%" PRId64 "", v);
        } else {
            fputs("<?>", m);
        }
    }
    fputc(']', m);
    fclose(m);
    /* the memstream buffer is libc's: hand back a collected copy */
    char *out = km_strndup(buf, size);
    free(buf);
    return out;
}

/* `string(x)` — tag is a compile-time constant (0 int / 1 float-bits /
 * 2 string / 3 bool / packed array, see km_runtime.h); value is the raw
 * payload word (an array's KmArray* for the array case). */
const char *km_to_string(int64_t tag, int64_t value)
{
    if ((tag & 0xFF) == KM_TAG_ARRAY)
        return km_array_to_string(KM_ARR(value), (tag >> 8) & 0xFF, (tag >> 16) & 0xFF);
    char buf[64];
    switch (tag) {
    case 1: {
        double d;
        __builtin_memcpy(&d, &value, sizeof d);
        km_fmt_double(buf, sizeof buf, d);
        break;
    }
    case 2:
        return value ? KM_STR(value) : "null";
    case 3:
        return value ? "true" : "false";
    default:
        snprintf(buf, sizeof buf, "%" PRId64 "", value);
        break;
    }
    return km_strndup(buf, strlen(buf));
}

/* ── process entry ──────────────────────────────────────────────────
 * The compiler names a program's entry point `km_program_main` and
 * expects its (optional) `args` parameter as a Komodo string array
 * passed through the uniform call ABI.  This shim marshals the C argv
 * into that array and invokes it. */
int64_t km_program_main(int64_t argc, KmArg *items);

/* The process's argv, recorded by main() for the `os` module. */
int km_main_argc;
char **km_main_argv;

#ifdef __EMSCRIPTEN__
/* emcc 3.1.6 + clang 15 only recognises a prototype-less `main` (a main
 * taking argc / argv is mangled to __main_argc_argv and then never run),
 * so the node command line is fetched from JS: argv[0] is the script. */
#include <emscripten.h>
EM_JS(int, km_js_argc, (void), { return process.argv.length - 1; });
EM_JS(char *, km_js_argv, (int i), {
    var s = process.argv[i + 1];
    var n = lengthBytesUTF8(s) + 1;
    var p = _malloc(n);
    stringToUTF8(s, p, n);
    return p;
});

int main()
{
    int argc = km_js_argc();
    char **argv = malloc((size_t)(argc + 1) * sizeof *argv);
    for (int i = 0; i < argc; i++)
        argv[i] = km_js_argv(i);
    argv[argc] = NULL;
#else
int main(int argc, char **argv)
{
#endif
    km_heap_init();
    km_crash_init();

    /* Resolve argv[0] to the executable's real path so a program can
     * locate files that ship next to it (e.g. the compiler's stdlib/)
     * regardless of how it was invoked ("./kmc", bare "kmc" via PATH…). */
#ifndef __EMSCRIPTEN__
    static char exe[4096];
    ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        exe[n] = '\0';
        if (argc > 0)
            argv[0] = exe;
    }
#endif

    /* Kept for the `os` module (os.args / os.exe); argv outlives main. */
    km_main_argc = argc;
    km_main_argv = argv;

    KmArray *a = km_array_new(argc);
    for (int i = 0; i < argc; i++)
        a->data[i] = KM_WORD(argv[i]);
    KmArg arg = { KM_TAG_INT, KM_WORD(a) };
    km_program_main(1, &arg);
    return 0;
}

/* `s[i]` — a one-character string (a Komodo `char`).  Strings are
 * immutable, so every result is drawn from one static table of the 256
 * possible single-byte strings instead of a fresh 2-byte allocation per
 * index (a lexer indexes every byte of its input).  Byte 0 maps to "". */
static char km_char_strs[256][2];
static int km_char_strs_ready;

const char *km_str_char(const char *s, int64_t i)
{
    if (!s) return "";
    if (!km_char_strs_ready) {
        for (int c = 0; c < 256; c++)
            km_char_strs[c][0] = (char)c;
        km_char_strs_ready = 1;
    }
    return km_char_strs[(unsigned char)s[i]];
}

/* `s1 <op> s2` ordering — like C strcmp. */
int64_t km_str_cmp(const char *a, const char *b)
{
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b);
}

/* FNV-1a over the bytes of `s`, masked non-negative — the hash behind
 * the compiler's name indexes (modules/support/strindex.kmm). */
int64_t km_str_hash(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    if (s)
        for (; *s; s++)
            h = (h ^ (unsigned char)*s) * 1099511628211ULL;
    return (int64_t)(h & 0x7fffffffffffffffULL);
}

/* Index of the first occurrence of `sub` in `s`, or -1 — the compiler's
 * x86 peephole pass searches its emitted assembly lines with it. */
int64_t km_str_find(const char *s, const char *sub)
{
    const char *p = strstr(s, sub);
    return p ? (int64_t)(p - s) : -1;
}

/* s starts / ends with p — no slice, no allocation (the x86 peephole asks
 * this of every asm line many times over). */
int64_t km_str_starts(const char *s, const char *p)
{
    while (*p) if (*s++ != *p++) return 0;
    return 1;
}
/* Line scanning for the driver (main.km `gather`): index of the next '\n' at
 * or after `start` (strlen when none), and whether the line starting at
 * `start` opens (after blanks) with `kw`. */
int64_t km_str_nl(const char *s, int64_t start)
{
    const char *p = strchr(s + start, '\n');
    return p ? (int64_t)(p - s) : (int64_t)(start + strlen(s + start));
}
int64_t km_str_kw(const char *s, int64_t start, const char *kw)
{
    s += start;
    while (*s == ' ' || *s == '\t') s++;
    return km_str_starts(s, kw);
}

/* x86 peephole: may this asm line sit inside a spill span?  (straight-line
 * code only: no label, jump, call, push/pop, stack-pointer use or %-directive).
 * One pass over the line instead of eight substring scans. */
int64_t km_asm_spanbad(const char *l)
{
    size_t n = strlen(l);
    if (n == 0 || l[0] == '%' || l[n - 1] == ':') return 1;
    if (n >= 5 && l[0] == ' ' && l[1] == ' ' && l[2] == ' ' && l[3] == ' ' && l[4] == 'j') return 1;
    return strstr(l, "rsp") || strstr(l, "call") || strstr(l, "push") || strstr(l, "pop");
}

/* Character classes for the lexer: `c` is a one-char string (or NULL). */
int64_t km_ch_alpha(const char *c)
{
    unsigned char x = c ? (unsigned char)*c : 0;
    return (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || x == '_';
}
int64_t km_ch_digit(const char *c)
{
    return c && *c >= '0' && *c <= '9';
}

/* x86 peephole: can any rewrite start at this line?  (the patterns open with
 * mov [, movsd [, push rax, cmp , ucomisd , sub rsp, 16) */
int64_t km_asm_peepstart(const char *l)
{
    if (strncmp(l, "    ", 4) != 0) return 0;
    l += 4;
    switch (*l) {
    case 'm': return strncmp(l, "mov [", 5) == 0 || strncmp(l, "movsd [", 7) == 0;
    case 'p': return strcmp(l, "push rax") == 0;
    case 'c': return strncmp(l, "cmp ", 4) == 0;
    case 'u': return strncmp(l, "ucomisd ", 8) == 0;
    case 's': return strcmp(l, "sub rsp, 16") == 0;
    }
    return 0;
}

int64_t km_str_ends(const char *s, const char *p)
{
    size_t n = strlen(s), m = strlen(p);
    return n >= m && memcmp(s + n - m, p, m) == 0;
}

/* `s[start to end]` for a caller that already knows `n == #s` — the same
 * clamping as km_str_slice, without re-measuring `s`.  The lexer cuts
 * every token out of a source of hundreds of KB; measuring up to `end`
 * each time made that quadratic. */
const char *km_str_sub(const char *s, int64_t n, int64_t start, int64_t end)
{
    if (!s) { s = ""; n = 0; }
    if (end < 0 || end > n)
        end = n;
    if (start < 0)
        start = 0;
    if (start > end)
        start = end;
    return km_strndup(s + start, (size_t)(end - start));
}
