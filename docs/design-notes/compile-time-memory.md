# Freeing memory at compile time

Status: design note, nothing implemented. Programs allocate through the Boehm
collector (`libgc`; `km_alloc_atomic` in `stdlib/runtime.c`). The goal is to
free some allocations without any dev annotation, keeping the GC as the
fallback for everything the compiler cannot prove.

## Measured first

Benchmark: 2M iterations of `"alpha" + "-" + "beta" + "-" + "gamma" + "-" + "delta"`.

| Form | Time |
|---|---|
| chained `+` (6 concats, 6 allocations) | 0.335 s |
| `join([...7 parts...], "")` (array + 1 string) | 0.271 s |

So fusing a 7-part chain through the existing `join` helper saves about 19%,
and less for short chains, because the array it builds is itself an
allocation. That is too small a win to justify a rewrite in the code
generator. A native n-ary helper would do better, but it needs a new
operation in all three backends (x86, stackvm, C), so it is not a quick
prototype.

A cheaper trick does **not** work: extending the left operand in place
(amortised append, as Go slices do). Strings are NUL-terminated, so writing
the new bytes would overwrite the terminator that the old, still-live
string relies on. A loop `s = s + x` stays quadratic until strings carry a
length or a builder type exists.

## Plan

1. **Escape analysis (the real win).** Per function, mark a string or array
   temporary as non-escaping when it is never returned, stored in a field,
   global or array element, captured, or passed to a function that keeps it.
   Non-escaping temporaries are freed at scope exit (or reused each loop
   iteration). Anything unproven stays GC-managed. No syntax change.
   Best done on a typed IR (not written yet), which would have
   the per-expression types this needs.
2. **N-ary concat.** `a + b + c + d` becomes one `km_str_concat_n`: measure
   total length, allocate once, copy once. Needs a backend operation in
   x86, stackvm and C.
3. **Last-use free for `unique` values.** The analyser already tracks moves
   of `unique` values, so the free can go right after the last use.
4. **Inferred ownership / reference counting (Perceus-style).** Largest
   coverage, largest project. Revisit after 1–3 show how much remains.

Open questions for the maintainer: whether temporaries are freed with
`free()`-style explicit calls into the collector (`GC_free`) or stack/arena
allocated; and whether a stdlib string builder should ship alongside, since
it removes the quadratic loop that no compiler analysis fixes.
