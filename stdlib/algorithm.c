/*
 * algorithm — Komodo standard algorithms over arrays.
 *
 * Reached through the uniform Komodo call ABI (see km_runtime.h).  For
 * every routine here argument 0 is an array handle:
 *
 *     [ int64 len | int64 cap | int64 data[] ]
 */
#include "km_runtime.h"

#define ARR(items)  ((KmArray *)(items)[0].value)

void algorithm_sort(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);

    for (long i = 1; i < a->len; i++) {
        long key = a->data[i];
        long j = i - 1;
        while (j >= 0 && a->data[j] > key) {
            a->data[j + 1] = a->data[j];
            j--;
        }
        a->data[j + 1] = key;
    }
}

void algorithm_reverse(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);

    for (long i = 0, k = a->len - 1; i < k; i++, k--) {
        long t = a->data[i];
        a->data[i] = a->data[k];
        a->data[k] = t;
    }
}

long algorithm_sum(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long total = 0;
    for (long i = 0; i < a->len; i++)
        total += a->data[i];
    return total;
}

long algorithm_max(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long best = a->data[0];
    for (long i = 1; i < a->len; i++)
        if (a->data[i] > best)
            best = a->data[i];
    return best;
}

long algorithm_min(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long best = a->data[0];
    for (long i = 1; i < a->len; i++)
        if (a->data[i] < best)
            best = a->data[i];
    return best;
}

long algorithm_index_of(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long needle = items[1].value;
    for (long i = 0; i < a->len; i++)
        if (a->data[i] == needle)
            return i;
    return -1;
}

long algorithm_contains(long argc, KmArg *items)
{
    return algorithm_index_of(argc, items) >= 0;
}

void algorithm_fill(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long value = items[1].value;
    for (long i = 0; i < a->len; i++)
        a->data[i] = value;
}
