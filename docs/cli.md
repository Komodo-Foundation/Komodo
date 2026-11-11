# Komodo CLI reference

This page covers the `komodo` command, the direct `kmc` compiler, the REPL,
the build commands and the test scripts. For a first run see
[getting-started.md](getting-started.md).

```bash
komodo                          # build and run the project in this directory
komodo file.km [args...]        # build and run one file; args go to the program
komodo <command> [options]
```

`komodo` is a bash driver. It runs `kmc` for the front end, then `nasm` and
`gcc` (or `emcc`) for assembly, linking and the wasm target, and runs the
program. It finds its own checkout through symlinks, so the copy installed in
`~/.local/bin` uses this checkout's `kmc` and `stdlib/`. There is no separate
compiler build step: when `./kmc` is missing or older than its sources, the
next `komodo` run builds it (`first run: building the compiler (~2s)`).

## The project

With no file argument, `komodo` looks in the current directory for the entry
file, in this order: `main.km`, `src/main.km`, `<directory name>.km`, and
finally the only `.km` file there (not counting `*_test.km`). If none
qualifies it prints the candidates and what to do. `komodo <dir>` does the
same for another directory.

The **project root** is the entry file's directory (its parent, when that is
named `src` or `tests`). Build products of `komodo` / `komodo run` go to
`<root>/.komodo/`, which gets a `.gitignore` of `*` automatically, so the
source tree stays clean. Only `komodo build`, `komodo lib` and explicit `-o`
write anything elsewhere.

Everything after the file is passed to your program:
`komodo main.km --verbose 3` runs it with `--verbose 3`.

## Commands

| Command | What it does |
|---|---|
| `komodo [run] [file]` | Build incrementally and run. The default. |
| `komodo build [file]` | Build an executable: `./<project name>` (or the file name without `.km`), or `-o <path>`. Does not run it. |
| `komodo test [path...]` | Build and run every `*_test.km` file, plus every `.km` under a `tests/` directory that defines `void main()`. Exit status 0 means pass. Prints one `ok` / `FAIL` line per file (output shown for failures) and a summary; exits 1 if any failed. Paths may be files or directories (default `.`). |
| `komodo check [file\|dir]` | Analyse only. Prints the compiler's diagnostics to stdout, one per line, as `file:line:col: error: …` (or `warning: …`), and exits 1 if there is an error. A clean program prints nothing and exits 0. Nothing is linked or run. It writes only the work directory `.komodo/` and `komodo_bindings/` (auto-bound imports), as a normal run does. Honours `--strict`. See [Checking](#checking). |
| `komodo lint [file\|dir]` | `komodo check` plus lint warnings: the `--strict` checks (unused locals and parameters, non-exhaustive enum `match`, untyped parameters) reported as warnings, a `let` that reuses a name already declared in the function, and `== true` / `== false`. Same output format and exit status as `check`. See [Linting](#linting). |
| `komodo explain <code>` | The long explanation of an error code such as `K0001`, with a wrong example and its fix. See [Diagnostics](diagnostics.md). |
| `komodo repl` | The interactive prompt, see [REPL](#repl). |
| `komodo new <name>` | Create `<name>/` with `main.km`, `tests/smoke_test.km` and a `.gitignore`. |
| `komodo bind ...` | Generate Komodo `extern` bindings from a C header (`tools/km-bind`). Arguments go to that tool. |
| `komodo watch [file]` | Run, then rebuild and rerun whenever a `.km`, `.kmm` or native source under the project root changes (polled every 0.4 s). Ctrl-C stops. |
| `komodo lib [file]` | Build a library for C, C++, Rust or Python callers: `<name>.o`, `lib<name>.a`, `lib<name>.so` and the header `<name>.h` generated from the program's `export fn`s. `-o <path>` sets the output prefix. |
| `komodo clean [dir]` | Delete `.komodo/` and the files `komodo build` / `lib` recorded. `--cache` also empties the shared cache. |
| `komodo doctor` | Check for `nasm`, `gcc`, libgc, and the optional `g++`, `pkg-config`, `emcc`, `node`, `km-bind`; prints a fix hint for anything missing. |
| `komodo self build\|test\|seed` | For compiler contributors, see [Working on the compiler](#working-on-the-compiler). |
| `komodo completions bash\|zsh` | Print a shell completion script for commands, options and file names, see [Shell completion](#shell-completion). |

A mistyped command gets a suggestion (`komodo biuld` -> `did you mean 'build'?`).

## Options

| Option | Meaning |
|---|---|
| `--release` | Optimise: the highest optimizer level (see [Optimization](#optimization)). Slower to build, faster to run. |
| `-t, --target <x86\|c\|wasm\|stackvm>` | Code-generation target, see [Targets](#targets). `wasm` builds with `emcc` and runs with `node`. |
| `-o <path>` | Where `build` writes its executable, or the output prefix for `lib`. The directory must exist. |
| `-q, --quiet` | Only the program's own output (plus diagnostics and errors). |
| `-v, --verbose` | Show the steps and every `nasm` / `gcc` command. |
| `--errors=short\|rich\|auto` | How diagnostics are printed: `short` is the one-line `file:line:col: error: …`, `rich` adds a source snippet, a code and an explanation, `auto` (the default) is `rich` on a terminal and `short` otherwise. `--explain` means `--errors=rich`; `KM_ERRORS` sets the default. See [Diagnostics](diagnostics.md). |
| `-h, --help`, `-V, --version` | Help and version. |

Errors are one line with a hint, for example
`komodo: nasm not found — install it (Debian/Ubuntu: sudo apt install nasm ...)`.
Run `komodo doctor` when something is missing.

## Native code and libraries

### Using libraries

Import the name; that is all.

```km
import zlib

void main()
    console.print(zlib::zlibVersion())
```

For an `import NAME` that is neither a module of your project (a `NAME.km` or
`NAME.kmm` below the entry file's directory) nor a standard-library module,
`komodo` asks `tools/km-bind --auto NAME` to find the C library (via
pkg-config, then the system headers) and write a binding module to
`komodo_bindings/NAME.kmm` next to your entry file. The binding carries the
link line (`pkg:NAME` or `-lNAME`). Existing bindings are reused, so repeated
runs are instant; delete the directory to regenerate. `komodo_bindings/` is
generated, so `komodo new` ignores it in git. (It cannot live inside
`.komodo/`: the compiler's module search skips directories whose name begins
with a dot.) If no library is found, the build stops at the import line with
`main.km:2:8: error: cannot find module 'NAME'`, followed by a second line,
`komodo: no C library 'NAME' found (try: sudo apt install libNAME-dev, or write
a binding with komodo bind)`. The same happens for any import that is neither
a project module nor a library. For a header you want to bind yourself, run
`komodo bind`.

### native/

If a `native/` directory sits next to the entry file (or at the project root),
every `.c`, `.cpp`, `.cc`, `.cxx`, `.o` and `.a` directly inside it is built
and linked automatically. Sources are compiled once and cached by content
(`.cpp` with `g++`, which adds `-lstdc++`); `native/` is also an include path,
and editing a header there rebuilds what includes it.

<!-- doc-skip -->
```km
// main.km                      native/hello.c: long hello_add(long a, long b) {...}
extern fn hello_add(a: int, b: int): int
```

### Linking names

`link` directives and `extern "lib"` headers (see [ffi.md](ffi.md)) accept:

- a bare name, `-lname`, or flags: passed to the linker. If `pkg-config` has a
  package of that name, its flags are used instead (so private dependencies and
  `-I` come along);
- `pkg:name` (for example `extern "pkg:sdl2"` or `link "pkg:sdl2"`): expands to
  `pkg-config --cflags --libs name`; an unknown package is an error with an
  install hint;
- a path to a `.c`, `.cpp`, `.cc`, `.cxx`, `.o` or `.a`.

### Shared libraries

`komodo lib` writes `lib<name>.so` next to `lib<name>.a`. The object it is built
from is position-independent (the `x86pic` target: calls through the PLT, C
globals through the GOT), and the runtime is compiled `-fPIC` for the shared
object. The default `komodo` build and `-t x86` output are unchanged. Use
`-Wl,-rpath,<dir>` when a program loads the `.so` from a directory that is not on
the loader path. Examples: [interop.md](interop.md#part-2-using-komodo-from-c-c-rust-and-python).

## Working on the compiler

```bash
komodo self build     # ./build-self.sh: build ./kmc (no-op when up to date)
komodo self test      # ./run-tests.sh, then ./run-sema-tests.sh
komodo self seed      # ./build-self.sh --force --update-seed
```

`self build` accepts `build-self.sh`'s own options (`--force`, `--fast`).
Ordinary use never needs it: `komodo` builds the compiler on demand.

## Legacy flags

These keep working (the test scripts use them) but are hidden from `--help`.

| Flag | Equivalent / meaning |
|---|---|
| `-O <level>`, `-O<level>`, `--opt=<level>` | Optimization level `none`, `low`, `medium`, `max` or `0`-`3`; `--release` is `-O max`. |
| `-S`, `--emit-asm` | Stop after writing the generated code next to the source: `<file>.km.asm` for x86 and stackvm, `<file>.c` for `-t c` and `-t wasm`. |
| `-c`, `--compile` | Build but do not run; the binary goes to `-o` or the file name without `.km`. |
| `-k`, `--kmc <path>` | Use another `kmc` (as `$KMC`); disables the automatic compiler build. |
| `--strict` | Make analyser warnings fatal, and enable extra checks, see [Strict mode](#strict-mode). |
| `--no-cache` | Ignore the build cache for this run. |
| `--clean` | Delete the shared cache and, given a file, its `.asm/.o/.links/.deps`; exit. |
| `--time` | Print per-phase timings to stderr. |
| `--lib`, `--emit-header <h>` | What `komodo lib` does; `--emit-header` alone writes only the header. |
| `file.km -- <args>` | Everything after a literal `--` following the file goes to the linker: `.c` / `.o` / `.a` files, `-l`, `-L`, `-I`. `$KM_CC_INPUTS` is read the same way. Prefer `native/`. |

```bash
komodo -O max -c -o app src/main.km
komodo --strict hello.km
komodo -S hello.km                    # writes hello.km.asm
komodo -t c -S hello.km               # writes hello.c, next to the source
komodo --time -q hello.km
komodo hello.km -- util.cpp           # same as putting util.cpp in native/
```

`--time` output looks like:

```text
  front end      13 ms  cached
  assemble        5 ms  cached
  link            7 ms  cached
Hello, world!
  run             4 ms
```

## Targets

| Target | Emits | Built by | Run |
|---|---|---|---|
| `x86` (default) | NASM x86-64 assembly in `<file>.asm` | `nasm` + `gcc`, linked with `stdlib/*.c` and libgc | the native executable |
| `c` | portable C; `-S` writes it as `<file>.c` | `gcc -std=gnu11 -O1`, with `stdlib/*.c` | the native executable |
| `wasm` | the same C (`-S` writes `<file>.c`), built with `emcc` into `<out>.js` and `<out>.wasm` | `emcc` with `stdlib/*.c` | `node <out>.js` |
| `stackvm` | stack-VM bytecode text in `<file>.asm` | nothing (no runner yet) | not runnable |

```bash
komodo hello.km                            # x86, compile + run
komodo -t c hello.km                       # C, compile + run
komodo -t wasm hello.km                    # wasm: emcc, then run with node
komodo -t wasm build -o hello && node hello.js
komodo -t stackvm hello.km                 # writes hello.km.asm and stops
```

The `stackvm` output for `hello.km` begins like this:

```text
; generated by the Komodo compiler — stack-VM bytecode

FUNC main frame=16 params=0
    PUSHS "Hello, world!"
    CALL print 1
    PUSHI 0
    RET
ENDFUNC
```

The wasm target needs `emcc` and `node` on `PATH`. It rejects programs that use
the C FFI (`extern fn`, callbacks, SDL, raylib) with
`wasm backend: … not supported yet`. Most examples still build for wasm.
Details are in [the WebAssembly section](language/reference.md#223-webassembly).

## Optimization

`--release` (legacy `-O max`) and `-O <level>` select the optimizer pass, which runs on the AST after analysis.
The levels are defined in [`modules/optimize/optimize.kmm`](../modules/optimize/optimize.kmm):

| Level | What it does |
|---|---|
| `none` | Nothing. Literals and branches are kept as written. |
| `low` | Constant folding: integer, string and boolean operations on literals. The x86 backend also shares identical `.rodata` constants. |
| `medium` | `low`, plus algebraic identities (`x*1`, `x+0`, `x*0`), strength reduction (`x * 2^k` becomes a shift), removal of branches whose condition is constant, and removal of code after `return`, `throw`, `break` or `continue`. |
| `max` | `medium`, plus constant propagation for `let` bindings that are never reassigned and hold a literal, followed by another fold. |

Optimization preserves behaviour. The output is the same at every level.
Division by zero and integer overflow are never folded.

Example: this program prints `7 14` at any level. At `-O max` the generated
assembly is shorter. The file is `opt.km`.

```km
void main()
    let n = 7
    if 2 > 3 then
        console.print("never")
    console.print(n * 1 + 0, 2 + 3 * 4)
```

```bash
komodo -q opt.km        # prints: 7 14
komodo -q --release opt.km        # prints: 7 14
```

Generated assembly for this file is 72 lines at `-O none` and 42 lines at
`-O max`. Counts are from one run.

## Strict mode

`--strict` makes the analyser's warnings fatal, and adds checks that are only
enabled in strict mode.

Warnings that are printed by default, and become errors under `--strict`:
division by a constant zero, `x == x`, `x = x`, an `if` whose condition is the
literal `true` or `false`, and unreachable code.

Checks that are enabled only under `--strict`: a binding with neither a type
nor an initialiser, a local or parameter that is never used, and a `match` on
an enum that does not cover every variant. The `match` check is not applied
when the match has an `else` arm.

Example, a program with an unused local, saved as `unused.km`:

```km
void main()
    let unused = 42
    console.print("hello")
```

```bash
komodo unused.km           # runs, prints: hello
komodo --strict unused.km  # error: 'unused' is declared but never used
```

The diagnostics are reported as `file:line:col: error: …` or
`… warning: …`, against the file that contains the problem. Imported modules
are included.

## Checking

`komodo check` runs the analyser and nothing else. An editor or a CI step can
use it: the output is the compiler's own diagnostics, one per line, on stdout.

```bash
komodo check                  # the project in this directory
komodo check --strict app.km  # strict mode, one file
```

```text
app.km:5:19: error: undefined name 'cont' (did you mean 'count'?)
```

The exit status is 1 when there is an error, and 0 otherwise. Warnings are
printed but do not fail the check, unless `--strict` makes them errors. A clean
program prints nothing. An import that cannot be found is reported at its line,
as `cannot find module`, followed by the same library hint as a build.

## Linting

`komodo lint` is `komodo check` with more warnings. It reports, as warnings
that do not fail the run:

- everything `komodo check` reports;
- a local or parameter that is never used (prefix the name with `_` to keep it);
- a `match` on an enum that misses a variant and has no `else`;
- a parameter with no type;
- a `let` that reuses a name already declared in the same function (locals
  are function-scoped, so the inner `let` overwrites the outer variable);
- a comparison with `true` or `false`.

```bash
komodo lint app.km
```

```text
app.km:7:9: warning: 'x' is already declared in this function; this `let` reuses it
app.km:5:5: warning: 'y' is declared but never used
```

The exit status is 1 only when there is an error. `komodo lint --strict` makes
every warning an error.

## Caching

`komodo` keeps a content-addressed cache under `$KOMODO_CACHE`, which defaults
to `${XDG_CACHE_HOME:-~/.cache}/komodo`. It has three parts:

- `fe/`: front-end output (the `.asm`, `.links` and `.deps` files), keyed on the
  source path, the target, the optimization level, the strict flag and the `kmc`
  binary. A hit skips `kmc`.
- `obj/`: each `stdlib/*.c` file and each `native/` source, compiled once per content and toolchain.
- `bin/`: the finished executable, keyed on the assembly and the link inputs.
  A hit skips `nasm` and `gcc`.

The front-end entry is valid only while the SHA-256 of the source and of every
file it read last time is unchanged, so an edit is always noticed, even one
that keeps the file size and lands in the same second.

The cache holds only derived data, so it is safe to delete:
`komodo clean --cache` removes it. `--no-cache` bypasses it for one run.

## `kmc`: the compiler by itself

`kmc` is the compiler binary. `komodo` runs it with this argument order:

```text
kmc <file.km> [x86|stackvm|c|wasm|check] [strict] [none|low|medium|max]
```

The arguments after the file can come in any order. The defaults are `x86`,
no strict mode and `none`. `kmc` writes `<file.km>.asm`, `<file.km>.deps` and
`<file.km>.links` next to the source. It does not assemble or link. The
`check` target runs the front end and the analyser only: it prints the
diagnostics and writes nothing, which is what `komodo check` uses.

With `REPO` set to the checkout and `hello.km` in the current directory:

```bash
REPO=~/Komodo                       # your checkout
"$REPO/kmc" hello.km x86
nasm -f elf64 hello.km.asm -o hello.o
gcc -no-pie -I "$REPO/stdlib" -o hello hello.o "$REPO"/stdlib/*.c -lm -l:libgc.so.1
./hello
```

The `-l:libgc.so.1` flag needs the `libgc1` runtime. The `komodo` driver adds
the automatic `link` libraries and the cache, which this example skips.

## REPL

```bash
komodo repl            # or ./kmrepl from the checkout
komodo repl --release  # optimize each entry
komodo repl -k /path/to/kmc
```

Commands: `:help`, `:show` (prints the program the session has built so far),
`:reset`, `:undo` (drops the last accepted entry), `:quit`. Ctrl-D also quits.
A session is replayed from source on each entry. See
[getting-started.md](getting-started.md#the-repl) for an example.

## Installing

```bash
./install.sh            # symlinks ~/.local/bin/komodo to this checkout
./install.sh <dir>      # symlinks <dir>/komodo instead
```

It only creates the symlink (and refuses to overwrite a regular file at the
target path). Nothing is built until the first `komodo` run, which builds
`./kmc` on its own.

### `./build-self.sh`

`komodo` and `komodo self build` call this; you rarely need it directly.
It builds the compiler with itself. Options:

- `--force`: rebuilds even if the sources are unchanged.
- `--fast`: skips the fixpoint re-check, which saves about half a second.
- `--update-seed`: rewrites `bootstrap/main.km.asm.gz` after a successful build.
- `-h, --help`: prints the usage text.

The bootstrap compiler is chosen in this order: `$KMC`, then an existing
`./kmc`, then the seed in `bootstrap/main.km.asm.gz` (needs only `nasm` and
`gcc`), then the interpreter named by `$KOMODO_MAIN`. The interpreter path is
used only when the others are missing. After the first build, `build-self.sh`
checks that `kmc` reproduces its own assembly byte for byte. The script
rechecks a second stage if the first differs.

## Test scripts

- **`./run-tests.sh [name ...]`** compiles and runs each `examples/*.km` and `tests/examples/*.km`, diffing
  its output against the `.expected` file beside it. It also builds each
  example with `-t stackvm`, `-t c` and, when `emcc` and `node` are installed,
  `-t wasm`. A file can opt out of a target with a comment line containing
  `stackvm: skip`, `c: skip` or `wasm: skip`.

  ```bash
  ./run-tests.sh                 # every example
  ./run-tests.sh is_demo oop_demo
  ```

- **`./run-sema-tests.sh`** checks the analyser's diagnostics. Each file in
  `tests/sema/` must produce the error text in its `.experr` file. Files in
  `tests/sema/strict/` are checked in strict mode, and must compile cleanly in
  default mode.

  ```bash
  ./run-sema-tests.sh
  ```

Both scripts need `./kmc`. Run `./build-self.sh` first.

## Shell completion

```bash
source <(komodo completions bash)                 # bash, in this shell
komodo completions bash > ~/.local/share/bash-completion/completions/komodo
komodo completions zsh > "${fpath[1]}/_komodo"    # zsh; then restart the shell
```

The scripts complete the commands, the options (with the values of `-t` and
`-O`), the `self` and `completions` subcommands, and file names.

## Environment variables

| Variable | Used by | Meaning |
|---|---|---|
| `KMC` | `komodo`, `build-self.sh` | The `kmc` binary to use. |
| `KOMODO_CACHE` | `komodo` | The cache directory. Default `${XDG_CACHE_HOME:-~/.cache}/komodo`. |
| `KM_CC_INPUTS` | `komodo` | Extra linker inputs, word-split, as with `--`. |
| `KOMODO` | `kmrepl`, `run-tests.sh` | The `komodo` driver to use. |
| `KOMODO_MAIN` | `build-self.sh` | Path to the interpreter's `main.py`. Used only when no seed or `kmc` is available. |
| `KM_PY` | `build-self.sh` | The Python used for the interpreter path. |
