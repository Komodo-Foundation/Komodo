/*
 * random — Komodo seeded pseudo-random numbers, reached through the
 * uniform call ABI (rdi = argc, rsi = KmArg records).
 *
 * Deliberately NOT libc rand(): the generator is splitmix64, pure 64-bit
 * integer arithmetic, so a given seed yields the identical sequence on
 * every platform and compiler.  The state starts from a fixed seed, so a
 * program that never calls random.seed is still deterministic.
 */
#include <stdint.h>
#include "km_runtime.h"

static uint64_t rng_state = 0x9E3779B97F4A7C15ULL;

static uint64_t next_u64(void)
{
    uint64_t z = (rng_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Uniform integer in [0, n) for n >= 1, rejection-sampled to avoid bias. */
static uint64_t below(uint64_t n)
{
    uint64_t limit = UINT64_MAX - (UINT64_MAX % n);
    uint64_t r;
    do r = next_u64(); while (r >= limit);
    return r % n;
}

/* random.seed(n) — restart the sequence from integer seed `n`. */
void random_seed(int64_t argc, KmArg *items)
{
    if (argc < 1) return;
    rng_state = (uint64_t)items[0].value;
}

/* random.int(lo, hi) : int — uniform in the inclusive range [lo, hi]
 * (the bounds are swapped if given backwards). */
int64_t random_int(int64_t argc, KmArg *items)
{
    if (argc < 2) return 0;
    int64_t lo = items[0].value, hi = items[1].value;
    if (hi < lo) { int64_t t = lo; lo = hi; hi = t; }
    uint64_t span = (uint64_t)hi - (uint64_t)lo;   /* inclusive span - 1 */
    if (span == UINT64_MAX) return (int64_t)next_u64();
    return (int64_t)((uint64_t)lo + below(span + 1));
}

/* random.float() : float — uniform in [0, 1) with 53 random bits. */
double random_float(int64_t argc, KmArg *items)
{
    (void)argc; (void)items;
    return (double)(next_u64() >> 11) * (1.0 / 9007199254740992.0);
}

/* random.shuffle(xs) — Fisher-Yates, in place, on an [int] array. */
void random_shuffle(int64_t argc, KmArg *items)
{
    if (argc < 1) return;
    KmArray *a = KM_ARR(items[0].value);
    if (!a) return;
    for (int64_t i = a->len - 1; i > 0; i--) {
        int64_t j = (int64_t)below((uint64_t)i + 1);
        int64_t t = a->data[i];
        a->data[i] = a->data[j];
        a->data[j] = t;
    }
}
