# Komodo

A **self-hosting** compiler for the **Komodo** language, written in Komodo
itself: `./kmc` compiles `main.km` + the `modules/` tree to a byte-identical copy
of itself. It lowers a program to native **x86-64** assembly (NASM) or to a
textual **stack-VM** bytecode, links the result against a small C runtime in
`stdlib/`, statically type-checks it first (`modules/sema*.kmm`), and can
optimize it (`modules/optimize.kmm`, four levels).

The reference Komodo interpreter (a separate project, `KomodoPy`) can also
run the compiler directly — useful for bootstrapping `kmc` on a machine that
doesn't have one yet, or for working on the compiler's own sources.

For the language itself — syntax, types, semantics — see [`design.md`](design.md).

## Installing

```bash
./install.sh                 # builds ./kmc if needed, symlinks it as `komodo`
komodo --version
```

See [Requirements](#requirements) below for what `install.sh` needs the first
time (it bootstraps `kmc` through the interpreter, once).

## The `komodo` CLI

```bash
komodo file.km                    # compile, link, run  (default)
komodo -O max -o app src/main.km  # optimize, name the output binary
komodo -c file.km                  # compile + link only, don't run
komodo -S file.km                  # emit file.km.asm only
komodo file.km -- -lSDL2           # extra linker flags / objects
komodo -h                          # full flag list
```

`komodo` is a thin wrapper: it drives `kmc` for the front end, then `nasm` +
`gcc` for assembly and linking. It resolves its own real path, so installing
it as a symlink on your `PATH` still finds this checkout's `kmc` / `stdlib/`.

## Layout

| Path | What |
|---|---|
| `komodo` | the CLI — compile/link/run wrapper around `kmc` (see above) |
| `install.sh` | builds `kmc` if needed and puts `komodo` on your `PATH` |
| `build-self.sh` | bootstraps `./kmc` (via the interpreter or an existing `kmc`) and checks the fixpoint |
| `main.km` | compiler driver: resolves `import`s (mirrors the interpreter's module loader), invokes the compiler |
| `modules/lexer.kmm` | tokenizer |
| `modules/parser.kmm` | recursive-descent parser → AST |
| `modules/ast.kmm` | AST node definitions |
| `modules/types.kmm` | the static type lattice |
| `modules/sema*.kmm` | `Analyzer`: static type-checking, flow narrowing, null-safety |
| `modules/optimize.kmm` | `Optimizer`: constant folding / dead-code elimination, 4 levels |
| `modules/codegen*.kmm` | target-independent lowering walk (frame layout, statements, expressions) |
| `modules/backend.kmm` | the `Backend` interface every target implements |
| `modules/backend_x86.kmm` | System V AMD64 / NASM backend (reference lowering) |
| `modules/backend_stackvm.kmm` | textual stack-VM bytecode backend |
| `modules/km_compiler.kmm` | `compileOpt` / `save` entry points, `selectBackend` |
| `stdlib/*.c` | C runtime: operator lowerings, the `console` / `math` / `algorithm` / `file` libraries |
| `stdlib/*.kmm` | signatures for the above — ordinary Komodo modules, no special-casing in the compiler |
| `examples/*.km` | sample programs, each with a `.expected` output fixture |
| `demos/` | larger standalone demos (e.g. an SDL window loop) |
| `tests/sema/` | negative tests for the analyser's diagnostics (`run-sema-tests.sh`) |

## Requirements

- `nasm` and `gcc` (for the x86 target — always needed)
- the Boehm garbage collector runtime, `libgc.so.1` (Debian / Ubuntu package
  `libgc1`) — every program links it; see *Memory* in `design.md`
- Python 3 and the Komodo interpreter (`KomodoPy/main.py`) — only needed
  once, to bootstrap `kmc` (`install.sh` / `build-self.sh`), or if you're
  editing the compiler's own sources and want the interpreter's better error
  messages while iterating. PyPy speeds the bootstrap up considerably if
  it's on `PATH` (`build-self.sh` picks it up automatically).

Point the build scripts at the interpreter if it isn't in one of the
well-known locations they check:

```bash
export KOMODO_MAIN="/path/to/KomodoPy/main.py"
```

## Building a program

The convenient way is the [`komodo` CLI](#the-komodo-cli) above. The
lower-level pieces it's built from are also usable directly:

```bash
./build-self.sh                              # bootstrap / rebuild ./kmc
./krun.sh <file.km> [output] [cc inputs...]   # compile+link+run with kmc — no interpreter
./build.sh <file.km> [target] [output] [cc inputs...]   # same, but via the interpreter
```

- `target` (build.sh only): `x86` (default) or `stackvm`
- x86: compiles → `<file>.km.asm`, assembles, links `stdlib/*.c` + any trailing
  `.c` / `.o` / `-l` arguments (also read from `$KM_CC_INPUTS`), then runs it
- `stackvm`: writes the bytecode text; there is no runner yet
- `$KM_OPT` (`none`/`low`/`medium`/`max`) selects the optimizer level for
  both scripts

```bash
./krun.sh examples/virtual_demo.km              # compile + run (native kmc)
./build.sh demos/sdl_window.km x86 ./w -lSDL2    # link an external library
KM_OPT=max ./krun.sh examples/stdlib_demo.km
```

`build.sh` runs `python3 $KOMODO_MAIN main.km <file.km> <target>` and clears
the interpreter's `~/.km/cache` each time, so edits to the `.kmm` sources
always take effect.

## Testing

```bash
./run-tests.sh              # every examples/*.km
./run-tests.sh oop_demo     # a subset
./run-sema-tests.sh         # the analyser's diagnostics (tests/sema/)
```

Each example is compiled + run on the x86 target and its output diffed against
`examples/<name>.expected`, then also compiled with the `stackvm` target to keep
the target-independent path honest (an example that needs the native C ABI
opts out of the stackvm pass with a `stackvm: skip` comment). Both scripts
drive the native compiler: `run-tests.sh` goes through `./komodo -q`,
`run-sema-tests.sh` calls `./kmc` directly. Build `./kmc` first with
`./build-self.sh`.

## Code generation

The lowering walk in `codegen*.kmm` never names a machine instruction — it
drives an operand stack through the `Backend` interface. To add a target,
implement `<T>Backend : Backend` and register it in `selectBackend`
(`km_compiler.kmm`); nothing in the lexer, parser, AST, or lowering walk
changes.

Objects carry a one-word header holding an integer type id. Structs are flat C
aggregates plus that tag (single inheritance appends fields); classes are
reference types with method inheritance, `abstract`, field defaults, and
virtual dispatch (direct call when monomorphic, otherwise through a per-class
vtable indexed by type id). `x is Type` is a compile-time-expanded type-id
check.

## Status

**Self-hosting**: `./kmc main.km x86` reproduces the exact assembly used to
build `kmc` itself (`build-self.sh` checks this fixpoint on every run), and
the resulting binary re-compiles itself again to the same output.

Working: variables and macros, control flow, functions with the `fn` / `mut` /
`void` contract, integers / floats / strings / bools / arrays, array & string
**slices** (`a[i to j]`), structs and classes with single inheritance and
virtual dispatch, `x is Type`, `enum` / `match`, `throw` / `f(args)?` error
handling, multi-file modules (with automatic sibling-module merging, like the
interpreter), `extern fn` C FFI (full System V AMD64 ABI), body-less function
*prototypes* and `...` variadic parameters (how the standard library declares
itself), and a static analyser (`modules/sema*.kmm`) with flow-sensitive
null-safety, ahead of an optional constant-folding / dead-code optimizer.
Also: interpolated strings (`` `n = {n + 1}` ``), hex literals (`0xFF`),
`_` digit separators and binary literals (`1_000_000`, `0b1010`),
`int(x)` float→int truncation, `f32` and by-value C structs (`copy(x)` to an
`extern fn`, e.g. raylib's `Vector3` — see `stdlib/raylib.kmm`).

Diagnostics are reported as `file:line:col: error: …` (or `warning:`)
against the file the problem is in, imported modules included.

**Compile speed**: `kmc` compiles its own ~11k lines in about 0.25 s
(one front-end pass; lexing, slicing and name lookup are all linear).

## License

MIT — see [`LICENSE`](LICENSE). Copyright (c) 2026 Lukas Perkins.
