# C interoperability (FFI)

Komodo calls C directly. An `extern` declaration is a prototype with no body:
the call goes through the native System V AMD64 C ABI, the symbol is resolved by
the linker under its C name, and the library that provides it is linked
automatically. This document covers calling C from Komodo on the default x86-64
target. Exporting Komodo functions to C is not covered here (see
[Planned](#planned)).

## Table of contents

- [Overview](#overview)
- [Calling C](#calling-c)
  - [`extern fn` and `extern void`](#extern-fn-and-extern-void)
  - [Variadic calls](#variadic-calls)
  - [Opaque handles: `extern type`](#opaque-handles-extern-type)
  - [`extern "lib"` blocks](#extern-lib-blocks)
  - [How linking is decided](#how-linking-is-decided)
- [Type mapping](#type-mapping)
- [Strings, buffers and out-parameters](#strings-buffers-and-out-parameters)
- [Structs by value](#structs-by-value)
- [Callbacks](#callbacks)
- [Worked example: binding a C library](#worked-example-binding-a-c-library)
- [Calling the C standard library](#calling-the-c-standard-library)
- [Error-prone points](#error-prone-points)
- [Runtime ABI for runtime C files](#runtime-abi-for-runtime-c-files)
- [Target support](#target-support)
- [Planned](#planned)

## Overview

| Declaration | Meaning |
| --- | --- |
| `extern fn f(a: int): int` | A C function returning a value |
| `extern void f(a: int)` | A C function returning `void` |
| `extern type H` | An opaque, pointer-sized C handle |
| `extern "lib"` + indented block | Several declarations that belong to one library |
| `fn f(a: int): int` (no `extern`) | A Komodo-ABI function implemented in a companion C file. Not a C-ABI call; see [Runtime ABI](#runtime-abi-for-runtime-c-files) |

The snippets below were compiled and run with `komodo -q` on the x86 target;
each output block is the program's real output.

## Calling C

### `extern fn` and `extern void`

A prototype names the C symbol directly. Parameters and the return type use the
types in the [type mapping](#type-mapping).

```km
extern fn strlen(s: string): int
extern fn getenv(name: string): string?

void main()
    print(strlen("hello, world"))
    if getenv("KM_DOC_NO_SUCH_VAR") == null then
        print("KM_DOC_NO_SUCH_VAR is unset")
    if getenv("PATH") != null then
        print("PATH is set")
```

```text
12
KM_DOC_NO_SUCH_VAR is unset
PATH is set
```

`getenv` can return `NULL`, so it is declared `string?` and checked with `== null`.

Calls must pass exactly as many arguments as the prototype declares, except for
a variadic prototype (`...`). A variadic call passes its fixed arguments first and
any number of extra arguments after them; see [Variadic calls](#variadic-calls).
A fixed-arity prototype rejects extra arguments:

```km
extern fn puts(s: string): int

void main()
    puts("a", "b")
```

```text
error: 'puts' takes 1 argument(s), got 2
```

A call to a variadic prototype with no extra arguments works too, and C's stdio output is interleaved correctly with `print`
because `print` also writes through stdio:

```km
extern fn puts(s: string): int
extern fn printf(fmt: string, ...): int

void main()
    puts("one")
    printf("two\n")
```

```text
one
two
```

### Opaque handles: `extern type`

`extern type` declares a nominal, pointer-sized type with no fields. A C function
produces it; Komodo only stores it and passes it back.

- It cannot be built with `new`.
- It supports only `==` and `!=`, so `h == 0` is the NULL check.
- It is not interchangeable with `int` or with another handle type.

### `extern "lib"` blocks

A block groups several declarations under one library name. The header string is
also the link directive:

```km
extern "m"
    fn cbrt(x: float): float
    fn hypot(a: float, b: float): float

void main()
    print(cbrt(27.0))
    print(hypot(3.0, 4.0))
```

```text
3.0
5.0
```

`"m"` becomes `-lm`. A header containing `/` or `.`, or starting with `-`, is
passed to the linker unchanged, so `extern "-lm"` also works. A path such as
`"counter.c"` is resolved relative to the **current directory** where you run
`komodo`, not relative to the source file. For a companion C file, prefer
`link "file.c"` inside a module (below), which is resolved relative to the
module.

Blocks need no enclosing `module`, so a one-off program can bind a library inline.

### How linking is decided

| Where | Form | Linker input |
| --- | --- | --- |
| A module file (`module x`) | `link Name` | `-lName` |
| A module file | `link "file.c"` | the file, resolved relative to the module's own directory |
| Any file | `extern "Name"` block | `-lName` |
| Any file | `extern "path/or/flag"` block | passed as-is, resolved from the current directory |

The compiler reads every `link` and `extern "..."` reachable from the program
through its `import`s, writes them to `<file>.links`, and `komodo` passes them
to the linker. Nothing has to be listed on the command line. `komodo file.km -- ...`
adds extra inputs such as `-I`, `-L`, or a `.o` file.

## Type mapping

| Komodo | C parameter or return | Notes |
| --- | --- | --- |
| `int` | `int64_t` (`long` on LP64) | 64-bit signed. A C `int` return is not sign-extended; see [Error-prone points](#error-prone-points) |
| `float` | `double` | 64-bit IEEE-754 |
| `f32` | `float` | 32-bit; narrowed at the call boundary |
| `bool` | `_Bool` / `bool` | 0 or 1 |
| `string` | `const char *` | NUL-terminated; the returned pointer is borrowed |
| `string?` | `char *` that may be `NULL` | `== null` tests it |
| `extern type H` | any pointer | opaque; test `== 0` for NULL |
| `data(arr)` of `[int]` | `int64_t *` | array elements are 64-bit slots |
| `copy(s)` of a struct | struct by value | see [Structs by value](#structs-by-value) |
| bare function name | function pointer | see [Callbacks](#callbacks) |

## Strings, buffers and out-parameters

A `string` argument is passed as a `char *` to a NUL-terminated copy of the
text. A `string` return is a pointer to memory owned by C. Komodo does not
free it, and the collector does not manage memory allocated by libc `malloc`.
If C allocates the string, expose a C function that frees it.

To pass a buffer or receive results in an out-parameter, use `data(arr)`, which
gives the address of the array's elements. Komodo arrays store every element in
a 64-bit slot, so the C side must use 64-bit element types:

<!-- doc-skip -->
```km
import counter

void main()
    let squares = [0, 0, 0, 0]
    counter::counter_squares(data(squares), #squares)
    print(squares)
    print(`sum of squares: {counter::counter_sum(data(squares), #squares)}`)
```

```text
[1, 4, 9, 16]
sum of squares: 30
```

(`counter_squares` is declared in the binding module shown in the
[worked example](#worked-example-binding-a-c-library).)

## Structs by value

An `extern` argument written `copy(s)` is passed **by value**. Komodo copies the
object's fields into the tight C layout their declared types imply (`f32` is 4
bytes, `u8` 1 byte, `i32` 4 bytes, `float` and `int` 8 bytes) and places them
according to the System V AMD64 rules:

- Up to 8 bytes: one eightbyte. Up to 16 bytes: two eightbytes.
- An eightbyte is passed in an SSE register (`xmm`) when all of its fields are
  floating-point, otherwise in a general-purpose register.
- Larger than 16 bytes (up to 192): passed in memory on the stack.
- A struct returned by an `extern fn` (up to 16 bytes) is unpacked from
  `rax:rdx` or `xmm0:xmm1` into a fresh Komodo object.
- Nested structs and arrays inside a by-value struct are not supported (the
  compiler reports the field).

The mirror struct must match the C struct field for field. Here is a complete
example with a C `struct { double x; double y; }`:

```c
/* geom.c */
typedef struct Vec2 {
    double x;
    double y;
} Vec2;

Vec2 vec2_scale(Vec2 v, double k)
{
    Vec2 r = { v.x * k, v.y * k };
    return r;
}

double vec2_dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}
```

<!-- doc-skip -->
```km
// geom.kmm
module geom

link "geom.c"

struct Vec2
    x: float = 0.0
    y: float = 0.0

extern fn vec2_scale(v: Vec2, k: float): Vec2
extern fn vec2_dot(a: Vec2, b: Vec2): float
```

<!-- doc-skip -->
```km
import geom

void main()
    let v = geom::vec2_scale(copy(new geom::Vec2(1.5, -2.0)), 2.0)
    print(`scaled: {v.x}, {v.y}`)
```

```text
scaled: 3.0, -4.0
```

Without `copy`, the struct is passed as a pointer, and C reads garbage:

<!-- doc-skip -->
```km
import geom

void main()
    let v = geom::vec2_scale(new geom::Vec2(1.5, -2.0), 2.0)
    print(`scaled: {v.x}, {v.y}`)
```

```text
scaled: 0.0, 0.0
```

`copy` also leaves the source usable, so `a` is still a live object after
`copy(a)`. The repository's `examples/ffi_struct.km` and `stdlib/raylib.kmm`
use the same mechanism.

## Callbacks

Pass a bare Komodo function name as an `extern` argument and C receives a
function pointer. The compiler emits a System V trampoline that converts the C
integer-register arguments into the Komodo call convention.

<!-- doc-skip -->
```km
import counter

fn double_it(x: int): int
    return x * 2

void main()
    print(counter::counter_apply_twice(double_it, 5))
```

```text
20
```

Limits, enforced by the compiler:

- Up to 6 parameters.
- Every parameter and the return type must be a plain machine word: `int`,
  `bool`, `string`, or a handle. `float` is rejected.
- The return type is one word or `void`.

```text
error: 'scale' cannot be a C callback: parameter type 'float' is not a plain machine word
error: 'seven' cannot be a C callback: it takes 7 parameters (max 6)
```

The repository's `examples/ffi_callback.km` registers a Komodo function with
C's `atexit`, so it runs after `main` returns.

## Worked example: binding a C library

This walks through binding a small C library, `counter`, with three files in one
directory. It uses only features shown above.

**Step 1. Write the C library.** `counter.c`:

```c
/* counter.c — a tiny C library used as the worked FFI example. */
#include <stdint.h>
#include <stdlib.h>

typedef struct Counter {
    int64_t value;
    int64_t step;
} Counter;

Counter *counter_new(int64_t step)
{
    Counter *c = malloc(sizeof *c);
    if (c) {
        c->value = 0;
        c->step = step;
    }
    return c;                       /* NULL when malloc fails */
}

void counter_tick(Counter *c)
{
    c->value += c->step;
}

int64_t counter_value(const Counter *c)
{
    return c->value;
}

void counter_free(Counter *c)
{
    free(c);
}

/* Sums n 64-bit words: the buffer comes from a Komodo array via data(). */
int64_t counter_sum(const int64_t *xs, int64_t n)
{
    int64_t s = 0;
    for (int64_t i = 0; i < n; i++)
        s += xs[i];
    return s;
}

/* Applies f twice: a callback target from Komodo. */
int64_t counter_apply_twice(int64_t (*f)(int64_t), int64_t x)
{
    return f(f(x));
}

/* Out-parameter: writes n squares into a caller-owned buffer. */
void counter_squares(int64_t *out, int64_t n)
{
    for (int64_t i = 0; i < n; i++)
        out[i] = (i + 1) * (i + 1);
}

/* A C bool return. */
_Bool counter_is_even(int64_t x)
{
    return x % 2 == 0;
}
```

**Step 2. Write the binding module.** `counter.kmm`. The `link` line makes
`komodo` compile and link `counter.c` automatically. Each prototype mirrors one C
signature: `int` becomes `int64_t`, and the `Counter` pointer becomes an opaque
handle.

<!-- doc-skip -->
```km
module counter

link "counter.c"

extern type Counter
extern fn counter_new(step: int): Counter
extern void counter_tick(c: Counter)
extern fn counter_value(c: Counter): int
extern void counter_free(c: Counter)
extern fn counter_sum(xs: int, n: int): int
extern fn counter_apply_twice(f: int, x: int): int
extern void counter_squares(out: int, n: int)
extern fn counter_is_even(x: int): bool
```

Calls from other files use the module prefix, `counter::counter_new`. The
prefix is dropped when the linker resolves the symbol.

**Step 3. Use it.** `app.km` (here `02.km`):

<!-- doc-skip -->
```km
import counter

void main()
    let c = counter::counter_new(3)
    if c == 0 then
        print("allocation failed")
        return
    counter::counter_tick(c)
    counter::counter_tick(c)
    print(counter::counter_value(c))
    counter::counter_free(c)
```

```text
6
```

Run it with `komodo app.km`. The `c == 0` check tests the handle for NULL.

## Calling the C standard library

Nothing has to be linked for the C library, because `libc` is always available:

```km
extern fn atoi(s: string): int
extern fn sqrt(x: float): float

void main()
    print(atoi("42"))
    print(sqrt(16.0))
```

```text
42
4.0
```

`math` functions in `libm` can be declared with `extern "m"` (see
[`extern "lib"` blocks](#extern-lib-blocks)). `puts` and `strlen` also work. Use
`float` (double) for C `double` parameters, and `f32` for C `float` parameters.
The [error-prone points](#error-prone-points) cover the common mistakes.

## Error-prone points

**1. A C `int` return is not sign-extended.** A C function that returns
`int32_t` leaves the upper 32 bits of `rax` unspecified, and Komodo reads all 64.
A negative result shows up as a large positive number:

```c
/* pitfalls.c */
#include <stdint.h>

int32_t c_neg32(int32_t x) { return -x; }
uint8_t c_low_byte(int32_t x) { return (uint8_t)x; }
```

<!-- doc-skip -->
```km
// pitfalls.kmm
module pitfalls

link "pitfalls.c"

extern fn c_neg32(x: int): int
extern fn c_low_byte(x: int): int
```

<!-- doc-skip -->
```km
import pitfalls

void main()
    print(pitfalls::c_neg32(5))
    print(pitfalls::c_neg32(-5))
```

```text
4294967291
5
```

`c_neg32(5)` should be `-5`. The fix is on the C side: return `int64_t`.
The same applies to narrow returns: `c_low_byte(-1)` returns `4294967295`
instead of `255`, because the upper bits of `rax` are whatever the register held.

**2. A C `float` is not a Komodo `float`.** `float` is a 64-bit `double`. A C
`float` parameter reads the low 32 bits of the wrong bit pattern. Use `f32` for
C `float`:

```km
extern fn sqrtf(x: float): float
extern fn sqrtf32(x: f32): f32

void main()
    print(sqrtf(16.0))
    print(sqrtf32(16.0))
```

```text
16.0
4.0
```

The first line is wrong (the square root of 16 is 4). The `f32` version is correct.

**3. Buffers need 64-bit C element types.** `data(arr)` points at 64-bit
slots. A C `int32_t *` parameter reads the slots as if they were packed 32-bit
values:

```c
/* i32buf.c */
#include <stdint.h>
int64_t sum_i32(const int32_t *xs, int64_t n)
{
    int64_t s = 0;
    for (int64_t i = 0; i < n; i++)
        s += xs[i];
    return s;
}
```

<!-- doc-skip -->
```km
// i32buf.kmm
module i32buf

link "i32buf.c"

extern fn sum_i32(xs: int, n: int): int
```

<!-- doc-skip -->
```km
import i32buf

void main()
    let xs = [1, 2, 3]
    print(i32buf::sum_i32(data(xs), #xs))
```

```text
3
```

The sum of `[1, 2, 3]` is 6. The C side read `1, 0, 2` from the 64-bit slots.
Declare the C parameter `int64_t *`.

**4. Missing `copy()` on a struct argument** passes a pointer, not the value.
See [Structs by value](#structs-by-value): the output is `0.0, 0.0`.

**5. Handle misuse is a compile error.** Passing an `int` where a handle is
expected, doing arithmetic on a handle, or constructing one with `new` are all
rejected:

```text
error: argument 1 to extern 'counter_tick': type 'int' is not assignable to handle type 'Counter'
error: operator '+' is not defined for the opaque handle type 'Counter'
error: cannot construct `extern type Counter` — it is an opaque C handle; obtain one from a C function
```

**6. Block paths resolve against the module.** A quoted path in an
`extern "counter.c"` block is resolved against the directory of the file that
names it, the same as `link "counter.c"`, so the program builds from any working
directory. When no file exists there, the path is used as given, relative to the working
directory.

**7. Variadic calls** are supported; see [Variadic calls](#variadic-calls).

## Variadic calls

A prototype ending in `...` takes its fixed parameters, then any number of
extra arguments:

```km
extern fn printf(fmt: string, ...)

void main()
    printf("%d %s %.2f\n", 7, "hi", 3.5)
```

The extra arguments follow the System V rules: integers, pointers and strings go
in integer registers and then the stack, `float` values in `xmm` registers and
then the stack. Komodo's `float` is a double, which is what C's default argument
promotion produces. Nothing checks the extra arguments against the format, so
they must match it. Fewer arguments than the fixed parameters is a compile error.
Full example: `examples/ffi_variadic.km`.

## Runtime ABI for runtime C files

A companion C file linked with `link "file.c"` can implement Komodo functions
(bodyless `fn` prototypes with no `extern`). These use Komodo's own call
convention, not the C ABI. The header `stdlib/km_runtime.h` defines it. The
companion file below includes it with `#include "km_runtime.h"` and builds with
no extra flags.

- Every function receives `(int64_t argc, KmArg *items)`. Read the arguments
  by index from `items[i].value`.
- `KmArg` is `{ int64_t tag; int64_t value; }`. `value` holds an `int`, a float's
  bit pattern (`km_as_float` / `km_from_float`), a `char *` string, or a `KmArray *`.
- `KmArray` is `{ int64_t len; int64_t cap; int64_t data[]; }`. Use `KM_ARR(v)` to
  get the `KmArray *` from a value and `KM_STR(v)` for a `const char *`.
- Returns use `int64_t` (or `double` for a float, `const char *` for a string).
- Allocate memory the collector can see with `km_alloc`, `km_alloc_atomic`, or
  `km_strndup`, not libc `malloc`.

```c
/* rtdemo.c */
#include "km_runtime.h"

/* fn rt_sum(xs: [int]): int */
int64_t rt_sum(int64_t argc, KmArg *items)
{
    KmArray *a = KM_ARR(items[0].value);
    int64_t s = 0;
    for (int64_t i = 0; i < a->len; i++)
        s += a->data[i];
    return s;
}

/* fn rt_greet(name: string): string */
const char *rt_greet(int64_t argc, KmArg *items)
{
    const char *name = KM_STR(items[0].value);
    return km_str_concat("hello, ", name);
}
```

<!-- doc-skip -->
```km
// rtdemo.kmm
module rtdemo

link "rtdemo.c"

fn rt_sum(xs: [int]): int
fn rt_greet(name: string): string
```

<!-- doc-skip -->
```km
import rtdemo

void main()
    print(rtdemo::rt_sum([1, 2, 3, 4]))
    print(rtdemo::rt_greet("komodo"))
```

```text
10
hello, komodo
```

The standard library (`stdlib/math.kmm`, `stdlib/console.kmm`, and others) is
written this way. Its `.kmm` files declare Komodo-ABI prototypes, and the
matching `.c` files implement them in `stdlib/*.c`.

## Target support

| Target | `extern fn` / `extern void` / `extern type` | Callbacks | `copy(struct)` by value |
| --- | --- | --- | --- |
| x86-64 (default, `komodo file.km`) | yes | yes | yes |
| `-t stackvm` | compile only (`-S`); calls emit `CCALL`, no runner | compiles (`-S`) | rejected |
| `-t c` | rejected: `extern fn (C FFI call) not supported yet` | rejected | rejected |
| `-t wasm` | rejected: `wasm backend: extern fn (C FFI call) not supported yet` | rejected | rejected |

The `ffi_*` examples carry `c: skip` and `wasm: skip` markers (`ffi_struct` also
has `stackvm: skip`). Komodo-ABI functions from a module's companion C file also
fail on `-t c` and `-t wasm` with an undeclared-identifier error. Use the x86
target for FFI.

## Planned

Nested structs and arrays inside by-value structs are not implemented; they
need a recursive marshalling pass in codegen and the backend.
