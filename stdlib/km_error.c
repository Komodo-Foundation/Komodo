/*
 * km_error — runtime support for Komodo error handling.
 *
 *   throw <msg>      lowers to a call to km_throw(msg)
 *   f(args)?         wraps the call site in _setjmp against a jmp_buf
 *                    reserved on the caller's stack, registered here
 *
 * km_throw _longjmps to the innermost registered landing pad (removing
 * it from the stack as it goes).  With no pad registered a thrown error
 * is fatal: the message is printed and the process exits non-zero.
 *
 * Single-threaded — the pad stack is a plain global, matching the rest
 * of the Komodo runtime.
 *
 * Also here: the runtime errors the hardware reports.  An out-of-range
 * `arr[i]` throws like any Komodo error; a crash signal (bad memory
 * access, stack overflow, division by zero, ...) prints one readable
 * line plus the Komodo call stack and exits with 128 + the signal number
 * instead of dumping core.
 */
#define _GNU_SOURCE
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef __EMSCRIPTEN__
#include <ucontext.h>
#include <elf.h>
#include <dlfcn.h>
#include <unwind.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif
#include "km_runtime.h"

/* Present only in programs built with KM_DEBUG=1 (x86 backend): the
 * extent of the generated code, and a line table mapping code offsets to
 * Komodo file:line.  Weak, so every other program links without them. */
extern const char km_text_begin[] __attribute__((weak));
extern const char km_text_end[] __attribute__((weak));
extern const uint32_t km_dbg_lines[] __attribute__((weak));  /* n, then n x { offset, line, file } */
extern const char km_dbg_files[] __attribute__((weak));      /* NUL-separated file names */

/* The registered landing pads, innermost last.  Each points at a
 * jmp_buf on its caller's stack (no heap data, so plain libc memory
 * holds them); the array grows with the nesting depth of `f(...)?`
 * calls, so a deep recursion through them never loses a pad. */
static struct __jmp_buf_tag **km_pads;
static int64_t        km_pad_n = 0, km_pad_cap = 0;
static const char *km_err_msg = NULL;

void km_try_push(struct __jmp_buf_tag *b)
{
    if (km_pad_n == km_pad_cap) {
        int64_t cap = km_pad_cap ? km_pad_cap * 2 : 64;
        struct __jmp_buf_tag **p = realloc(km_pads, (size_t)cap * sizeof *p);
        if (!p) {
            fputs("Komodo: out of memory - protected calls nested too deeply\n", stderr);
            _exit(3);
        }
        km_pads = p;
        km_pad_cap = cap;
    }
    km_pads[km_pad_n++] = b;
}

void km_try_pop(void)
{
    if (km_pad_n > 0)
        km_pad_n--;
}

/* Compiler-side switch: is KM_DEBUG set?  (x86 backend: emit %line info.) */
long km_debug_info(void)
{
    const char *d = getenv("KM_DEBUG");
    return d && *d && *d != '0';
}

const char *km_error_message(void)
{
    return km_err_msg;
}

static void km_uncaught_trace(void);

void km_throw(const char *msg)
{
    km_err_msg = msg;
    if (km_pad_n > 0)
        _longjmp(km_pads[--km_pad_n], 1);
    fflush(stdout);                  /* the program's output first, then the error */
    fprintf(stderr, "Uncaught error: %s\n", msg ? msg : "");
    fflush(stderr);
    km_uncaught_trace();
    exit(1);
}

/* Print `msg` on stderr and exit(1) — a tool's fatal-error path, with no
 * "Uncaught error:" prefix (used by the compiler driver). */
void km_fail(const char *msg)
{
    fprintf(stderr, "%s\n", msg ? msg : "");
    exit(1);
}

#ifndef __EMSCRIPTEN__   /* ELF symbol lookup, stack walking, crash handlers: native only */

/* ── symbols: naming code addresses ────────────────────────────────
 * The executable's own ELF symbol table (/proc/self/exe, mapped on first
 * use) names every function: generated Komodo code carries NASM labels
 * (untyped, STT_NOTYPE), the C runtime and libraries real STT_FUNC
 * symbols.  NASM's local `.L` jump labels ("fn.L12") are skipped.  A
 * sorted index of start addresses makes each lookup a binary search, so
 * even a 100 000-frame recursion is walked quickly. */
typedef struct { uintptr_t addr; uint32_t sym; } KmSymIdx;

static const Elf64_Shdr *km_shdr;
static int               km_shnum;
static const Elf64_Sym  *km_sym;
static size_t            km_nsym;
static const char       *km_symstr;
static size_t            km_symstr_len;
static uintptr_t         km_bias;
static KmSymIdx         *km_idx;      /* sorted by addr; NULL = scan linearly */
static size_t            km_nidx;
static int               km_sym_state; /* 0 not loaded, 1 ready, -1 none */

static int km_sym_usable(const Elf64_Sym *s)
{
    int t = ELF64_ST_TYPE(s->st_info);
    if (t != STT_FUNC && t != STT_NOTYPE)
        return 0;
    if (s->st_shndx == SHN_UNDEF || s->st_shndx >= km_shnum)
        return 0;
    if (!(km_shdr[s->st_shndx].sh_flags & SHF_EXECINSTR))
        return 0;
    if (s->st_name == 0 || s->st_name >= km_symstr_len)
        return 0;
    return strchr(km_symstr + s->st_name, '.') == NULL;
}

static void km_idx_sift(KmSymIdx *a, size_t root, size_t n)
{
    for (;;) {
        size_t c = 2 * root + 1;
        if (c >= n)
            return;
        if (c + 1 < n && a[c + 1].addr > a[c].addr)
            c++;
        if (a[root].addr >= a[c].addr)
            return;
        KmSymIdx t = a[root]; a[root] = a[c]; a[c] = t;
        root = c;
    }
}

static void km_load_symbols(void)
{
    if (km_sym_state)
        return;
    km_sym_state = -1;
    int fd = open("/proc/self/exe", O_RDONLY);
    if (fd < 0)
        return;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(Elf64_Ehdr)) {
        close(fd);
        return;
    }
    const char *img = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (img == MAP_FAILED)
        return;
    size_t size = (size_t)st.st_size;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)img;
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 || eh->e_ident[EI_CLASS] != ELFCLASS64
        || eh->e_shoff == 0 || eh->e_shoff + (size_t)eh->e_shnum * sizeof(Elf64_Shdr) > size)
        return;
    const Elf64_Shdr *sh = (const Elf64_Shdr *)(img + eh->e_shoff);
    for (int i = 0; i < eh->e_shnum; i++) {
        if (sh[i].sh_type != SHT_SYMTAB || sh[i].sh_link >= eh->e_shnum)
            continue;
        const Elf64_Shdr *str = &sh[sh[i].sh_link];
        if (sh[i].sh_offset + sh[i].sh_size > size || str->sh_offset + str->sh_size > size)
            continue;
        km_sym = (const Elf64_Sym *)(img + sh[i].sh_offset);
        km_nsym = sh[i].sh_size / sizeof(Elf64_Sym);
        km_symstr = img + str->sh_offset;
        km_symstr_len = str->sh_size;
    }
    if (!km_sym)
        return;                       /* stripped */
    km_shdr = sh;
    km_shnum = eh->e_shnum;
    if (eh->e_type == ET_DYN) {       /* PIE: symbols are load-relative */
        Dl_info di;
        if (dladdr((void *)km_load_symbols, &di))
            km_bias = (uintptr_t)di.dli_fbase;
    }
    km_sym_state = 1;

    /* the sorted index (skipped if memory is short: lookups then scan) */
    size_t n = 0;
    for (size_t i = 0; i < km_nsym; i++)
        n += km_sym_usable(&km_sym[i]);
    void *mem = n ? mmap(NULL, n * sizeof(KmSymIdx), PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) : MAP_FAILED;
    if (mem == MAP_FAILED)
        return;
    KmSymIdx *a = mem;
    size_t k = 0;
    for (size_t i = 0; i < km_nsym; i++)
        if (km_sym_usable(&km_sym[i])) {
            a[k].addr = km_sym[i].st_value;
            a[k].sym = (uint32_t)i;
            k++;
        }
    for (size_t r = n / 2; r-- > 0; )        /* heapsort: no malloc here */
        km_idx_sift(a, r, n);
    for (size_t end = n; end-- > 1; ) {
        KmSymIdx t = a[0]; a[0] = a[end]; a[end] = t;
        km_idx_sift(a, 0, end);
    }
    km_idx = a;
    km_nidx = n;
}

/* The executable's symbol covering `pc` (an address inside the code, not
 * a return address), or NULL.  *komodo = 1 for generated Komodo code. */
static const char *km_exe_symbol(uintptr_t pc, int *komodo)
{
    *komodo = 0;
    km_load_symbols();
    if (km_sym_state != 1)
        return NULL;
    uintptr_t a = pc - km_bias;
    const Elf64_Sym *best = NULL;
    if (km_idx) {
        size_t lo = 0, hi = km_nidx;           /* last entry with addr <= a */
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (km_idx[mid].addr <= a)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo == 0)
            return NULL;
        best = &km_sym[km_idx[lo - 1].sym];
    } else {
        for (size_t i = 0; i < km_nsym; i++) {
            const Elf64_Sym *s = &km_sym[i];
            if (s->st_value <= a && (!best || s->st_value > best->st_value) && km_sym_usable(s))
                best = s;
        }
        if (!best)
            return NULL;
    }
    const Elf64_Shdr *sec = &km_shdr[best->st_shndx];
    if (a >= sec->sh_addr + sec->sh_size)
        return NULL;
    int t = ELF64_ST_TYPE(best->st_info);
    if (t == STT_FUNC && best->st_size && a >= best->st_value + best->st_size)
        return NULL;                  /* in a gap, e.g. between C objects */
    /* NASM labels are untyped; a KM_DEBUG build types them (so a debugger
     * can size them) and is recognised by its code extent instead */
    *komodo = t == STT_NOTYPE
        || (km_text_begin && pc >= (uintptr_t)km_text_begin && pc < (uintptr_t)km_text_end);
    return km_symstr + best->st_name;
}

/* A symbol as Komodo source spells it: `Class__method` → `Class::method`,
 * the entry `km_program_main` → `main`, overload / generic suffixes
 * (`__ov2_int_int`, `__g1_float`) dropped. */
static void km_pretty(const char *sym, char *out, size_t cap)
{
    if (*sym == '$')
        sym++;
    if (strcmp(sym, "km_program_main") == 0) {
        snprintf(out, cap, "main");
        return;
    }
    if (strncmp(sym, "km_deepcopy_", 12) == 0) {
        snprintf(out, cap, "copy(%s)", sym + 12);
        return;
    }
    if (strncmp(sym, "km_tramp_", 9) == 0)
        sym += 9;
    size_t o = 0;
    for (const char *p = sym; *p && o + 3 < cap; p++) {
        if (p[0] == '_' && p[1] == '_') {
            if ((p[2] == 'o' && p[3] == 'v' && p[4] >= '0' && p[4] <= '9')
                || (p[2] == 'g' && p[3] >= '0' && p[3] <= '9'))
                break;
            out[o++] = ':';
            out[o++] = ':';
            p++;
            continue;
        }
        out[o++] = *p;
    }
    out[o] = '\0';
}

/* "file.km:12" for the Komodo code at `pc` (inside a call instruction),
 * when the program was built with KM_DEBUG; else "". */
static void km_where(uintptr_t pc, char *out, size_t cap)
{
    out[0] = '\0';
    if (!km_dbg_lines || !km_text_begin || pc < (uintptr_t)km_text_begin || pc >= (uintptr_t)km_text_end)
        return;
    uint32_t off = (uint32_t)(pc - (uintptr_t)km_text_begin);
    uint32_t n = km_dbg_lines[0], lo = 0, hi = n;     /* last row with offset <= off */
    const uint32_t *row = km_dbg_lines + 1;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (row[mid * 3] <= off)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo == 0)
        return;
    const uint32_t *r = row + (lo - 1) * 3;
    const char *f = km_dbg_files;
    for (uint32_t i = 0; i < r[2] && *f; i++)
        f += strlen(f) + 1;
    snprintf(out, cap, "%s:%u", f, (unsigned)r[1]);
}

/* Name the code at `pc` for a trace line ("Mesh::build", "strlen (libc.so.6)"). */
static void km_describe(uintptr_t pc, char *out, size_t cap, int *komodo)
{
    const char *s = km_exe_symbol(pc, komodo);
    if (s) {
        if (*komodo)
            km_pretty(s, out, cap);
        else
            snprintf(out, cap, "%s", s);
        return;
    }
    Dl_info di;
    if (dladdr((void *)pc, &di) && di.dli_fname) {
        const char *lib = strrchr(di.dli_fname, '/');
        lib = lib ? lib + 1 : di.dli_fname;
        snprintf(out, cap, "%s (%s)", di.dli_sname ? di.dli_sname : "??", lib);
        return;
    }
    snprintf(out, cap, "?? (%#lx)", (unsigned long)pc);
}

/* ── walking the call stack ────────────────────────────────────────
 * C frames are unwound with libgcc's unwinder (they carry .eh_frame);
 * generated code has no unwind tables but keeps a strict rbp frame
 * chain (push rbp / mov rbp, rsp in every prologue), so from the first
 * Komodo frame on the walk follows [rbp] / [rbp + 8].  Frames are kept
 * as addresses *inside* the calling instruction (return address - 1). */
#define KM_C_FRAMES 32

typedef struct {
    uintptr_t skip_until;            /* signal: the faulting pc; earlier frames are the handler's */
    int       started;
    uintptr_t c[KM_C_FRAMES];        /* C frames before the first Komodo one */
    int       nc;
    uintptr_t kpc, krbp;             /* the first Komodo frame and its rbp */
} KmWalk;

static uintptr_t km_stack_hi;        /* frames all lie below this */
static uintptr_t km_stack_lo;        /* ... and above the innermost sp */

static _Unwind_Reason_Code km_walk_cb(struct _Unwind_Context *ctx, void *arg)
{
    KmWalk *w = arg;
    int before = 0;
    uintptr_t pc = _Unwind_GetIPInfo(ctx, &before);
    if (!pc)
        return _URC_END_OF_STACK;
    if (!w->started) {
        if (w->skip_until && pc != w->skip_until)
            return _URC_NO_REASON;
        w->started = 1;
    }
    uintptr_t at = before ? pc : pc - 1;
    int komodo = 0;
    km_exe_symbol(at, &komodo);
    if (komodo) {
        w->kpc = at;
        w->krbp = _Unwind_GetGR(ctx, 6);    /* DWARF 6 = rbp */
        return _URC_END_OF_STACK;
    }
    if (w->nc < KM_C_FRAMES)
        w->c[w->nc++] = at;
    return _URC_NO_REASON;
}

typedef struct {
    const KmWalk *w;
    int           ci, stage;
    uintptr_t     rbp;
} KmIter;

/* Next frame address, innermost first; 0 at the end. */
static uintptr_t km_next_frame(KmIter *it)
{
    if (it->ci < it->w->nc)
        return it->w->c[it->ci++];
    if (it->stage == 0) {
        it->stage = 1;
        it->rbp = it->w->krbp;
        return it->w->kpc;
    }
    uintptr_t rbp = it->rbp;
    if (!it->w->kpc || (rbp & 7) || rbp < km_stack_lo || rbp + 16 > km_stack_hi)
        return 0;
    uintptr_t next = ((uintptr_t *)rbp)[0], ret = ((uintptr_t *)rbp)[1];
    int komodo = 0;
    if (!ret || !km_exe_symbol(ret - 1, &komodo) || !komodo)
        return 0;                    /* back in C: the runtime's main */
    it->rbp = next > rbp ? next : 0;
    return ret - 1;
}

/* Name of the innermost Komodo function on the current call stack. */
void km_komodo_caller(char *buf, size_t cap)
{
    KmWalk w;
    memset(&w, 0, sizeof w);
    _Unwind_Backtrace(km_walk_cb, &w);
    buf[0] = '\0';
    if (w.kpc) {
        int komodo;
        km_describe(w.kpc, buf, cap, &komodo);
    }
}

/* After an uncaught error: the Komodo call chain that raised it, one
 * `  at fn (file.km:12)` line per frame.  Printed for programs built with
 * KM_DEBUG=1 (which carry a line table), and for any program run with
 * KM_TRACE set in the environment (function names only).  Plain builds
 * print just the error line. */
static void km_print_trace(const KmWalk *w, int uncaught);

static void km_uncaught_trace(void)
{
    if (!km_dbg_lines && !getenv("KM_TRACE"))
        return;
    KmWalk w;
    memset(&w, 0, sizeof w);
    km_stack_lo = 0;
    _Unwind_Backtrace(km_walk_cb, &w);
    if (w.kpc)
        km_print_trace(&w, 1);
}

#else  /* __EMSCRIPTEN__: no ELF image or signals — traps surface as JS errors */

static void km_uncaught_trace(void) {}

void km_komodo_caller(char *buf, size_t cap)
{
    if (cap)
        buf[0] = '\0';
}

#endif

/* `arr[i]` with i outside 0 .. #arr-1 (generated code checks inline and
 * calls here).  Thrown like any Komodo error, so `f(...)?` can catch it. */
void km_index_error(int64_t index, KmArray *a)
{
    char where[160] = "";
#ifndef __EMSCRIPTEN__
    int komodo = 0;
    const char *s = km_exe_symbol((uintptr_t)__builtin_return_address(0) - 1, &komodo);
    if (s && komodo)
        km_pretty(s, where, sizeof where);
#endif
    char buf[256];
    snprintf(buf, sizeof buf, "array index %" PRId64 " out of range (length %" PRId64 ")%s%s",
             index, a->len, *where ? " in " : "", where);
    km_throw(km_strndup(buf, strlen(buf)));
}

#ifndef __EMSCRIPTEN__

/* ── crash signals ─────────────────────────────────────────────────
 * One line saying what went wrong and where, then the Komodo call
 * stack, then exit(128 + signal) — the status a shell shows for that
 * signal, without a core dump (dumping a multi-GB process can stall a
 * desktop for minutes).  The handler runs on its own stack, so a stack
 * overflow can still be reported. */
#define KM_TRACE_HEAD 24
#define KM_TRACE_TAIL 6

static void km_write(const char *s)
{
    size_t n = strlen(s);
    while (n > 0) {
        ssize_t w = write(2, s, n);
        if (w <= 0)
            return;
        s += w;
        n -= (size_t)w;
    }
}

/* Print the walk, folding a run of the same call site ("fib  x 104857")
 * and eliding the middle of very deep stacks. */
static void km_print_trace(const KmWalk *w, int uncaught)
{
    KmIter it = { w, uncaught ? w->nc : 0, 0, 0 };   /* an error report skips the C frames */
    int64_t runs = 0;
    uintptr_t prev = 0, f;
    while ((f = km_next_frame(&it)) != 0) {
        if (f != prev)
            runs++;
        prev = f;
    }
    const char *ind = uncaught ? "  at " : "    ";
    if (!uncaught)
        km_write("Komodo call stack (innermost first):\n");
    KmIter jt = { w, uncaught ? w->nc : 0, 0, 0 };
    int64_t run = 0;
    uintptr_t cur = km_next_frame(&jt);
    while (cur) {
        int64_t count = 1;
        uintptr_t nx;
        while ((nx = km_next_frame(&jt)) == cur)
            count++;
        if (run < KM_TRACE_HEAD || run >= runs - KM_TRACE_TAIL) {
            char name[200], at[260], line[560];
            int komodo;
            km_describe(cur, name, sizeof name, &komodo);
            km_where(cur, at, sizeof at);
            if (*at)
                snprintf(line, sizeof line, "%s%s (%s)", ind, name, at);
            else
                snprintf(line, sizeof line, "%s%s", ind, name);
            if (count > 1)
                snprintf(line + strlen(line), 60, "   (x %" PRId64 ", recursive)", count);
            strcat(line, "\n");
            km_write(line);
        } else if (run == KM_TRACE_HEAD) {
            char line[96];
            snprintf(line, sizeof line, "%s... %" PRId64 " more ...\n", ind, runs - KM_TRACE_HEAD - KM_TRACE_TAIL);
            km_write(line);
        }
        run++;
        cur = nx;
    }
}

static volatile sig_atomic_t km_crashing;

static void km_on_crash(int sig, siginfo_t *si, void *ucv)
{
    if (km_crashing++) {
        km_write("Komodo: crashed again while reporting a crash\n");
        _exit(128 + sig);
    }
    ucontext_t *uc = ucv;
    uintptr_t rip = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
    uintptr_t rsp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    uintptr_t rbp = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
    uintptr_t addr = (uintptr_t)si->si_addr;
    km_stack_lo = rsp - 128;
    fflush(stdout);                  /* keep the program's output so far */

    char what[96];
    switch (sig) {
    case SIGSEGV:
        if (addr < km_stack_hi && addr + 65536 > rsp && addr < rsp + 65536)
            snprintf(what, sizeof what, "stack overflow (recursion too deep)");
        else if (addr < 65536)
            snprintf(what, sizeof what, "invalid memory access (a null value was used)");
        else
            snprintf(what, sizeof what, "invalid memory access at address %#lx", (unsigned long)addr);
        break;
    case SIGBUS:
        snprintf(what, sizeof what, "bus error (invalid memory access at %#lx)", (unsigned long)addr);
        break;
    case SIGFPE:
        snprintf(what, sizeof what, "%s", si->si_code == FPE_INTDIV ? "integer division by zero"
                 : si->si_code == FPE_INTOVF ? "integer overflow" : "arithmetic error");
        break;
    case SIGILL:
        snprintf(what, sizeof what, "illegal instruction");
        break;
    default:
        snprintf(what, sizeof what, "aborted");
        break;
    }

    KmWalk w;
    memset(&w, 0, sizeof w);
    w.skip_until = sig == SIGABRT ? 0 : rip;
    _Unwind_Backtrace(km_walk_cb, &w);
    if (!w.started) {                /* the unwinder could not cross the signal frame */
        int komodo = 0;
        memset(&w, 0, sizeof w);
        km_exe_symbol(rip, &komodo);
        if (komodo) {
            w.kpc = rip;
            w.krbp = rbp;
        } else {
            w.c[w.nc++] = rip;
        }
    }

    char where[200] = "", line[400];
    if (w.kpc) {
        int komodo;
        km_describe(w.kpc, where, sizeof where, &komodo);
        char at[260];
        km_where(w.kpc, at, sizeof at);
        if (*at)
            snprintf(where + strlen(where), sizeof where - strlen(where), " (%s)", at);
    }
    snprintf(line, sizeof line, "Komodo: crash - %s%s%s\n", what, *where ? " in " : "", where);
    km_write(line);
    km_print_trace(&w, 0);
    _exit(128 + sig);
}

void km_crash_init(void)
{
    extern void *__libc_stack_end;
    km_stack_hi = (uintptr_t)__libc_stack_end;

    /* the handler's own stack, so a stack overflow can be reported */
    size_t alt = 256 * 1024;
    void *mem = mmap(NULL, alt, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (mem != MAP_FAILED) {
        stack_t ss = { .ss_sp = mem, .ss_size = alt, .ss_flags = 0 };
        sigaltstack(&ss, NULL);
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = km_on_crash;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    int sigs[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
    for (size_t i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
        sigaction(sigs[i], &sa, NULL);
}

#else

void km_crash_init(void) {}

#endif
