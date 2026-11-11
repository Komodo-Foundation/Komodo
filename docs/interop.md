# Using C and C++ libraries from Komodo, and Komodo from other languages

Komodo talks to native code in both directions. This guide shows the quickest
route each way. The details of the C ABI live in [ffi.md](ffi.md) and
[the language reference](language/reference.md); this page is the friendly front door.

Everything below was run on the default x86-64 target (`komodo file.km`).

## Part 1. Using C and C++ libraries from Komodo

### `import zlib`: no setup

If a C library is installed with its development headers, you can import it by
name:

```km
import zlib

void main()
    print(zlib::crc32(0, "hello", 5))
    print(zlib::adler32(1, "hello", 5))
```

```text
907060870
103547413
```

Behind the scenes `komodo` runs `tools/km-bind --auto zlib`, which writes a
binding module `zlib.kmm` (kept under `.komodo/bindings/`) and links the library
through `pkg-config`. Run it yourself to see or keep the result:

```bash
tools/km-bind --auto zlib -o zlib.kmm     # then: import zlib
```

`--auto <name>` resolves a library by name only:

1. If `pkg-config --exists <name>` succeeds, its `--cflags` are used for the
   header search and the module links with `pkg:<name>` (which `komodo` expands
   to `pkg-config --cflags --libs <name>`).
2. The header is looked up in an alias table for common libraries
   (`sdl2` is `SDL2/SDL.h`, `zlib` and `z` are `zlib.h`, `m` and `math` are
   `math.h`, `curl` is `curl/curl.h`, `sqlite3`, `png`, `ssl`/`openssl`,
   `readline`, `gl`, `ncurses`, `pthread`, and libc headers such as `string`,
   `ctype`, `stdlib`, `stdio` and `time` by their own name). Otherwise it tries
   `<name>.h`, `<name>/<name>.h` and `lib<name>.h` under the pkg-config include
   directory, the compiler's system include directories, `/usr/include`,
   `/usr/include/x86_64-linux-gnu` and `/usr/local/include`.
3. The module is named after the import name (made into a valid identifier).

If nothing is found, the tool prints one line and exits with status 3:

```text
km-bind: no header for 'foo': install libfoo-dev or write a binding
```

`-v` prints the resolved header and library on stderr. The output is
deterministic, so it can be cached. A libc header such as `string` or `math`
needs no link line. Do not use a name that clashes with a standard-library
module (`math`, `string`, ...) for a generated module; use `-m c_math`
(see below).

### `km-bind`: a binding from any header

```bash
tools/km-bind <header.h> [-l lib] [-p pkg-config-name] [-I dir]... [-D macro]...
                         [-m module] [-o out.kmm] [--prefix str] [--also substr]
```

| Option | Meaning |
| --- | --- |
| `-l <lib>` | Emit `link <lib>` (`-l<lib>`). A path such as `foo.c` becomes `link "foo.c"`, a companion source compiled next to the module. |
| `-p <pkg>` | Emit `link "pkg:<pkg>"` and use `pkg-config --cflags <pkg>` for preprocessing. |
| `-I`, `-D`, `--cflag` | Passed to the C preprocessor. |
| `-m <module>` | Module name. Default: the `-o` file stem, else the header stem (`c_` is prepended if it would clash with a standard-library module). |
| `-o <file>` | Output file. Default: stdout. |
| `--prefix <str>` | Bind only functions, constants and types whose name starts with `<str>` (for example `SDL_`). |
| `--also <substr>` | Also bind declarations from included files whose path contains `<substr>`. |
| `-v` | Print the resolved header and library on stderr. |

It needs only `bash`, `awk` and `gcc` (`gcc -E`). The header is preprocessed
with `gcc -E -dD`, and only declarations that come from the header itself are
bound, not everything it `#include`s. For a header that lives outside the
system include directories (your own project, or `SDL2/SDL.h`), the files beside
it count as part of it. A system header like `math.h` also pulls in its own
`bits/mathcalls.h`.

A complete example, using the test header `tests/bind/shapes.h` and its C file:

```bash
tools/km-bind tests/bind/shapes.h -l shapes.c -m shapes -o shapes.kmm
```

<!-- doc-skip -->
```km
import shapes

fn visit(v: int, label: string): int
    return v * 10

void main()
    let c = shapes::canvas_new(3, 4)
    print(shapes::canvas_area(c))
    print(shapes::canvas_name(c))
    shapes::canvas_free(c)

    let p = shapes::point_scale(copy(new shapes::Point(1.5, -2.0)), 2.0)
    print(`{p.x} {p.y}`)

    let xs = [0, 0, 0, 0]
    shapes::fill_squares(data(xs), #xs)
    print(xs)
    print(shapes::each(visit, 3))
    print(shapes::SH_FLAG_ALL)
```

```text
12
rect
3.0 -4.0
[1, 4, 9, 16]
60
9
```

The generated file begins with a comment documenting the exact command used and
the conventions, then `module shapes`, the link line, handle types, mirrored
structs, constants, and the functions. A `// skipped: ...` comment replaces
anything that could not be bound.

### What gets bound

| C | Komodo |
| --- | --- |
| `int`, `long`, `size_t`, `uint8_t` ... `int64_t`, `char`, enums | `int` (64-bit; see the width note below) |
| `double` | `float` |
| `float` | `f32` |
| `bool` / `_Bool` | `bool` |
| `const char *` parameter | `string` |
| `char *` returned | `string?` (test with `== null`) |
| `void` return | `extern void` |
| `struct Foo *`, `typedef struct Foo Foo; Foo *`, `typedef struct Foo *FooRef` | an opaque handle `extern type Foo` (or `FooRef`); NULL is `h == 0` |
| `void *` returned | `<module>_Ptr` handle |
| `void *` parameter | `int`, so any handle or `data(arr)` fits |
| `int *`, `double *`, ... (out-parameters, arrays) | `int`, pass `data(arr)`; the array has 64-bit slots |
| struct of scalar fields passed or returned by value | a Komodo `struct` with matching field types (`f32`, `i32`, `u8`, ...); pass `copy(x)` |
| function-pointer parameter or typedef | `int` plus a comment with the Komodo signature; pass a bare function name |
| `enum` constants, `#define N <int/float/string literal>`, simple integer expressions such as `(1 << 3)` or `(A \| B)` | `let NAME be value` |
| `extern int counter;` | `extern let counter: i32` |

A name used by two modules must be unique across the whole program, so a
struct whose definition is not in the bound header (for example `FILE`) is
treated like `void *` instead of becoming its own handle type.

A function parameter or constant named like a Komodo keyword is renamed with a
trailing `_`; the C symbol is kept with `= "symbol"`.

### What is skipped

Each of these leaves a `// skipped: name (reason)` line in the output.

- variadic functions are bound by their fixed prefix with `...` kept, so `printf`
  becomes `printf(format: string, ...)`; a function with no fixed parameter is skipped;
- `long double`, `__int128`, `_Complex`, `va_list`, and `_Float128`;
- unions and nested structs, arrays, bit-fields or pointers inside a struct that
  would be passed by value, and structs over 192 bytes;
- `static` or `inline` functions with bodies;
- function-like macros, and object-like macros that are not constants;
- functions that return a function pointer.

### Limits

| Topic | Behaviour |
| --- | --- |
| Variadic C functions | Callable with any number of extra arguments after the fixed ones. Integers, pointers and strings go in integer registers, `float` values in SSE registers (C's default promotion applies). Extra arguments are not type-checked, so the format string must match them. |
| Struct arguments | By value only with `copy(x)`; scalar fields only; up to 192 bytes. |
| Struct returns | Scalar-field structs, including over 16 bytes. The result is a fresh Komodo object. |
| `int` returns narrower than 64 bits | The upper register bits are not defined, so a negative C `int` result can read as `2^32 + n`. The binding carries a note on such functions. |
| Buffers | `data(arr)` points at 64-bit slots; a C `int32_t *` or `char *` buffer does not line up. Use `const char *` as `string` for read-only text. |
| Writable `char *` parameters | Bound as `string`, which is read-only text; C must not write into it. |
| Callbacks | A bare Komodo function name. Parameters are words (`int`, `bool`, `string`, handles), `float` or `f32`; the result is one of those or `void`. No struct or array callbacks. |
| C++ | Bind an `extern "C"` wrapper, or name mangled symbols with `= "symbol"`. See `tests/examples/ffi_cxx.km`. |
| Targets | FFI works on the x86 target only. `-t c`, `-t wasm` and `-t stackvm` reject or only compile it. |

## Part 2. Using Komodo from C, C++, Rust and Python

A function marked `export fn` gets an unmangled C symbol:

<!-- doc-skip -->
```km
export fn add(a: int, b: int): int
    return a + b

export fn greet(name: string): string
    return "hello, " + name
```

`komodo lib mathx.km` writes the object `mathx.o`, the static archive
`libmathx.a`, the shared object `libmathx.so` and a header `mathx.h`:

```text
built mathx.o, ./libmathx.a, ./libmathx.so and mathx.h
link with: gcc -no-pie main.c ./libmathx.a -l:libgc.so.1 -lm
or with the shared library: gcc main.c -L. -lmathx -Wl,-rpath,. -l:libgc.so.1 -lm
```

```c
#include <stdio.h>
#include "mathx.h"
int main(void) {
    printf("%ld %s\n", (long)add(2, 3), greet("C"));
    return 0;
}
```

```bash
gcc -no-pie main.c ./libmathx.a -l:libgc.so.1 -lm -o app && ./app
```

```text
5 hello, C
```

C++ is the same with `g++`; the header already has the `extern "C"` guard. A
returned `string` points into the Komodo heap: copy it before calling back into
Komodo.

`-lmathx` alone picks the shared object, which is the one to use when the
program has to load it at run time:

```bash
gcc main.c -L. -lmathx -Wl,-rpath,. -l:libgc.so.1 -lm -o app && ./app
```

The shared object is built from the same code, compiled position-independent:
`komodo lib` selects the `x86pic` target for it, and the default output is
unchanged. Calls go through the PLT and C globals through the GOT, which is what
lets the `.so` be loaded at any address. The snippets below were run against
`mathx.km` above.

**C, with `dlopen`** (no link step; build with `gcc dl.c -ldl`):

```c
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    void *h = dlopen("./libmathx.so", RTLD_NOW);
    if (!h) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    int64_t (*add)(int64_t, int64_t) = (int64_t (*)(int64_t, int64_t))dlsym(h, "add");
    printf("%ld\n", (long)add(2, 3));
    dlclose(h);
    return 0;
}
```

```text
5
```

**Python, with `ctypes`** (run it in the directory that holds `libmathx.so`):

```python
import ctypes

lib = ctypes.CDLL("./libmathx.so")
lib.add.argtypes = [ctypes.c_int64, ctypes.c_int64]
lib.add.restype = ctypes.c_int64
lib.greet.argtypes = [ctypes.c_char_p]
lib.greet.restype = ctypes.c_char_p
print(lib.add(2, 3), lib.greet(b"Python").decode())
```

```text
5 hello, Python
```

The Komodo runtime initialises on the first call, so no setup call is needed.
A string result points into the Komodo heap: copy it (`ctypes` does, for
`c_char_p`) before calling back into Komodo.

- **Rust**: declare the functions in an `extern "C"` block and link the `.so` or
  the archive as for C, with `-l:libgc.so.1` and `-lm`. Not run here, because
  `rustc` is not installed on the machine where this guide was written.

## Where to look next

- [ffi.md](ffi.md): the type mapping, callbacks and struct rules in full.
- `tests/bind/run.sh`: runs `km-bind` on the test header, `math.h`, `string.h`,
  `ctype.h` and `zlib.h`, compiles programs against the output and compares them
  to `.expected`.
- `tests/interop/run.sh`: the `export fn` and `komodo lib` tests.
