/*
 * os — Komodo standard operating-system access, reached through the
 * uniform call ABI (rdi = argc, rsi = KmArg records).  Everything here
 * is portable across the x86 and C / wasm targets; wasm (Emscripten)
 * provides getenv / setenv / getcwd / getpid over its virtual FS and
 * process environment.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "km_runtime.h"

/* The process's argv, recorded by main() in runtime.c. */
extern int km_main_argc;
extern char **km_main_argv;

/* os.args() : [string] — the program's argv, argv[0] included (the same
 * array main receives).  Each call returns a fresh array. */
KmArray *os_args(int64_t argc, KmArg *items)
{
    (void)argc; (void)items;
    int n = km_main_argc;
    KmArray *out = km_array_new(n);
    for (int i = 0; i < n; i++)
        out->data[i] = KM_WORD(km_main_argv[i]);
    return out;
}

/* os.getenv(name) : string?  — NULL (null) when unset. */
const char *os_getenv(int64_t argc, KmArg *items)
{
    if (argc < 1) return NULL;
    const char *name = KM_STR(items[0].value);
    if (!name || !*name) return NULL;
    const char *v = getenv(name);
    if (!v) return NULL;
    return km_strndup(v, strlen(v));
}

/* os.setenv(name, value) — set (or overwrite) an environment variable. */
void os_setenv(int64_t argc, KmArg *items)
{
    if (argc < 2) return;
    const char *name = KM_STR(items[0].value);
    const char *value = KM_STR(items[1].value);
    if (!name || !*name || !value) return;
    setenv(name, value, 1);
}

/* os.exit(code) — terminate the program with status `code`. */
void os_exit(int64_t argc, KmArg *items)
{
    int code = argc >= 1 ? (int)items[0].value : 0;
    exit(code);
}

/* os.cwd() : string — the current working directory ("" on failure). */
const char *os_cwd(int64_t argc, KmArg *items)
{
    (void)argc; (void)items;
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof buf)) return km_strndup("", 0);
    return km_strndup(buf, strlen(buf));
}

/* os.pid() : int */
int64_t os_pid(int64_t argc, KmArg *items)
{
    (void)argc; (void)items;
    return (int64_t)getpid();
}

/* os.platform() : string */
const char *os_platform(int64_t argc, KmArg *items)
{
    (void)argc; (void)items;
#if defined(__EMSCRIPTEN__)
    const char *p = "wasm";
#elif defined(__APPLE__)
    const char *p = "darwin";
#elif defined(_WIN32)
    const char *p = "windows";
#elif defined(__linux__)
    const char *p = "linux";
#else
    const char *p = "unknown";
#endif
    return km_strndup(p, strlen(p));
}
