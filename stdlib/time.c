/*
 * time — Komodo standard clock access, reached through the uniform call
 * ABI (rdi = argc, rsi = KmArg records).
 */
#include <time.h>
#include <string.h>
#include "km_runtime.h"

/* time.now() : int — wall-clock unix time, whole seconds. */
int64_t time_now(int64_t argc, KmArg *items)
{
    (void)argc; (void)items;
    return (int64_t)time(NULL);
}

/* Monotonic milliseconds since an unspecified epoch. */
static int64_t mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
}

/* time.millis() : int — monotonic milliseconds; differences are meaningful. */
int64_t time_millis(int64_t argc, KmArg *items)
{
    (void)argc; (void)items;
    return mono_ms();
}

/* time.sleep_ms(ms) — block for at least `ms` milliseconds.  Under wasm
 * there is no blocking sleep on the main thread, so it spins instead. */
void time_sleep_ms(int64_t argc, KmArg *items)
{
    if (argc < 1) return;
    int64_t ms = items[0].value;
    if (ms <= 0) return;
#if defined(__EMSCRIPTEN__)
    int64_t until = mono_ms() + ms;
    while (mono_ms() < until)
        ;
#else
    struct timespec ts = { (time_t)(ms / 1000), (long)((ms % 1000) * 1000000) };
    while (nanosleep(&ts, &ts) != 0)
        ;   /* resume after EINTR with the remaining time */
#endif
}

/* time.format(secs, fmt) : string — strftime of the UTC broken-down time.
 * Empty string when the value or format cannot be rendered. */
const char *time_format(int64_t argc, KmArg *items)
{
    if (argc < 2) return km_strndup("", 0);
    time_t t = (time_t)items[0].value;
    const char *fmt = KM_STR(items[1].value);
    struct tm tmv;
    if (!fmt || !gmtime_r(&t, &tmv)) return km_strndup("", 0);
    char buf[512];
    size_t n = strftime(buf, sizeof buf, fmt, &tmv);
    return km_strndup(buf, n);
}
