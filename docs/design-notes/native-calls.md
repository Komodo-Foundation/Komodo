# Plan: C-style calls (remove the KmArg overhead)

Status: stage 1 (plan). Approved by the maintainer as a full rewrite, done in
four stages, each its own PR that keeps `./build-self.sh` at `fixpoint OK`
and refreshes the seed.

## Today

Every free Komodo function has the same shape:

    int64_t f(int64_t argc, KmArg *items);   /* rdi = argc, rsi = items */
    /* KmArg { int64_t tag; int64_t value; } */

The caller builds a `KmArg[n]` frame on each call (`callFrameBegin`,
`callArgTag`, `callArgStore`), tags every argument, and the callee reads
`items[i].value` and ignores the tag except where it matters. This costs a
stack frame, two stores per argument, and a tag check per use, and it means
C callers and stdlib code must speak `KmArg`.

The x86 backend already has a cheaper path, `$f` (see
`modules/backend/backend_x86.kmm`, `callFast`, and `isFastFn`). A free
function with 1–6 parameters gets a second entry that takes its arguments in
the System V integer registers (`fastArgReg`). Direct calls use it. The
ordinary `KmArg` entry is kept as a wrapper that unpacks the frame and falls
into `$f`. Limits today: x86 only, direct calls only, not `export`, not
`main`, not variadic, not constructors.

## Target

Every Komodo function has a plain C signature over typed, unboxed arguments.
A `string` is a `const char *`, an array is a `KmArray *`, a `float` is a
`double`, `int`/`bool`/`char` are `int64_t` words, and objects are pointers.

    int64_t add(int64_t a, int64_t b);
    double  scale(double x, int64_t k);
    const char *greet(const char *name);

No `argc`, no `items`, no tags. A call site passes its arguments and nothing
else. Overloads and generic instances keep their current mangled names.

Tags are still needed where the runtime must know a value's type at run time
(printing `console.print`, `km_to_string`, `throw`). Those stay as explicit
typed helpers that the codegen selects at compile time, not as a per-call
frame. Where a value is untyped at compile time, the codegen already
dispatches statically.

## Stages

1. **Plan (this note).** No behaviour change.
2. **Backends emit C-style calls.** x86 gets the register convention for all
   free functions (the existing `$f` becomes the only entry). C backend emits
   typed prototypes. wasm and stackvm keep their own frames internally but
   use the same typed signature at the boundary. Callbacks passed to C
   (`km_tramp_*`) keep a trampoline that adapts to the C signature.
3. **Stdlib C files move to typed functions.** `math`, `algorithm`, `file`,
   `console`, `string`, `os`, `path`, `time`, `random`, `process` drop
   `(int64_t argc, KmArg *items)` and take real parameters. `console`
   printing uses typed helpers.
4. **Remove KmArg.** Delete `KmArg` and the tag helpers from
   `stdlib/km_runtime.h`, remove the `$f` wrapper, update `docs/internals.md`
   (section 9), `docs/ffi.md` ("Runtime ABI for runtime C files"), and
   `docs/design-notes/stdlib-as-libraries.md`, and rewrite the companion-C
   examples in `docs/ffi.md`.

## Constraints

- Keep `tools/perf-budget.sh` ceilings. Measure before and after each stage.
  A call-heavy benchmark should not get slower.
- Keep every program in `examples/` and `tests/examples/` compiling and
  producing the same output on x86, C, wasm and stackvm.
- Refresh the seed (`./build-self.sh --force --update-seed`) and commit it
  in the same PR as the compiler change.
- Do not use the new convention in the compiler's own sources until the
  checked-in seed supports it.

## Open questions for the maintainer

1. Stage 2 changes what the stackvm and wasm frames look like at the
   boundary. Is it fine if stackvm keeps an internal frame and only the
   call boundary becomes typed?
2. Functions that are passed to C need a trampoline in the typed form.
   Confirm that is acceptable rather than limiting callbacks to ints.
