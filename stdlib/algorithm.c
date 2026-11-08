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

#define ARR(items)  KM_ARR((items)[0].value)

/* ── integer-word primitives (also serve the word-wise float ops) ──── */

void km_algo_sort(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    for (int64_t i = 1; i < a->len; i++) {
        int64_t key = a->data[i];
        int64_t j = i - 1;
        while (j >= 0 && a->data[j] > key) {
            a->data[j + 1] = a->data[j];
            j--;
        }
        a->data[j + 1] = key;
    }
}

void km_algo_reverse(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    for (int64_t i = 0, k = a->len - 1; i < k; i++, k--) {
        int64_t t = a->data[i];
        a->data[i] = a->data[k];
        a->data[k] = t;
    }
}

int64_t km_algo_sum(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    int64_t total = 0;
    for (int64_t i = 0; i < a->len; i++)
        total += a->data[i];
    return total;
}

int64_t km_algo_max(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    int64_t best = a->data[0];
    for (int64_t i = 1; i < a->len; i++)
        if (a->data[i] > best)
            best = a->data[i];
    return best;
}

int64_t km_algo_min(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    int64_t best = a->data[0];
    for (int64_t i = 1; i < a->len; i++)
        if (a->data[i] < best)
            best = a->data[i];
    return best;
}

int64_t km_algo_index_of(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    int64_t needle = items[1].value;
    for (int64_t i = 0; i < a->len; i++)
        if (a->data[i] == needle)
            return i;
    return -1;
}

int64_t km_algo_contains(int64_t argc, KmArg *items)
{
    return km_algo_index_of(argc, items) >= 0;
}

void km_algo_fill(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    int64_t value = items[1].value;
    for (int64_t i = 0; i < a->len; i++)
        a->data[i] = value;
}

/* ── float primitives — element words are IEEE-754 double bit patterns ── */

void km_algo_sortf(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    for (int64_t i = 1; i < a->len; i++) {
        int64_t key = a->data[i];
        double kd = km_as_float(key);
        int64_t j = i - 1;
        while (j >= 0 && km_as_float(a->data[j]) > kd) {
            a->data[j + 1] = a->data[j];
            j--;
        }
        a->data[j + 1] = key;
    }
}

double km_algo_sumf(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    double total = 0.0;
    for (int64_t i = 0; i < a->len; i++)
        total += km_as_float(a->data[i]);
    return total;
}

double km_algo_maxf(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    double best = km_as_float(a->data[0]);
    for (int64_t i = 1; i < a->len; i++) {
        double v = km_as_float(a->data[i]);
        if (v > best)
            best = v;
    }
    return best;
}

double km_algo_minf(int64_t argc, KmArg *items)
{
    (void)argc;
    KmArray *a = ARR(items);
    double best = km_as_float(a->data[0]);
    for (int64_t i = 1; i < a->len; i++) {
        double v = km_as_float(a->data[i]);
        if (v < best)
            best = v;
    }
    return best;
}

/* float aliases for the order-independent ops — element words are just
 * copied / compared, so the integer primitive is bit-for-bit correct. */
void km_algo_reversef(int64_t argc, KmArg *items)  { km_algo_reverse(argc, items); }
void km_algo_fillf(int64_t argc, KmArg *items)     { km_algo_fill(argc, items); }
int64_t km_algo_index_off(int64_t argc, KmArg *items) { return km_algo_index_of(argc, items); }
int64_t km_algo_containsf(int64_t argc, KmArg *items) { return km_algo_contains(argc, items); }
