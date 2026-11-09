/*
 * process — Komodo standard subprocess helpers, reached through the
 * uniform call ABI (rdi = argc, rsi = KmArg records).
 *
 * Both run the command through the shell (sh -c).  Under wasm
 * (Emscripten) there is no fork / popen, so every call reports failure:
 * process_run returns -1 and process_output returns "".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "km_runtime.h"
#if !defined(__EMSCRIPTEN__)
#include <sys/wait.h>
#endif

/* process.run(cmd) : int — run `cmd` with the program's stdin / stdout /
 * stderr inherited.  Returns the exit status; a command killed by a
 * signal returns 128 + signal number; -1 when it cannot be started. */
int64_t process_run(int64_t argc, KmArg *items)
{
    if (argc < 1) return -1;
    const char *cmd = KM_STR(items[0].value);
    if (!cmd || !*cmd) return -1;
#if defined(__EMSCRIPTEN__)
    return -1;
#else
    fflush(NULL);                       /* keep our output ahead of the child's */
    int st = system(cmd);
    if (st == -1) return -1;
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
    return -1;
#endif
}

/* process.output(cmd) : string — the command's standard output, captured
 * whole.  Its stderr is inherited; "" when the command cannot be started. */
const char *process_output(int64_t argc, KmArg *items)
{
    if (argc < 1) return km_strndup("", 0);
    const char *cmd = KM_STR(items[0].value);
    if (!cmd || !*cmd) return km_strndup("", 0);
#if defined(__EMSCRIPTEN__)
    return km_strndup("", 0);
#else
    fflush(NULL);
    FILE *p = popen(cmd, "r");
    if (!p) return km_strndup("", 0);
    size_t cap = 4096, n = 0;
    char *buf = km_alloc_atomic(cap);
    for (;;) {
        if (n + 1 >= cap) {
            char *nb = km_alloc_atomic(cap * 2);
            memcpy(nb, buf, n);
            buf = nb;
            cap *= 2;
        }
        size_t got = fread(buf + n, 1, cap - 1 - n, p);
        n += got;
        if (got == 0) break;
    }
    buf[n] = '\0';
    pclose(p);
    return buf;
#endif
}
