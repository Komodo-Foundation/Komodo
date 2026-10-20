/*
 * console — Komodo standard console I/O.
 *
 * Reached through the uniform Komodo call ABI (see km_runtime.h):
 * `console.print(a, b, ...)` → `call console_print`, argc in rdi and the
 * KmArg records in rsi.  The compiler tags each argument; these routines
 * do all the formatting and arity handling.
 */
#include <stdio.h>
#include "km_runtime.h"

static void write_arg(FILE *out, KmArg a)
{
    /* an array-valued arg packs (leaf, depth) above the base tag — see
     * km_runtime.h — so dispatch on the low byte, not the raw tag. */
    char fbuf[64];
    switch (a.tag & 0xFF) {
    case KM_TAG_INT:   fprintf(out, "%ld", a.value);                 break;
    case KM_TAG_FLOAT: km_fmt_double(fbuf, sizeof fbuf, km_as_float(a.value)); fputs(fbuf, out); break;
    case KM_TAG_STR:   fputs((const char *)a.value, out);            break;
    case KM_TAG_BOOL:  fputs(a.value ? "true" : "false", out);       break;
    case KM_TAG_ARRAY: {
        fputs(km_array_to_string((KmArray *)a.value, (a.tag >> 8) & 0xFF, (a.tag >> 16) & 0xFF), out);
        break;
    }
    default:           fputs("<?>", out);                            break;
    }
}

static void write_all(FILE *out, long argc, KmArg *items, int newline)
{
    for (long i = 0; i < argc; i++) {
        if (i)
            putc(' ', out);
        write_arg(out, items[i]);
    }
    if (newline)
        putc('\n', out);
}

void console_print(long argc, KmArg *items) { write_all(stdout, argc, items, 1); }
void console_write(long argc, KmArg *items) { write_all(stdout, argc, items, 0); }
void console_error(long argc, KmArg *items) { write_all(stderr, argc, items, 1); }

/* Bare `print(...)` (used by the self-hosted compiler driver) is an
 * alias for `console.print`. */
void print(long argc, KmArg *items) { console_print(argc, items); }
