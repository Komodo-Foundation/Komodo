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

/* ── float variants ───────────────────────────────────────────────────
 *
 * Same array layout, but each element word is the IEEE-754 bit pattern of
 * a double.  `km_as_float` / `km_from_float` (km_runtime.h) reinterpret.
 */

void algorithm_sort_f(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    for (long i = 1; i < a->len; i++) {
        long key = a->data[i];
        double kd = km_as_float(key);
        long j = i - 1;
        while (j >= 0 && km_as_float(a->data[j]) > kd) {
            a->data[j + 1] = a->data[j];
            j--;
        }
        a->data[j + 1] = key;
    }
}

double algorithm_sum_f(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    double total = 0.0;
    for (long i = 0; i < a->len; i++)
        total += km_as_float(a->data[i]);
    return total;
}

double algorithm_max_f(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    double best = km_as_float(a->data[0]);
    for (long i = 1; i < a->len; i++) {
        double v = km_as_float(a->data[i]);
        if (v > best)
            best = v;
    }
    return best;
}

double algorithm_min_f(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    double best = km_as_float(a->data[0]);
    for (long i = 1; i < a->len; i++) {
        double v = km_as_float(a->data[i]);
        if (v < best)
            best = v;
    }
    return best;
}

/* word-wise on the element bits — identical to the int versions */
void algorithm_reverse_f(long argc, KmArg *items)  { algorithm_reverse(argc, items); }
void algorithm_fill_f(long argc, KmArg *items)     { algorithm_fill(argc, items); }
long algorithm_index_of_f(long argc, KmArg *items) { return algorithm_index_of(argc, items); }
long algorithm_contains_f(long argc, KmArg *items) { return algorithm_contains(argc, items); }
