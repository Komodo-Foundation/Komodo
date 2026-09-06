/*
 * algorithm — Komodo standard algorithms over arrays.
 *
 * These are the C *primitives*; the public `algorithm.sort` / `.sum` / …
 * surface is a set of overloaded Komodo wrappers in stdlib/algorithm.kmm
 * that pick `_i` (int elements) or `_f` (IEEE-754 double bit patterns)
 * from the argument type.  Reached through the uniform Komodo call ABI
 * (see km_runtime.h): argument 0 is an array handle
 *
 *     [ int64 len | int64 cap | int64 data[] ]
 */
#include "km_runtime.h"

#define ARR(items)  ((KmArray *)(items)[0].value)

/* ── integer-word primitives (also serve the word-wise float ops) ──── */

void km_algo_sort(long argc, KmArg *items)
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

void km_algo_reverse(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    for (long i = 0, k = a->len - 1; i < k; i++, k--) {
        long t = a->data[i];
        a->data[i] = a->data[k];
        a->data[k] = t;
    }
}

long km_algo_sum(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long total = 0;
    for (long i = 0; i < a->len; i++)
        total += a->data[i];
    return total;
}

long km_algo_max(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long best = a->data[0];
    for (long i = 1; i < a->len; i++)
        if (a->data[i] > best)
            best = a->data[i];
    return best;
}

long km_algo_min(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long best = a->data[0];
    for (long i = 1; i < a->len; i++)
        if (a->data[i] < best)
            best = a->data[i];
    return best;
}

long km_algo_index_of(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long needle = items[1].value;
    for (long i = 0; i < a->len; i++)
        if (a->data[i] == needle)
            return i;
    return -1;
}

long km_algo_contains(long argc, KmArg *items)
{
    return km_algo_index_of(argc, items) >= 0;
}

void km_algo_fill(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    long value = items[1].value;
    for (long i = 0; i < a->len; i++)
        a->data[i] = value;
}

/* ── float primitives — element words are IEEE-754 double bit patterns ── */

void km_algo_sortf(long argc, KmArg *items)
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

double km_algo_sumf(long argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    double total = 0.0;
    for (long i = 0; i < a->len; i++)
        total += km_as_float(a->data[i]);
    return total;
}

double km_algo_maxf(long argc, KmArg *items)
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

double km_algo_minf(long argc, KmArg *items)
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

/* float aliases for the order-independent ops — element words are just
 * copied / compared, so the integer primitive is bit-for-bit correct. */
void km_algo_reversef(long argc, KmArg *items)  { km_algo_reverse(argc, items); }
void km_algo_fillf(long argc, KmArg *items)     { km_algo_fill(argc, items); }
long km_algo_index_off(long argc, KmArg *items) { return km_algo_index_of(argc, items); }
long km_algo_containsf(long argc, KmArg *items) { return km_algo_contains(argc, items); }
