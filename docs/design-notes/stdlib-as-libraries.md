# Plan: standard library as ordinary C-ABI libraries

## Goal

`math` / `algorithm` / `file` / `console` stop being compiler-known
prototypes on Komodo's tagged `(argc, KmArg*)` ABI. Each becomes an
ordinary module: a `.kmm` interface of `extern fn` C-ABI prototypes plus
its own `link "…"` line — exactly the `stdlib/sdl2.kmm` pattern. Nothing
in the compiler, the runtime header, or the build scripts special-cases
them.

## What stays "runtime" (always linked, not a library)

- `stdlib/runtime.c`  — operator lowerings, `KmArray`, `km_str_*`,
  `km_array_*`, `km_to_string`, `km_program_main` + `main()` shim.
- `stdlib/km_error.c` — `throw` / protected-call support.
- `stdlib/km_runtime.h` — `KmArray`, `KmArg`, `km_as_float`, and the
  compiler-internal runtime prototypes only. The `module: console /
  algorithm / math` prototype blocks are **removed**.

The tagged `(argc, KmArg*)` ABI itself is unchanged — it is still how
every *Komodo* function (user code included) is called. Only the C
standard-library bindings move off it.

## Change 1 — `link "path"` resolves against the declaring module's dir

`main.km` `gather()` currently keeps a quoted `link` target verbatim
(CWD-relative). A library imported from another directory then can't find
its `.c`. Fix: in `gather`, a link token that is **not** `-l…` is
resolved against `dirOf(p)` (the file that declared it), unless already
absolute. `stdlib/math.kmm`'s `link "math.c"` → `<compilerdir>/stdlib/math.c`.

Touches: `main.km` (`gather`, `linkOnLine`/`linkTargets` helper). AST
`link` handling and the parser are unaffected (codegen never reads
`prog.links`; only the driver's text scan produces `<file>.links`).

design.md's "Standard library" section is updated to state the new
resolution rule.

## Change 2 — build scripts read `<src>.links`; drop the `stdlib/*.c` glob

`build.sh`, `krun.sh`, `build-self.sh` currently link `stdlib/*.c`
wholesale. New behaviour (matching the `komodo` CLI):

- always link `stdlib/runtime.c stdlib/km_error.c`
- read `<src>.links` (written by `main.km`) and append its tokens

`komodo` already reads `<src>.links`; it changes only in that its
always-linked set becomes `runtime.c km_error.c` instead of `stdlib/*.c`,
and its `obj/` cache keys on those two.

## Change 3 — `math`, `algorithm`, `file` → C-ABI `extern fn`

### stdlib/math.kmm
```
module math
link "math.c"
extern fn math_abs(n: int): int
extern fn math_min(a: int, b: int): int
extern fn math_max(a: int, b: int): int
extern fn math_pow(base: int, exp: int): int
extern fn math_gcd(a: int, b: int): int
extern fn math_sqrt(x: float): float
```
### stdlib/math.c — plain C signatures, no `km_runtime.h`
`long math_abs(long n)`, … `double math_sqrt(double x)`.

### stdlib/algorithm.kmm — `extern` on each; `[int]` arg = the array handle
```
module algorithm
link "algorithm.c"
extern void algorithm_sort(a: [int])
extern void algorithm_reverse(a: [int])
extern fn  algorithm_sum(a: [int]): int
extern fn  algorithm_max(a: [int]): int
extern fn  algorithm_min(a: [int]): int
extern fn  algorithm_index_of(a: [int], v: int): int
extern fn  algorithm_contains(a: [int], v: int): int
extern void algorithm_fill(a: [int], v: int)
```
### stdlib/algorithm.c — `void algorithm_sort(KmArray *a)`, …
Still `#include "km_runtime.h"` for the `KmArray` layout (that header is
the documented ABI contract; `-Istdlib` is on every link step).

### stdlib/file.kmm — symbols stay **unprefixed**
`file::name` drops the namespace to a bare `name`, so the C symbols must
stay `exists` / `read_all` / `write_all` / `find` / `list_dir`.
```
module file
link "file.c"
extern fn  exists(path: string): int
extern fn  read_all(path: string): string
extern void write_all(path: string, data: string)
extern fn  find(name: string, from: string): string?
extern fn  list_dir(path: string): [string]
```
### stdlib/file.c — `long exists(const char *path)`, … `KmArray *list_dir(const char *path)`.

## Change 4 — codegen: route dotted calls through the C-ABI path

`Codegen::genCall` looks up `fnKind(e.name)` with the **raw** dotted name
(`"math.abs"`), which never matches the registered symbol (`math_abs`), so
today the call silently falls to the tagged-ABI path. Fix: mangle once at
the top of the non-method branch —

```
let callName = e.name
if dotIndex(e.name) > 0 and not <method receiver> then
    callName = m_backend.mangle(e.name)      // math.abs -> math_abs
let kind = fnKind(callName)
```

then use `callName` for `fnKind` / `fnArity` / `cCallEnd` / `callFrameEnd`
/ `call0`. `mangle` is idempotent on already-flat names, so user calls and
`file::`-style calls are unaffected. sema already resolves these
(`mangleDotted` + `resolveFnCall` with the `extern` arg rule) — no sema
change.

## Change 5 — console: overloaded Komodo writers + `print` builtin

Per the chosen option: `console`'s own functions are ordinary Komodo
overloads calling one tiny C primitive; the variadic entry stays a
compiler builtin that dispatches to them per argument.

### stdlib/console.c  (the whole C side)
```c
#include <stdio.h>
void km_put(const char *s, long err) { fputs(s ? s : "", err ? stderr : stdout); }
```
### stdlib/console.kmm  (real Komodo, with type-directed overloads)
```
module console
link "console.c"

extern void km_put(s: string, err: int)

void write(v: int)      km_put(string(v), 0)
void write(v: float)    km_put(string(v), 0)
void write(v: string)   km_put(v, 0)
void write(v: bool)     km_put(string(v), 0)

void error(v: int)      km_put(string(v), 1)
void error(v: float)    km_put(string(v), 1)
void error(v: string)   km_put(v, 1)
void error(v: bool)     km_put(string(v), 1)
```
(`string(v)` is the existing builtin; overload resolution picks the writer
from `v`'s static type, or by runtime type-id for an object value.)

### `print(...)` / `console.print(...)` — compiler builtin, per-arg lowering
In `genCall`, recognise `print` and `console.print` (and the variadic
`console.write` / `console.error` uses, which keep working):
for each argument, in source order, emit `string(arg)` then a call to the
matching `console` writer overload (`km_put` for the primitive path),
inserting `" "` separators and a trailing `"\n"` for `print`. No tagged
`KmArg` frame is built.

Because `print` now needs the `console` module compiled in, `main.km`
gains `import console` (it already relies on bare `print` in
`km_compiler.kmm` / `sema.kmm`). All example/demo programs already
`import console`.

Fallback considered: if wiring the builtin to the Komodo overloads proves
too invasive in one pass, the intermediate step is the builtin lowering
each arg straight to `km_put(string(arg), 0)` and `console.kmm` keeping
just the `write`/`error` overloads for direct calls — same C surface,
same ABI, slightly less "dispatches to the overloads".

## Change 6 — docs

- design.md: "Standard library" section rewritten — every stdlib module is
  now the `sdl2.kmm` pattern (`extern fn` + `link`); note the runtime
  (`runtime.c` / `km_error.c`) as the only always-linked C; note the
  module-relative `link "path"` resolution. "Built-ins" section: `print`
  stays; add `write` / `error` note if kept as builtins.
- `stdlib/km_runtime.h` header comment trimmed to the runtime-only role.

## Verification

1. `rm -rf ~/.km/cache`
2. `./build-self.sh` — bootstrap from `./kmc`, then the **fixpoint**
   check (fresh `kmc` reproduces `main.km.asm` byte-for-byte).
3. `./scripts/run-tests.sh` — every `tests/examples/*.km` (x86 run + stackvm compile);
   `stdlib_demo`, `median`, `mod_demo` cover `math` / `algorithm` /
   `console`.
4. `./scripts/run-sema-tests.sh` — analyser diagnostics unchanged.
5. Spot-check `demos/sdl_window.km` still links via its own `link SDL2`.
6. Inspect `main.km.asm`: `call $math_abs` / `call km_put`, and **no**
   `callFrameBegin`-style tagged frame for `math.*` / `algorithm.*`.

## Files touched

- `stdlib/math.kmm`, `stdlib/math.c`
- `stdlib/algorithm.kmm`, `stdlib/algorithm.c`
- `stdlib/file.kmm`, `stdlib/file.c`
- `stdlib/console.kmm`, `stdlib/console.c`
- `stdlib/km_runtime.h`  (drop stdlib prototype blocks)
- `modules/codegen/codegen_expr.kmm`  (dotted-name mangle; `print` builtin lowering)
- `main.km`  (module-relative `link` path; `import console`)
- `build.sh`, `krun.sh`, `build-self.sh`  (read `.links`; drop `stdlib/*.c` glob)
- `komodo`  (always-linked set: `runtime.c` + `km_error.c`)
- `design.md`

## Risks

- **Fixpoint**: `main.km` compiles itself; any behavioural drift in the
  `print` lowering or `file::` calls breaks the byte-identical check.
  Mitigation: keep the bare-`print` lowering output identical in shape to
  today where it matters, verify with a diff of `main.km.asm`.
- **`math.sqrt(2)` with an int literal** would pass in a GP reg, not xmm
  (pre-existing extern-ABI limitation; callers pass `2.0`). Left as-is.
- **`komodo` build cache** keys on `stdlib/*.c` contents; the key
  definition changes, so first rebuild is cold. Harmless.
