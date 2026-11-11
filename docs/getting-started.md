# Getting started with Komodo

Komodo is a statically typed, garbage-collected language with a self-hosting
compiler. The compiler, `kmc`, is written in Komodo and compiles programs to
native x86-64 code (through NASM and gcc) or to portable C and WebAssembly. The
`komodo` command wraps it: one command compiles, links and runs a program.

For the full language reference see [`docs/language/reference.md`](language/reference.md). For the
project overview see the [README](../README.md).

## Requirements

- **`nasm`** and **`gcc`**: always needed. The x86-64 target assembles with
  NASM and links with gcc.
- **Boehm GC runtime** (`libgc.so.1`): every program links against it. On
  Debian and Ubuntu it comes from the `libgc1` package.
- **`emcc` and `node`**: optional, only for `komodo -t wasm`. Emscripten 3.1.6
  is the tested version.

You do **not** need Python or the Komodo interpreter. A checked-in seed,
`bootstrap/main.km.asm.gz`, is a compiled copy of the compiler's assembly.
The build uses it to produce the first `kmc`, then checks that the new `kmc`
rebuilds itself to identical output (the fixpoint check).

On Debian or Ubuntu:

```bash
sudo apt install nasm gcc libgc1
```

## Build and install

From the repository root:

```bash
./install.sh    # symlinks komodo into ~/.local/bin
komodo doctor   # checks nasm, gcc, libgc (and the optional tools)
```

That is the whole install. There is no compiler build step: the first
`komodo` run builds `./kmc` itself (about 2 seconds) and rebuilds it whenever
its sources change. If `~/.local/bin` is not on your `PATH`,
`install.sh` prints the line to add to your shell's rc file. Pass a different
directory as an argument to `install.sh` to install elsewhere. The installed
`komodo` is a symlink, so it always uses this checkout's `kmc` and `stdlib/`.

For compiler work (`komodo self build|test|seed`, `./build-self.sh --force`)
see [docs/cli.md](cli.md#working-on-the-compiler).

## Hello, world

Save this as `hello.km`:

```km
void main()
    print("Hello, world!")
```

Run it:

```bash
komodo hello.km
```

```text
Hello, world!
```

`komodo hello.km` compiles, links and runs the program; build products go to
a `.komodo/` directory beside it (git-ignored automatically). To build an
executable instead of running:

```bash
komodo build hello.km     # writes ./hello
./hello
```

Better still, make a project and let `komodo` find the entry file:

```bash
komodo new demo && cd demo
komodo                    # builds (incrementally) and runs main.km
komodo test               # runs tests/*_test.km
komodo watch              # rerun on every save
```

`print` is a built-in and needs no import. Other library functions come from
modules brought in with `import`, for example `import math`.

## Running a program

- `komodo` (no arguments) runs the project in this directory.
- `komodo file.km [args...]` compiles, links and runs; the args go to your
  program.
- `komodo build [-o name]` compiles and links only.
- `komodo --release` optimises.
- `komodo -t c` and `komodo -t wasm` build through the C and WebAssembly
  targets.
- C code in a `native/` directory is compiled and linked automatically;
  `import zlib` finds a system library by itself (see
  [Using libraries](cli.md#using-libraries)).

The full flag list, with examples, is in [docs/cli.md](cli.md).

## The REPL

`komodo repl` starts an interactive prompt:

```text
Komodo REPL — :help for commands, Ctrl-D to quit
>>> let n = 6
>>> 1 + 2
3
>>> :quit
```

A bare expression is evaluated and its value printed. A declaration (`fn`,
`struct`, `class`, `enum`, `import`, ...) or a statement is kept for the rest
of the session. A line that opens a block continues on `...` lines until you
enter a blank line. The commands are `:help`, `:show` (print the program built
so far), `:reset`, `:undo` and `:quit`. Each entry recompiles the session, so
the REPL is best for quick experiments. Use files for anything larger.

## A five-minute tour

Each snippet below runs as written. The output shown is what it prints.

### Variables, arithmetic and interpolation

```km
void main()
    let name = "Komodo"
    let answer = 6 * 7
    print(`{name}: the answer is {answer}`)
    let ratio = 1 / 4
    print("ratio:", ratio, "half:", 0.5 * 3.0)
```

```text
Komodo: the answer is 42
ratio: 0 half: 1.5
```

`let x = expr` creates a reassignable variable. Integer division truncates, so
`1 / 4` is `0`. Use a float literal (`1.0 / 4`) for a fractional result.
Backtick strings interpolate `{expr}`.

### Functions and loops

```km
fn square(x: int) : int
    return x * x

void main()
    let total = 0
    for i = 1 to 5
        total += square(i)
    print("sum of squares:", total)
```

```text
sum of squares: 55
```

A `fn` returns a value and a `void` function returns nothing. `for i = 1 to 5`
includes both ends. `total += ...` is a compound assignment.

### Arrays

```km
void main()
    let xs = [4, 15, 8, 23, 16, 42]
    let big = 0
    for x in xs
        if x > 10 then
            big += 1
    print("values above 10:", big)
    print("first:", xs[0], "last:", xs[#xs - 1])
```

```text
values above 10: 4
first: 4 last: 42
```

Indexes start at `0`. `#xs` is the length of the array.

### Structs and errors

```km
struct Point
    x: int
    y: int

fn check_port(n: int) : int
    if n < 1 or n > 65535 then
        throw "port out of range"
    return n

void main()
    let p = new Point(3, 4)
    print("distance squared:", p.x * p.x + p.y * p.y)
    let port = check_port(70000)?
    if port == null then
        print("bad port, falling back to 8080")
```

```text
distance squared: 25
bad port, falling back to 8080
```

`throw` raises an error. A call written `f(args)?` does not propagate the
error. Instead it evaluates to `null`, which you test with `== null`. Without
the `?`, an uncaught error prints `Uncaught error: <message>` and the program
exits with status 1.

## Where to go next

- [Language basics](language/basics.md): syntax, functions, control flow.
- [Types](language/types.md): the type system, nullability and classes.
- [Foreign functions (FFI)](ffi.md): calling C and linking libraries.
- [CLI reference](cli.md): every flag, target and build command.
- [Internals](internals.md): how the compiler is organised.
- [`docs/language/reference.md`](language/reference.md): the complete language reference.
